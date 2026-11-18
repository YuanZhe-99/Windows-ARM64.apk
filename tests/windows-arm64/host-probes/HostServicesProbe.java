import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.graphics.MacApkSystemFonts;
import android.os.Looper;
import android.os.PersistableBundle;
import android.system.OsConstants;
import com.macapk.loader.NativeCocoa;
import com.macapk.loader.NativeOs;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/**
 * Verifies the Windows ARM64 host services bridge (NativeOs, isolated host clipboard,
 * DirectWrite PostScript font lookup, /proc synthesis and gated host URL opening).
 */
public final class HostServicesProbe {
    private static final byte[] SAMPLE_XZ = new byte[] {
        (byte)0xfd, (byte)0x37, (byte)0x7a, (byte)0x58, (byte)0x5a, (byte)0x00, (byte)0x00, (byte)0x04,
        (byte)0xe6, (byte)0xd6, (byte)0xb4, (byte)0x46, (byte)0x02, (byte)0x00, (byte)0x21, (byte)0x01,
        (byte)0x16, (byte)0x00, (byte)0x00, (byte)0x00, (byte)0x74, (byte)0x2f, (byte)0xe5, (byte)0xa3,
        (byte)0x01, (byte)0x00, (byte)0x1a, (byte)0x57, (byte)0x69, (byte)0x6e, (byte)0x64, (byte)0x6f,
        (byte)0x77, (byte)0x73, (byte)0x2d, (byte)0x41, (byte)0x52, (byte)0x4d, (byte)0x36, (byte)0x34,
        (byte)0x2e, (byte)0x61, (byte)0x70, (byte)0x6b, (byte)0x20, (byte)0x58, (byte)0x5a, (byte)0x20,
        (byte)0x74, (byte)0x65, (byte)0x73, (byte)0x74, (byte)0x21, (byte)0x0a, (byte)0x00, (byte)0x00,
        (byte)0x39, (byte)0x3e, (byte)0x38, (byte)0x83, (byte)0x58, (byte)0x18, (byte)0x69, (byte)0xcb,
        (byte)0x00, (byte)0x01, (byte)0x33, (byte)0x1b, (byte)0xf7, (byte)0x19, (byte)0x88, (byte)0x5e,
        (byte)0x1f, (byte)0xb6, (byte)0xf3, (byte)0x7d, (byte)0x01, (byte)0x00, (byte)0x00, (byte)0x00,
        (byte)0x00, (byte)0x04, (byte)0x59, (byte)0x5a
    };

    private HostServicesProbe() {}

    private static void check(boolean ok, String msg) {
        if (!ok) throw new AssertionError(msg);
    }

