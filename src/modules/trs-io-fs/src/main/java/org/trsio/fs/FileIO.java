package org.trsio.fs;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SeekableByteChannel;
import java.nio.file.*;
import java.util.*;

public class FileIO {

    class FileInfo {
        FileInfo() {
            name = "";
            size = 0;
            attrib = 0;
        }

        String name;
        long size;
        byte attrib;
    }

    final static String MARKER = "TRS-IO:FS:";

    final static byte F_LOG = 99;
    final static byte F_OPEN = 0;
    final static byte F_READ = 1;
    final static byte F_WRITE = 2;
    final static byte F_CLOSE = 3;
    final static byte F_UNLINK = 4;
    final static byte F_LSEEK = 5;
    final static byte F_OPENDIR = 6;
    final static byte F_READDIR = 7;
    final static byte F_STAT = 8;

    private ChannelIO io;

    private short nextID;
    private Map<Short, SeekableByteChannel> files;
    private Map<Short, List<FileInfo>> directoryFiles;

    public FileIO(ChannelIO io) {
        this.io = io;
        nextID = 0;
        files = new HashMap<>();
        directoryFiles = new HashMap<>();
    }

    private byte readByte() {
        return io.readByte();
    }

    private short readWord() {
        return (short) (((readByte() & 0xff) | ((readByte() & 0xff) << 8)) & 0xffff);
    }

    private int readLong() {
        int wl = readWord() & 0xffff;
        int wh = readWord() & 0xffff;
        return wl | (wh << 16);
    }

    private String readString() {
        StringBuilder s = new StringBuilder();
        while(true) {
            byte b = readByte();
            if (b == 0) {
                return s.toString();
            }
            s.append((char) (b & 0xff));
        }
    }

    private void writeString(String str) {
        for (byte b : str.getBytes()) {
            writeByte(b);
        }
        writeByte( (byte)0);
    }

    private void writeByte(byte b) {
        io.writeByte(b);
    }

    private void writeWord(int word) {
        writeByte((byte) (word & 0xff));
        writeByte((byte) ((word >> 8) & 0xff));
    }

    private void writeLong(long l) {
        writeWord((int) (l & 0xffff));
        writeWord((int) ((l >> 16) & 0xffff));
    }

    private void scanForMarker() {
        int markerPos = 0;

        while (true) {
            byte b = io.readByte();
            if (b != MARKER.charAt(markerPos)) {
                markerPos = MARKER.charAt(0) == b ? 1 : 0;
                continue;
            }
            markerPos++;
            if (markerPos == MARKER.length()) {
                return;
            }
        }
    }

    public void start() {
        while(true) {
            scanForMarker();
            switch (io.readByte()) {
                case F_LOG:
                    doLog();
                    break;
                case F_OPEN:
                    doOpen();
                    break;
                case F_READ:
                    doRead();
                    break;
                case F_WRITE:
                    doWrite();
                    break;
                case F_CLOSE:
                    doClose();
                    break;
                case F_UNLINK:
                    doUnlink();
                    break;
                case F_LSEEK:
                    doLseek();
                    break;
                case F_OPENDIR:
                    doOpenDir();
                    break;
                case F_READDIR:
                    doReadDir();
                    break;
                case F_STAT:
                    doStat();
                    break;
            }
        }
    }

    private void doLog() {
        String log = readString();
        System.out.println(log);
        writeByte(FRESULT.FR_OK);
    }

    private void doOpen() {
        byte mode = readByte();
        String path = readString();
        Set<StandardOpenOption> openOptions = new HashSet<>();
        if ((mode & OpenMode.FA_READ) != 0) {
            openOptions.add(StandardOpenOption.READ);
            mode &= ~OpenMode.FA_READ;
        }
        if ((mode & OpenMode.FA_WRITE) != 0) {
            openOptions.add(StandardOpenOption.WRITE);
            mode &= ~OpenMode.FA_WRITE;
        }
        if ((mode & OpenMode.FA_CREATE_ALWAYS) != 0) {
            openOptions.add(StandardOpenOption.CREATE);
            mode &= ~OpenMode.FA_CREATE_ALWAYS;
            if ((mode & OpenMode.FA_OPEN_APPEND) == 0) {
                openOptions.add(StandardOpenOption.TRUNCATE_EXISTING);
            }
        }
        if ((mode & OpenMode.FA_CREATE_NEW) != 0) {
            openOptions.add(StandardOpenOption.CREATE_NEW);
            mode &= ~OpenMode.FA_CREATE_NEW;
        }
        if ((mode & OpenMode.FA_OPEN_APPEND) != 0) {
            openOptions.add(StandardOpenOption.APPEND);
            mode &= ~OpenMode.FA_OPEN_APPEND;
        }
        if (mode != 0) {
            writeByte(FRESULT.FR_INVALID_PARAMETER);
            return;
        }
        try {
            SeekableByteChannel channel= Files.newByteChannel(Paths.get(path), openOptions);
            short id = nextID++;
            files.put(id, channel);
            writeByte(FRESULT.FR_OK);
            writeWord(id);
        } catch (IOException e) {
            writeByte(FRESULT.FR_NO_FILE);
        }
    }

    private SeekableByteChannel getChannel() {
        short id = readWord();
        if (!files.containsKey(id)) {
            writeByte(FRESULT.FR_INVALID_OBJECT);
            return null;
        }
        return files.get(id);
    }

