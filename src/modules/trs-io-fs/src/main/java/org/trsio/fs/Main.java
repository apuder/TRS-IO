package org.trsio.fs;


public class Main {

    public static void main(String[] args) {
        if (args.length != 1) {
            System.err.println("Usage: trs-io-fs <serial-port>");
            System.exit(-1);
        }
        //SocketIO channel = new SocketIO();
        SerialIO channel = new SerialIO(args[0]);
        if (!channel.isOpen()) {
            System.err.println("Could not open " + args[0]);
            System.exit(-1);
        }
        FileIO fileIO = new FileIO(channel);
        fileIO.start();
    }
}