    private static void testNativeOs(File workDir) throws Exception {
        // 1. File descriptor positive & negative matrix
        File missing = new File(workDir, "does-not-exist.bin");
        check(NativeOs.open(missing.getPath(), OsConstants.O_RDONLY, 0) == -OsConstants.ENOENT, "missing open errno");
        check(NativeOs.open(workDir.getPath(), OsConstants.O_RDWR, 0) == -OsConstants.EISDIR, "directory write errno");
        check(NativeOs.close(-1) == -9 && NativeOs.close(99999) == -9, "invalid fd close EBADF");
        check(NativeOs.lseek(-1, 0, 0) == -9, "invalid fd lseek EBADF");

        File target = new File(workDir, "测试-os.bin");
        int fd = NativeOs.open(target.getPath(), OsConstants.O_RDWR | OsConstants.O_CREAT | OsConstants.O_TRUNC, 0644);
        check(fd >= 0, "open create UTF-8 path: " + fd);
        byte[] payload = "hello-winapk-os".getBytes(StandardCharsets.UTF_8);
        check(NativeOs.write(fd, payload, 0, payload.length) == payload.length, "write full");
        check(NativeOs.lseek(fd, 6, 0) == 6, "lseek SEEK_SET");
        byte[] buf = new byte[6];
        check(NativeOs.read(fd, buf, 0, 6) == 6, "read slice");
        check("winapk".equals(new String(buf, StandardCharsets.UTF_8)), "read content");

        long[] st = NativeOs.fstat(fd);
        check(st != null && st.length == 11 && st[4] == payload.length, "fstat size");
        check(NativeOs.ftruncate(fd, 5) == 0, "ftruncate");
        st = NativeOs.fstat(fd);
        check(st != null && st[4] == 5, "fstat truncated size");

        int dupFd = NativeOs.dup(fd);
        check(dupFd >= 0 && dupFd != fd, "dup");
        check(NativeOs.close(fd) == 0, "close original");
        check(NativeOs.lseek(dupFd, 0, 0) == 0, "lseek on dup");
        byte[] five = new byte[5];
        check(NativeOs.read(dupFd, five, 0, 5) == 5 && "hello".equals(new String(five, StandardCharsets.UTF_8)), "read on dup");

        long[] vfs1 = NativeOs.statvfs(workDir.getPath());
        long[] vfs2 = NativeOs.fstatvfs(dupFd);
        check(vfs1 != null && vfs1.length == 11 && vfs1[0] == 4096 && vfs1[2] > 0 && vfs1[4] > 0, "statvfs");
        check(vfs2 != null && vfs2.length == 11 && vfs2[2] == vfs1[2], "fstatvfs");
        check(NativeOs.statvfs(new File(workDir, "nope").getPath()) == null, "statvfs missing");
        check(NativeOs.close(dupFd) == 0, "close dup");

        // 2. memfdCreate
        int mfd = NativeOs.memfdCreate("test-memfd", 0);
        check(mfd >= 0, "memfdCreate");
        check(NativeOs.write(mfd, payload, 0, payload.length) == payload.length, "memfd write");
        check(NativeOs.lseek(mfd, 0, 0) == 0, "memfd seek");
        byte[] mread = new byte[payload.length];
        check(NativeOs.read(mfd, mread, 0, mread.length) == payload.length, "memfd read");
        check(NativeOs.close(mfd) == 0, "memfd close");

        // 3. takeRunLock contention
        File lockFile = new File(workDir, "app.pid");
        int lock1 = NativeOs.takeRunLock(lockFile.getPath());
        check(lock1 >= 0, "takeRunLock first");
        int lock2 = NativeOs.takeRunLock(lockFile.getPath());
        check(lock2 == -11, "takeRunLock contention EWOULDBLOCK: " + lock2);
        check(NativeOs.close(lock1) == 0, "release run lock");

        // 4. sysconf
        check(NativeOs.sysconf(0x27) >= 4096 && NativeOs.sysconf(0x28) == NativeOs.sysconf(0x27), "sysconf page size");
        check(NativeOs.sysconf(0x06) == 100, "sysconf clk_tck");
        check(NativeOs.sysconf(0x61) >= 1, "sysconf nprocessors");
        check(NativeOs.sysconf(0x7fff) == -1, "sysconf unknown");

        // 5. setenv / getenv / unsetenv
        check(NativeOs.setenv("BAD=KEY", "x", true) == -22, "setenv invalid key");
        check(NativeOs.setenv("WINAPK_TEST_ENV", "first", true) == 0, "setenv initial");
        check("first".equals(NativeOs.getenv("WINAPK_TEST_ENV")), "getenv initial");
        check(NativeOs.setenv("WINAPK_TEST_ENV", "second", false) == 0, "setenv no-overwrite");
        check("first".equals(NativeOs.getenv("WINAPK_TEST_ENV")), "getenv preserved");
        check(NativeOs.setenv("WINAPK_TEST_ENV", "second", true) == 0, "setenv overwrite");
        check("second".equals(NativeOs.getenv("WINAPK_TEST_ENV")), "getenv overwritten");
        check(NativeOs.unsetenv("WINAPK_TEST_ENV") == 0 && NativeOs.getenv("WINAPK_TEST_ENV") == null, "unsetenv");

        // 6. xzDecompress positive & negative
        byte[] decoded = NativeOs.xzDecompress(SAMPLE_XZ);
        check(decoded != null && "Windows-ARM64.apk XZ test!\n".equals(new String(decoded, StandardCharsets.UTF_8)), "xzDecompress valid");
        byte[] corrupt = SAMPLE_XZ.clone();
        corrupt[10] ^= 0x55;
        check(NativeOs.xzDecompress(corrupt) == null && NativeOs.xzDecompress(new byte[0]) == null, "xzDecompress invalid");
        System.out.println("PASS NativeOs fd/memfd/lock/sysconf/statvfs/env/xz");
    }

