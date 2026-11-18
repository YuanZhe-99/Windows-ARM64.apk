import com.macapk.loader.NativeAudio;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Verifies the Windows ARM64 WASAPI NativeAudio bridge across hardware WASAPI (with live
 * endpoint rebind), PCM_8BIT/16BIT/FLOAT formats, buffer/latency sizing, rate resampling,
 * SoundPool, MediaPlayer and deviceless fallback.
 */
public final class AudioProbe {
    private AudioProbe() {}

    private static void check(boolean ok, String msg) {
        if (!ok) throw new AssertionError(msg);
    }

    private static byte[] makeSilentWav(int sampleRate, int frames) {
        int dataBytes = frames * 2; // 16-bit mono
        ByteBuffer b = ByteBuffer.allocate(44 + dataBytes).order(ByteOrder.LITTLE_ENDIAN);
        b.put(new byte[]{'R', 'I', 'F', 'F'});
        b.putInt(36 + dataBytes);
        b.put(new byte[]{'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
        b.putInt(16);
        b.putShort((short) 1);          // PCM
        b.putShort((short) 1);          // 1 channel
        b.putInt(sampleRate);
        b.putInt(sampleRate * 2);
        b.putShort((short) 2);
        b.putShort((short) 16);
        b.put(new byte[]{'d', 'a', 't', 'a'});
        b.putInt(dataBytes);
        return b.array();
    }

    private static void runHardwareOrVirtual(boolean isHardware) throws Exception {
        // Negative creation checks
        check(NativeAudio.nativeCreate(0, 2, 2, 1024) == 0, "reject 0 Hz");
        check(NativeAudio.nativeCreate(48000, 0, 2, 1024) == 0, "reject 0 channels");
        check(NativeAudio.nativeCreate(48000, 2, 99, 1024) == 0, "reject unknown encoding");

        // Create stereo float stream (silent 0.0f samples so hardware runs make zero audible sound)
        long hFloat = NativeAudio.nativeCreate(48000, 2, 4 /* PCM_FLOAT */, 2048);
        check(hFloat > 0, "create PCM_FLOAT track");
        int cap = NativeAudio.nativeGetBufferCapacity(hFloat);
        check(cap >= 2048, "buffer capacity >= requested: " + cap);
        check(NativeAudio.nativeSetBufferSize(hFloat, 8) == 16, "setBufferSize clamps to AOSP min 16");
        check(NativeAudio.nativeSetBufferSize(hFloat, 64) == 64, "setBufferSize 64 frames");

        float[] silentFloats = new float[512]; // 256 stereo frames
        int nonBlock = NativeAudio.nativeWriteFloats(hFloat, silentFloats, 0, silentFloats.length, false);
        // If WASAPI endpoint is active, non-blocking write queues at most 64*2 = 128 floats; if deviceless, swallows all 512.
        check(nonBlock == 128 || nonBlock == 512, "non-blocking write respects shrunk buffer threshold: " + nonBlock);
        check(NativeAudio.nativeSetBufferSize(hFloat, cap) == cap, "restore full buffer size");

        long[] ts = new long[2];
        check(!NativeAudio.nativeGetTimestamp(hFloat, ts), "timestamp false before first rendered frame");
        NativeAudio.nativeSetVolume(hFloat, 0.0f, 0.0f);
        NativeAudio.nativePlay(hFloat);
        check(NativeAudio.nativeGetPlayState(hFloat) == 3, "PLAYSTATE_PLAYING");

        // Stream ~120 ms of silent frames across blocking writes (covers live WASAPI rebind when enabled)
        float[] blockChunk = new float[4800 * 2];
        int written = NativeAudio.nativeWriteFloats(hFloat, blockChunk, 0, blockChunk.length, true);
        check(written == blockChunk.length, "blocking float write completed: " + written);
        Thread.sleep(isHardware ? 140 : 90);

        long head = NativeAudio.nativeGetPlaybackHeadPosition(hFloat);
        check(head > 0, "playback head advanced: " + head);
        check(NativeAudio.nativeGetTimestamp(hFloat, ts) && ts[0] > 0 && ts[1] > 0, "QPC timestamp populated");

        // Rate & params checks
        check(NativeAudio.nativeSetPlaybackRate(hFloat, -1) == -2, "reject negative playback rate");
        check(NativeAudio.nativeSetPlaybackRate(hFloat, 96000) == 0, "accept 96 kHz resample rate");
        check(NativeAudio.nativeGetPlaybackRate(hFloat) == 96000, "getPlaybackRate reports 96000");
        check(NativeAudio.nativeSetPlaybackParams(hFloat, 1.5f, 1.0f) == -2, "refuse uncoupled time-stretch speed!=pitch");
        check(NativeAudio.nativeSetPlaybackParams(hFloat, 1.25f, 1.25f) == 0, "accept coupled speed==pitch");
        float[] params = new float[2];
        NativeAudio.nativeGetPlaybackParams(hFloat, params);
        check(Math.abs(params[0] - 1.25f) < 1e-4f && Math.abs(params[1] - 1.25f) < 1e-4f, "getPlaybackParams matches");

        if (!isHardware) {
            // Starve virtual stream briefly to verify underrun detection
            Thread.sleep(60);
            check(NativeAudio.nativeGetUnderrunCount(hFloat) >= 1, "underrun episode counted when starved");
        }

        NativeAudio.nativePause(hFloat);
        check(NativeAudio.nativeGetPlayState(hFloat) == 2, "PLAYSTATE_PAUSED");
        NativeAudio.nativeStop(hFloat);
        check(NativeAudio.nativeGetPlayState(hFloat) == 1 && NativeAudio.nativeGetPlaybackHeadPosition(hFloat) == 0,
              "stop resets head to zero");
        NativeAudio.nativeRelease(hFloat);

        // Verify PCM_16BIT and PCM_8BIT encodings
        long h16 = NativeAudio.nativeCreate(44100, 2, 2 /* PCM_16BIT */, 1024);
        short[] s16 = new short[512];
        check(NativeAudio.nativeWriteShorts(h16, s16, 0, s16.length, false) > 0, "PCM_16BIT writeShorts");
        byte[] b16 = new byte[512];
        check(NativeAudio.nativeWriteBytes(h16, b16, 0, b16.length, false) > 0, "PCM_16BIT writeBytes");
        NativeAudio.nativeFlush(h16);
        NativeAudio.nativeRelease(h16);

        long h8 = NativeAudio.nativeCreate(22050, 1, 3 /* PCM_8BIT */, 1024);
        byte[] b8 = new byte[256];
        java.util.Arrays.fill(b8, (byte) 128);
        check(NativeAudio.nativeWriteBytes(h8, b8, 0, b8.length, false) == 256, "PCM_8BIT writeBytes");
        NativeAudio.nativeRelease(h8);

        // SoundPool & MediaPlayer on in-memory WAV
        check(NativeAudio.nativeSpLoad(new byte[]{1, 2, 3, 4, 5}, 5) == 0, "refuse corrupt clip");
        byte[] wav = makeSilentWav(24000, 2400); // 100 ms
        int soundId = NativeAudio.nativeSpLoad(wav, wav.length);
        check(soundId > 0, "SoundPool load WAV");
        int streamId = NativeAudio.nativeSpPlay(soundId, 0.0f, 0.0f, 0, 1.5f);
        check(streamId > 0, "SoundPool play");
        NativeAudio.nativeSpSetRate(streamId, 2.0f);
        NativeAudio.nativeSpAutoPause();
        NativeAudio.nativeSpAutoResume();
        NativeAudio.nativeSpStop(streamId);
        check(NativeAudio.nativeSpUnload(soundId), "SoundPool unload");
        check(!NativeAudio.nativeSpUnload(soundId), "SoundPool double unload false");

        long mp = NativeAudio.nativeMpCreate();
        check(mp > 0, "MediaPlayer create");
        check(!NativeAudio.nativeMpSetSource(mp, new byte[]{0, 1, 2, 3, 4}, 5), "MediaPlayer refuse invalid source");
        check(NativeAudio.nativeMpSetSource(mp, wav, wav.length), "MediaPlayer setSource WAV");
        int dur = NativeAudio.nativeMpDuration(mp);
        check(dur >= 95 && dur <= 105, "MediaPlayer 100ms duration: " + dur);
        NativeAudio.nativeMpSetVolume(mp, 0.0f, 0.0f);
        NativeAudio.nativeMpSetLooping(mp, true);
        NativeAudio.nativeMpSeek(mp, 40);
        check(Math.abs(NativeAudio.nativeMpPosition(mp) - 40) <= 5, "MediaPlayer seek position");
        NativeAudio.nativeMpStart(mp);
        check(NativeAudio.nativeMpIsPlaying(mp), "MediaPlayer playing");
        Thread.sleep(30);
        NativeAudio.nativeMpPause(mp);
        check(!NativeAudio.nativeMpIsPlaying(mp), "MediaPlayer paused");
        NativeAudio.nativeMpStop(mp);
        NativeAudio.nativeMpRelease(mp);
        System.out.println("PASS AudioProbe mode=" + (isHardware ? "hardware" : "virtual"));
    }

    private static void runDeviceless() {
        long h = NativeAudio.nativeCreate(48000, 2, 2, 1024);
        check(h > 0, "deviceless create");
        NativeAudio.nativePlay(h);
        short[] big = new short[16384]; // larger than ring capacity; must swallow without blocking
        long t0 = System.nanoTime();
        int w = NativeAudio.nativeWriteShorts(h, big, 0, big.length, true);
        long ms = (System.nanoTime() - t0) / 1_000_000L;
        check(w == big.length && ms < 500, "deviceless blocking write swallowed immediately (" + ms + "ms)");
        check(NativeAudio.nativeGetPlaybackHeadPosition(h) == big.length / 2, "deviceless head advances");
        NativeAudio.nativeRelease(h);
        System.out.println("PASS AudioProbe mode=deviceless");
    }

    public static void main(String[] args) throws Exception {
        System.load(args[0]);
        String mode = args[1];
        if ("deviceless".equals(mode)) {
            runDeviceless();
        } else {
            runHardwareOrVirtual("hardware".equals(mode));
        }
        System.exit(0);
    }
}
