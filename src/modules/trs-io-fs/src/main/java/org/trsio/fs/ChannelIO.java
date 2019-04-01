package org.trsio.fs;

public interface ChannelIO {
    byte readByte();
    byte[] readBlob();
    void writeByte(byte b);
    void writeBlob(byte[] data);
}