    private static void testClipboard() throws Exception {
        Class<?> pb = Class.forName("android.content.ClipboardManager$MacApkHostPasteboard");
        Method pbName = pb.getDeclaredMethod("nativePasteboardName");
        pbName.setAccessible(true);
        String name = (String) pbName.invoke(null);
        check(name != null && name.startsWith("winapk-test-pasteboard-"), "isolated pasteboard active: " + name);

        Field focusOverride = ClipboardManager.class.getDeclaredField("sMacApkFocusOverrideForTest");
        focusOverride.setAccessible(true);
        focusOverride.set(null, Boolean.TRUE);

        if (Looper.getMainLooper() == null) {
            Looper.prepareMainLooper();
        }
        ClipboardManager cm = new ClipboardManager();
        ClipData htmlClip = ClipData.newHtmlText("greeting", "Hello \uD83D\uDCCB ARM64", "<b>ARM64</b>");
        PersistableBundle extras = new PersistableBundle();
        extras.putBoolean(ClipDescription.EXTRA_IS_SENSITIVE, true);
        htmlClip.getDescription().setExtras(extras);
        cm.setPrimaryClip(htmlClip);

        check(cm.hasPrimaryClip(), "hasPrimaryClip");
        ClipData got = cm.getPrimaryClip();
        check(got != null && got.getItemCount() == 1, "getPrimaryClip count");
        check("Hello \uD83D\uDCCB ARM64".equals(got.getItemAt(0).getText().toString()), "clipboard UTF-16 surrogate text");
        check("<b>ARM64</b>".equals(got.getItemAt(0).getHtmlText()), "clipboard CF_HTML roundtrip");

        Method pbTypes = pb.getDeclaredMethod("nativeTypes");
        pbTypes.setAccessible(true);
        int mask = (Integer) pbTypes.invoke(null);
        check((mask & 8) != 0, "sensitive clip carries TYPE_CONCEALED");

        focusOverride.set(null, Boolean.FALSE);
        check(!cm.hasPrimaryClip() && cm.getPrimaryClip() == null, "unfocused read denied per Android 10+");
        focusOverride.set(null, Boolean.TRUE);

        cm.clearPrimaryClip();
        check(!cm.hasPrimaryClip(), "clearPrimaryClip");
        boolean threwNull = false;
        try {
            cm.setPrimaryClip(null);
        } catch (NullPointerException expected) {
            threwNull = true;
        }
        check(threwNull, "setPrimaryClip(null) throws NPE");
        System.out.println("PASS ClipboardManager isolated roundtrip, CF_HTML, UTF-16 emoji, focus gate and ConcealedType");
    }

    private static void testFontsProcAndUrl(File urlCapture) throws Exception {
        String yahei = NativeCocoa.hostFontFile("MicrosoftYaHei");
        String arial = NativeCocoa.hostFontFile("ArialMT");
        check((yahei != null && new File(yahei).isFile()) || (arial != null && new File(arial).isFile()),
              "DirectWrite resolved system font file");
        check(NativeCocoa.hostFontFile("NoSuchPostScriptFace-WinApk999") == null, "unknown PostScript face returns null");

        String fontsDir = MacApkSystemFonts.materialize(NativeCocoa::hostFontFile);
        check(fontsDir != null && new File(fontsDir, "fonts.xml").isFile(), "MacApkSystemFonts materialized fonts.xml");
        String xml = Files.readString(new File(fontsDir, "fonts.xml").toPath());
        check(xml.contains("zh-Hans"), "fonts.xml includes Windows CJK fallback family");
        NativeCocoa.setSystemFontsDir(fontsDir);
        NativeCocoa.setSystemProperty("ro.build.version.sdk", "36");

        String meminfo = NativeCocoa.remapProcPath("/proc/meminfo");
        String status = NativeCocoa.remapProcPath("/proc/self/status");
        check(meminfo != null && Files.readString(new File(meminfo).toPath()).contains("MemTotal:"), "/proc/meminfo synthesized");
        check(status != null && Files.readString(new File(status).toPath()).contains("VmRSS:"), "/proc/self/status synthesized");
        check(NativeCocoa.remapProcPath("/proc/kallsyms") == null, "unhandled /proc path returns null");

        check(!NativeCocoa.openHostUrl("file:///C:/Windows/System32/cmd.exe"), "refuse file:// URL");
        check(!NativeCocoa.openHostUrl("javascript:alert(1)"), "refuse javascript: URL");
        check(!NativeCocoa.openHostUrl("https://example.com/bad space"), "refuse whitespace URL");
        check(NativeCocoa.openHostUrl("https://example.com/winapk?ok=1"), "accept https:// URL");
        String captured = Files.readString(urlCapture.toPath()).trim();
        check("https://example.com/winapk?ok=1".equals(captured), "captured URL matches");
        System.out.println("PASS DirectWrite host fonts, /proc synthesis and openHostUrl allowlist");
    }

    public static void main(String[] args) throws Exception {
        System.load(args[0]);
        File workDir = new File(args[1]);
        File urlCapture = new File(args[2]);
        workDir.mkdirs();
        testNativeOs(workDir);
        testClipboard();
        testFontsProcAndUrl(urlCapture);
        System.exit(0);
    }
}
