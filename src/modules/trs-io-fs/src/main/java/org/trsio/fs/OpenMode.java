package org.trsio.fs;

public class OpenMode {
    final public static byte FA_READ = 0x01;
    final public static byte FA_WRITE = 0x02;
    final public static byte FA_OPEN_EXISTING = 0x00;
    final public static byte FA_CREATE_NEW = 0x04;
    final public static byte FA_CREATE_ALWAYS = 0x08;
    final public static byte FA_OPEN_ALWAYS = 0x10;
    final public static byte FA_OPEN_APPEND = 0x30;
}
