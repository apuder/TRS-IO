package org.trsio.fs;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;

public class BaseIO {

    protected OutputStream out;
    protected InputStream in;

    public boolean isOpen() {
        return (in != null) && (out != null);
    }

    public byte readByte() {
        try {
            while (true) {
                int b = in.read();
                if (b == -1) {
                    continue;
                }
                return (byte) b;
            }
        } catch (IOException e) {
            return 0;
        }
    }

    public byte[] readBlob() {
        int btr = ((readByte() & 0xff) | ((readByte() & 0xff) << 8)) & 0xffff;
        byte[] data = new byte[btr];
        try {
            int read = 0;
            while (read != btr) {
                int br = in.read(data, read, btr - read);
                if (br == -1) {
                    //TODO actually, this shouldn't happen since the client is expected to send that many bytes
                    return Arrays.copyOfRange(data, 0, read);
                }
                read += br;
            }
        } catch (IOException e) {
            return new byte[0];
        }
        return data;
    }

    public void writeByte(byte b) {
        try {
            out.write((int) b & 0xff);
            out.flush();
        } catch (IOException e) {
        }
    }

    public void writeBlob(byte[] data) {
        int len = data.length;
        writeByte((byte) (len & 0xff));
        writeByte((byte) ((len >> 8) & 0xff));
        try {
            out.write(data);
            out.flush();
        } catch (IOException e) {
        }
    }
}