    private void doRead() {
        SeekableByteChannel channel = getChannel();
        if (channel == null) {
            return;
        }
        int btr = readWord() & 0xffff;
        ByteBuffer data = ByteBuffer.allocate(btr);
        writeByte(FRESULT.FR_OK);
        try {
            int read = 0;
            while (read != btr) {
                int br = channel.read(data);
                if (br == -1) {
                    break;
                }
                read += br;
            }
            data.rewind();
            io.writeBlob(Arrays.copyOfRange(data.array(), 0, read));
        } catch (IOException e) {
            writeWord(0);
        }
    }

    private void doWritex() {
        SeekableByteChannel channel = getChannel();
        if (channel == null) {
            return;
        }
        byte[] data = io.readBlob();
        writeByte(FRESULT.FR_OK);
        try {
            int bw = channel.write(ByteBuffer.wrap(data));
            writeWord(bw);
        } catch (IOException e) {
            writeWord(0);
        }
    }

    private void doReadx() {
        SeekableByteChannel channel = getChannel();
        if (channel == null) {
            return;
        }
        int btr = readWord() & 0xffff;
        ByteBuffer data = ByteBuffer.allocate(btr);
        writeByte(FRESULT.FR_OK);
        try {
            int br = channel.read(data);
            writeWord(br);
            for (int i = 0; i < br; i++) {
                writeByte(data.get(i));
            }
        } catch (IOException e) {
            writeWord(0);
        }
    }

    private void doWrite() {
        SeekableByteChannel channel = getChannel();
        if (channel == null) {
            return;
        }
        int btw = readWord() & 0xffff;
        ByteBuffer data = ByteBuffer.allocate(btw);
        for (int i = 0; i < btw; i++) {
            data.put(readByte());
        }
        data.rewind();
        writeByte(FRESULT.FR_OK);
        try {
            int bw = channel.write(data);
            writeWord(bw);
        } catch (IOException e) {
            writeWord(0);
        }
    }

    private void doClose() {
        SeekableByteChannel channel = getChannel();
        if (channel == null) {
            return;
        }
        try {
            channel.close();
        } catch (IOException e) {
        }
        writeByte(FRESULT.FR_OK);
    }

    private void doUnlink() {
        String path = readString();
        File file = new File(path);
        byte fr = file.delete() ? FRESULT.FR_OK : FRESULT.FR_NO_FILE;
        writeByte(fr);
    }

    private void doLseek() {
        SeekableByteChannel channel = getChannel();
        if (channel == null) {
            return;
        }
        int ofs = readLong();
        try {
            channel.position(ofs);
        } catch (IOException e) {
        }
        writeByte(FRESULT.FR_OK);
    }

    final private static byte AM_RDO = 0x01;
    final private static byte AM_HID = 0x02;
    final private static byte AM_SYS = 0x04;
    final private static byte AM_VOL = 0x08;
    final private static byte AM_DIR = 0x10;

    private void doOpenDir() {
        String directory = readString();
        //XXX
        directory = ".";
        List<FileInfo> fileInfos = new ArrayList<>();
        short id = nextID++;
        directoryFiles.put(id, fileInfos);

        try (DirectoryStream<Path> directoryStream = Files.newDirectoryStream(Paths.get(directory))) {
            for (Path path : directoryStream) {
                String p = path.getFileName().toString();
                if (p.equals(".") || p.equals("..")) {
                    continue;
                }
                // Filter out non-8.3 file names
                int i = p.indexOf('.');
                if (i == -1 && p.length() > 12) {
                    continue;
                }
                if (i > 8 || (i != -1 && p.length() - i > 4)) {
                    continue;
                }
                FileInfo fileInfo = new FileInfo();
                fileInfo.name = p;
                fileInfo.size = Files.size(path);
                fileInfo.attrib = getFileAttributes(path);
                fileInfos.add(fileInfo);
            }
        } catch (IOException ex) {}
        writeByte(FRESULT.FR_OK);
        writeWord(id);
    }

    private byte getFileAttributes(Path path) {
        byte attrib = 0;
        if (Files.isDirectory(path)) {
            attrib |= AM_DIR;
        }
        try {
            if (Files.isHidden(path)) {
                attrib |= AM_HID;
            }
        } catch (IOException e) {
        }
        if (!Files.isWritable(path)) {
            attrib |= AM_RDO;
        }
        return attrib;
    }

    private void doReadDir() {
        short id = readWord();
        if (!directoryFiles.containsKey(id)) {
            writeByte(FRESULT.FR_INVALID_OBJECT);
            return;
        }
        List<FileInfo> fileInfos = directoryFiles.get(id);
        FileInfo fileInfo;

        if (fileInfos.isEmpty()) {
            directoryFiles.remove(id);
            fileInfo = new FileInfo();
        } else {
            fileInfo = fileInfos.get(0);
            fileInfos.remove(0);
        }
        writeByte(FRESULT.FR_OK);
        writeString(fileInfo.name);
        writeLong(fileInfo.size);
        writeByte(fileInfo.attrib);
    }

    private void doStat() {
        Path path = Paths.get(readString());
        if (!Files.exists(path)) {
            writeByte(FRESULT.FR_NO_FILE);
            return;
        }

        long size = 0;
        try {
            size = Files.size(path);
        } catch (IOException e) {
            writeByte(FRESULT.FR_NO_FILE);
            return;
        }

        writeByte(FRESULT.FR_OK);
        writeLong(size);
        writeByte(getFileAttributes(path));
    }
}
