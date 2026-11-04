package com.winapk.probe;

import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.GraphicsEnvironment;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;

/**
 * Checks, inside the JVM that winapk.exe embeds, what the runtime needs of
 * the JDK (PLAN.md §7, fixture T01): a JNI call into the host and back, Java2D
 * drawing into an off-screen bitmap, and the fonts the bitmap is drawn with.
 *
 * <p>Notes: called by doctor once per run; it opens no window (the JVM runs
 * headless), so it works without a desktop.
 */
public final class HostProbe {
    private HostProbe() {}

    /**
     * Answers from the host: winapk.exe registers it before calling {@link
     * #run()}, and returns {@code value * 2 + 1}.
     */
    static native int hostEcho(int value);

    /**
     * Runs every check.
     *
     * <p>Returns: one {@code key=value} line per finding, which doctor reads;
     * a check that fails says why in its value instead of throwing.
     */
    public static String run() {
        StringBuilder out = new StringBuilder();
        line(out, "javaVersion", System.getProperty("java.version"));
        line(out, "javaVendor", System.getProperty("java.vendor"));
        line(out, "osArch", System.getProperty("os.arch"));
        line(out, "vmName", System.getProperty("java.vm.name"));
        try {
            int got = hostEcho(20);
            line(out, "jni", got == 41 ? "ok" : "wrong:" + got);
        } catch (Throwable t) {
            line(out, "jni", "failed:" + t);
        }
        try {
            BufferedImage img = new BufferedImage(64, 32, BufferedImage.TYPE_INT_ARGB_PRE);
            Graphics2D g = img.createGraphics();
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
            g.setColor(new Color(0x0072B5));
            g.fillRect(0, 0, 32, 32);
            g.setColor(Color.WHITE);
            g.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 12));
            g.drawString("Aa", 36, 20);
            g.dispose();
            int fill = img.getRGB(8, 8);
            boolean text = false;
            for (int x = 34; x < 64 && !text; x++) {
                for (int y = 0; y < 32 && !text; y++) {
                    text = (img.getRGB(x, y) >>> 24) != 0;
                }
            }
            line(out, "java2d", fill == 0xFF0072B5 && text ? "ok" : String.format("wrong:fill=%08x,text=%b", fill, text));
        } catch (Throwable t) {
            line(out, "java2d", "failed:" + t);
        }
        try {
            String[] families = GraphicsEnvironment.getLocalGraphicsEnvironment().getAvailableFontFamilyNames();
            line(out, "fontFamilies", Integer.toString(families.length));
        } catch (Throwable t) {
            line(out, "fontFamilies", "failed:" + t);
        }
        line(out, "headless", Boolean.toString(GraphicsEnvironment.isHeadless()));
        return out.toString();
    }

    private static void line(StringBuilder out, String key, String value) {
        out.append(key).append('=').append(value == null ? "" : value.replace('\n', ' ')).append('\n');
    }
}
