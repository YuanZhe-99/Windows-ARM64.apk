#define _GNU_SOURCE
#include <aaudio/AAudio.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <android/choreographer.h>
#include <android/looper.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <link.h>
#include <linux/futex.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <setjmp.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

/* Exported STT_OBJECT global resolved via dlsym(RTLD_DEFAULT, ...). */
__attribute__((visibility("default"))) uint64_t g_n2_exported_object = 0x4E324F4B55AA1234ULL;

struct LooperCheckState {
    int timer_fired;
    int64_t choreo_ns;
};

static int on_timer_fd(int fd, int events, void* data) {
    (void)events;
    uint64_t exp = 0;
    if (read(fd, &exp, sizeof(exp)) == 8 && exp >= 1) {
        ((struct LooperCheckState*)data)->timer_fired = 1;
    }
    return 0; /* unregister after first fire */
}

static void on_choreo_frame(int64_t frame_time_nanos, void* data) {
    ((struct LooperCheckState*)data)->choreo_ns = frame_time_nanos;
}

static void* looper_worker(void* arg) {
    struct LooperCheckState* st = (struct LooperCheckState*)arg;
    ALooper* looper = ALooper_prepare(0);
    if (!looper) return NULL;

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) return NULL;

    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    its.it_value.tv_nsec = 2 * 1000 * 1000; /* 2 ms relative */
    if (timerfd_settime(tfd, 0, &its, NULL) != 0) {
        close(tfd);
        return NULL;
    }
    if (ALooper_addFd(looper, tfd, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, on_timer_fd, st) != 1) {
        close(tfd);
        return NULL;
    }

    AChoreographer* ch = AChoreographer_getInstance();
    if (!ch) {
        close(tfd);
        return NULL;
    }
    AChoreographer_postFrameCallback64(ch, on_choreo_frame, st);

    for (int i = 0; i < 16 && (!st->timer_fired || st->choreo_ns <= 0); ++i) {
        ALooper_pollOnce(100, NULL, NULL, NULL);
    }
    close(tfd);
    return NULL;
}

struct FutexWakeArg {
    volatile uint32_t* word;
};

