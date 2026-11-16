package com.jack.tombraider;

import java.nio.ByteBuffer;

/** JNI surface of libtrlink.so (see jni/psxlink.c). */
final class Native {
    static {
        System.loadLibrary("pcsx_rearmed");
        System.loadLibrary("trlink");
    }
    private Native() {}

    static native void init(String systemDir, String saveDir);
    static native void setVariable(String key, String value);
    static native boolean loadGame(String path, String memcardPath, int padType);
    static native void setPadType(int padType);
    static native void runFrame();
    static native double fps();
    static native double sampleRate();
    static native float aspect();
    static native int copyFrame(ByteBuffer dst, int lastSeq, int[] seqOut);
    static native int readAudio(short[] out);
    static native int audioQueued();
    static native void setButtons(int port, int mask);
    static native void setAxes(int port, int lx, int ly, int rx, int ry);
    static native void setStickDpad(boolean on, int threshold);
    static native byte[] readRam(int addr, int len);
    static native boolean writeRam(int addr, byte[] data);
    static native String gameId();
    static native String gameLabel();
    static native String pollMessage(int[] seq);
    static native boolean saveMemcard();
    static native long memcardHash();
    static native boolean saveState(String path);
    /** The state as bytes (same contents saveState writes), for writing on another thread; null on failure. */
    static native byte[] stateBytes();
    static native boolean loadState(String path);
    static native void reset();
    static native void setCheat(int index, boolean enabled, String code);
    static native void clearCheats();
    static native int discCount();
    static native int discIndex();
    static native boolean setDisc(int index);
    static native boolean addDisc(String path);
    static native int appendDisc(String path);
    static native void setAnalogMode(boolean on);
    /** Tomb Raider's analog controls and detached camera (Mods screen): option bits 1 move where the stick points, 2 light push walks, 4 right stick camera,
        8 invert, 16 detached camera, 32 left stick not used (detached camera alone). */
    static native void setTrAnalog(boolean on, int options);
    /** Dev codes: always-on bits (1 invincible, 2 air, 4 ammo); one-shots (1 health, 2 weapons, 3 medi packs, 4 keys); warp to a level, -2 next, -3 restart, -4 the ending and credits. */
    static native void setTrDev(int options);
    static native void trDevDo(int what);
    static native void trWarp(int level);
    static native String trLevels();
    /** Analog controls program (bytes from 0x80017400), master switch and option bits; installed by the frontend. */
    static native void setAnalogMod(byte[] program, boolean on, int options);
    /** 0 off, 1 waiting for the game, 2 running, 3 stood down (the game had data where the program goes) */
    static native int analogModState();
    /** Plain-text report for Settings > Diagnostics. */
    static native String diagnostics();
    static native int cardStatus();
    static native byte[] readMemcard();
    static native boolean writeMemcard(byte[] data);
    static native void unload();

    // libretro joypad bit ids
    static final int B = 0, Y = 1, SELECT = 2, START = 3, UP = 4, DOWN = 5, LEFT = 6, RIGHT = 7,
            A = 8, X = 9, L = 10, R = 11, L2 = 12, R2 = 13, L3 = 14, R3 = 15;
    // PlayStation names for the same bits
    static final int CROSS = B, SQUARE = Y, CIRCLE = A, TRIANGLE = X;
}