static void* futex_waker(void* arg) {
    struct FutexWakeArg* fw = (struct FutexWakeArg*)arg;
    struct timespec req = {0, 5 * 1000 * 1000};
    nanosleep(&req, NULL);
    __atomic_store_n(fw->word, 1u, __ATOMIC_RELEASE);
    syscall(__NR_futex, fw->word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    return NULL;
}

static int phdr_cb(struct dl_phdr_info* info, size_t size, void* data) {
    (void)size;
    if (info && info->dlpi_name && strstr(info->dlpi_name, "libn2check.so")) {
        *(int*)data += 1;
    }
    return 0;
}

static int check_setjmp_helper(jmp_buf env, int v) {
    longjmp(env, v);
    return -1;
}

JNIEXPORT jstring JNICALL
Java_org_winapk_fixture_n2_MainActivity_runChecks(JNIEnv* env, jclass cls, jobject jassets, jobject jbmp) {
    (void)cls;

    /* 1. libm.so checks */
    double trig = sin(0.5) * sin(0.5) + cos(0.5) * cos(0.5);
    double hyp = hypot(3.0, 4.0);
    double pw = pow(2.0, 10.0);
    double fm = fma(3.0, 4.0, 5.0);
    if (fabs(trig - 1.0) > 1e-12 || fabs(hyp - 5.0) > 1e-12 || fabs(pw - 1024.0) > 1e-9 || fabs(fm - 17.0) > 1e-12) {
        return (*env)->NewStringUTF(env, "FAIL:libm");
    }

    /* 2. libjnigraphics.so positive + negative checks */
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, NULL, &info) >= 0) {
        return (*env)->NewStringUTF(env, "FAIL:jnigraphics-neg");
    }
    if (AndroidBitmap_getInfo(env, jbmp, &info) != ANDROID_BITMAP_RESULT_SUCCESS ||
        info.width != 4 || info.height != 4 || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        return (*env)->NewStringUTF(env, "FAIL:jnigraphics-info");
    }
    void* pixels = NULL;
    if (AndroidBitmap_lockPixels(env, jbmp, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS || !pixels) {
        return (*env)->NewStringUTF(env, "FAIL:jnigraphics-lock");
    }
    for (uint32_t y = 0; y < info.height; ++y) {
        uint8_t* row = (uint8_t*)pixels + y * info.stride;
        for (uint32_t x = 0; x < info.width; ++x) {
            row[x * 4 + 0] = 0x22;
            row[x * 4 + 1] = 0x88;
            row[x * 4 + 2] = 0xCC;
            row[x * 4 + 3] = 0xFF;
        }
    }
    if (AndroidBitmap_unlockPixels(env, jbmp) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return (*env)->NewStringUTF(env, "FAIL:jnigraphics-unlock");
    }

    /* 3. AAssetManager positive + negative checks */
    AAssetManager* mgr = AAssetManager_fromJava(env, jassets);
    if (!mgr) return (*env)->NewStringUTF(env, "FAIL:asset-mgr");
    if (AAssetManager_open(mgr, "does-not-exist.txt", AASSET_MODE_BUFFER) != NULL) {
        return (*env)->NewStringUTF(env, "FAIL:asset-neg");
    }
    AAsset* asset = AAssetManager_open(mgr, "greeting.txt", AASSET_MODE_BUFFER);
    if (!asset) return (*env)->NewStringUTF(env, "FAIL:asset-open");
    off64_t alen = AAsset_getLength64(asset);
    const char* abuf = (const char*)AAsset_getBuffer(asset);
    int asset_ok = (alen >= 14 && abuf && strncmp(abuf, "hello-n2-asset", 14) == 0);
    AAsset_close(asset);
    if (!asset_ok) return (*env)->NewStringUTF(env, "FAIL:asset-data");

    AAssetDir* adir = AAssetManager_openDir(mgr, "");
    if (!adir) return (*env)->NewStringUTF(env, "FAIL:asset-dir");
    int saw_greeting = 0;
    const char* fname = NULL;
    while ((fname = AAssetDir_getNextFileName(adir)) != NULL) {
        if (strcmp(fname, "greeting.txt") == 0) saw_greeting = 1;
    }
    AAssetDir_close(adir);
    if (!saw_greeting) return (*env)->NewStringUTF(env, "FAIL:asset-dir-entry");

    /* 4. ALooper + AChoreographer + timerfd on worker pthread */
    struct LooperCheckState lst = {0, 0};
    pthread_t th;
    if (pthread_create(&th, NULL, looper_worker, &lst) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:looper-thread");
    }
    pthread_join(th, NULL);
    if (!lst.timer_fired || lst.choreo_ns <= 0) {
        return (*env)->NewStringUTF(env, "FAIL:looper-choreo");
    }

    /* 5. epoll + eventfd + TCP loopback + SYS_futex */
    int ep = epoll_create1(EPOLL_CLOEXEC);
    int ev = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (ep < 0 || ev < 0) return (*env)->NewStringUTF(env, "FAIL:epoll-create");
    struct epoll_event epe;
    memset(&epe, 0, sizeof(epe));
    epe.events = EPOLLIN;
    epe.data.u64 = 0x77;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, ev, &epe) != 0) return (*env)->NewStringUTF(env, "FAIL:epoll-ctl");
    uint64_t one = 3;
    if (write(ev, &one, 8) != 8) return (*env)->NewStringUTF(env, "FAIL:eventfd-write");
    struct epoll_event out_ev;
    if (epoll_wait(ep, &out_ev, 1, 100) != 1 || out_ev.data.u64 != 0x77) {
        return (*env)->NewStringUTF(env, "FAIL:epoll-wait");
    }
    uint64_t got_ev = 0;
    if (read(ev, &got_ev, 8) != 8 || got_ev != 3) return (*env)->NewStringUTF(env, "FAIL:eventfd-read");
    close(ev);
    close(ep);

    /* Loopback TCP socket */
    int srv = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int cli = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (srv < 0 || cli < 0) return (*env)->NewStringUTF(env, "FAIL:socket");
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    socklen_t slen = sizeof(addr);
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0 ||
        getsockname(srv, (struct sockaddr*)&addr, &slen) != 0 ||
        listen(srv, 1) != 0 ||
        connect(cli, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:tcp-connect");
    }
    int acc = accept(srv, NULL, NULL);
    char ping[4] = {'N', '2', 'O', 'K'};
    char pong[4] = {0};
    if (acc < 0 || send(cli, ping, 4, 0) != 4 || recv(acc, pong, 4, 0) != 4 || memcmp(ping, pong, 4) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:tcp-io");
    }
    close(acc);
    close(cli);
    close(srv);

    /* SYS_futex wait/wake + timeout */
    volatile uint32_t fword = 0;
    struct timespec short_ts = {0, 1 * 1000 * 1000};
    errno = 0;
    long ftmo = syscall(__NR_futex, &fword, FUTEX_WAIT_PRIVATE, 0, &short_ts, NULL, 0);
    if (!(ftmo == -1 && errno == ETIMEDOUT)) return (*env)->NewStringUTF(env, "FAIL:futex-timeout");
    struct FutexWakeArg farg = {&fword};
    pthread_t wth;
    pthread_create(&wth, NULL, futex_waker, &farg);
    while (__atomic_load_n(&fword, __ATOMIC_ACQUIRE) == 0) {
        syscall(__NR_futex, &fword, FUTEX_WAIT_PRIVATE, 0, NULL, NULL, 0);
    }
    pthread_join(wth, NULL);

    /* 6. STT_OBJECT RTLD_DEFAULT dlsym, dladdr, dl_iterate_phdr, variadic snprintf/sscanf, setjmp/longjmp */
    uint64_t* sym_obj = (uint64_t*)dlsym(RTLD_DEFAULT, "g_n2_exported_object");
    if (!sym_obj || *sym_obj != 0x4E324F4B55AA1234ULL) {
        return (*env)->NewStringUTF(env, "FAIL:dlsym-object");
    }
    Dl_info dli;
    memset(&dli, 0, sizeof(dli));
    if (dladdr((const void*)&Java_org_winapk_fixture_n2_MainActivity_runChecks, &dli) == 0 ||
        !dli.dli_fname || !strstr(dli.dli_fname, "libn2check.so")) {
        return (*env)->NewStringUTF(env, "FAIL:dladdr");
    }
    int phdr_hits = 0;
    dl_iterate_phdr(phdr_cb, &phdr_hits);
    if (phdr_hits < 1) return (*env)->NewStringUTF(env, "FAIL:dl-iterate-phdr");

    char fmt_buf[64];
    int sn = snprintf(fmt_buf, sizeof(fmt_buf), "%s:%lld:%.2f", "n2", (long long)-42, 3.25);
    char tag[8] = {0};
    long long iv = 0;
    double dv = 0.0;
    if (sn <= 0 || sscanf(fmt_buf, "%2[^:]:%lld:%lf", tag, &iv, &dv) != 3 ||
        strcmp(tag, "n2") != 0 || iv != -42 || fabs(dv - 3.25) > 1e-6) {
        return (*env)->NewStringUTF(env, "FAIL:variadic-stdio");
    }

    jmp_buf jenv;
    volatile int jumped = setjmp(jenv);
    if (jumped == 0) {
        check_setjmp_helper(jenv, 77);
        return (*env)->NewStringUTF(env, "FAIL:longjmp-returned");
    } else if (jumped != 77) {
        return (*env)->NewStringUTF(env, "FAIL:longjmp-value");
    }

    /* 7. OpenSLES positive + AAudio negative refusal */
    SLObjectItf engine_obj = NULL;
    SLEngineItf engine_itf = NULL;
    SLObjectItf mix_obj = NULL;
    if (slCreateEngine(&engine_obj, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS || !engine_obj ||
        (*engine_obj)->Realize(engine_obj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS ||
        (*engine_obj)->GetInterface(engine_obj, SL_IID_ENGINE, &engine_itf) != SL_RESULT_SUCCESS ||
        (*engine_itf)->CreateOutputMix(engine_itf, &mix_obj, 0, NULL, NULL) != SL_RESULT_SUCCESS ||
        (*mix_obj)->Realize(mix_obj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
        return (*env)->NewStringUTF(env, "FAIL:opensles");
    }
    (*mix_obj)->Destroy(mix_obj);
    (*engine_obj)->Destroy(engine_obj);

    AAudioStreamBuilder* aab = NULL;
    if (AAudio_createStreamBuilder(&aab) >= 0) {
        return (*env)->NewStringUTF(env, "FAIL:aaudio-must-refuse");
    }

    /* 8. /system/etc/fonts.xml + /fonts/segoeui.ttf read-only positive + write/W^X negative refusals */
    int fxml = open("/system/etc/fonts.xml", O_RDONLY);
    struct stat st;
    if (fxml < 0 || fstat(fxml, &st) != 0 || st.st_size < 100) {
        if (fxml >= 0) close(fxml);
        return (*env)->NewStringUTF(env, "FAIL:fonts-xml");
    }
    void* xml_map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fxml, 0);
    close(fxml);
    if (xml_map == MAP_FAILED || memcmp(xml_map, "<?xml", 5) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:fonts-xml-mmap");
    }
    munmap(xml_map, (size_t)st.st_size);

    int fttf = open("/fonts/segoeui.ttf", O_RDONLY);
    if (fttf < 0) return (*env)->NewStringUTF(env, "FAIL:fonts-ttf-read");
    close(fttf);

    errno = 0;
    if (open("/fonts/segoeui.ttf", O_WRONLY) != -1 || errno != EACCES) {
        return (*env)->NewStringUTF(env, "FAIL:fonts-write-refusal");
    }
    errno = 0;
    if (mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != MAP_FAILED ||
        errno != EINVAL) {
        return (*env)->NewStringUTF(env, "FAIL:wx-refusal");
    }

    /* 9. Dart VirtualMemory::AllocateAligned regression: reserve PROT_NONE, munmap prefix + suffix,
          then mprotect(PROT_READ|PROT_WRITE) middle pages without losing the reservation. */
    uint8_t* cage = (uint8_t*)mmap(NULL, 2 * 1024 * 1024, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cage == MAP_FAILED) return (*env)->NewStringUTF(env, "FAIL:cage-mmap");
    if (munmap(cage, 64 * 1024) != 0 || munmap(cage + 1024 * 1024, 1024 * 1024) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:cage-trim");
    }
    uint8_t* mid = cage + 64 * 1024;
    if (mprotect(mid, 128 * 1024, PROT_READ | PROT_WRITE) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:cage-commit");
    }
    mid[0] = 0x5A;
    mid[128 * 1024 - 1] = 0xA5;
    if (mid[0] != 0x5A || mid[128 * 1024 - 1] != 0xA5 ||
        mprotect(mid, 128 * 1024, PROT_NONE) != 0 ||
        munmap(mid, 960 * 1024) != 0) {
        return (*env)->NewStringUTF(env, "FAIL:cage-release");
    }

    return (*env)->NewStringUTF(env, "PASS:all-n2");
}
