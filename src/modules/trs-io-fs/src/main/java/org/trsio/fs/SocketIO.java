package org.trsio.fs;

import java.io.*;
import java.net.ServerSocket;
import java.net.Socket;

public class SocketIO extends BaseIO implements ChannelIO {

    public SocketIO() {
        in = null;
        out = null;
        ServerSocket serverSocket = null;
        try {
            serverSocket = new ServerSocket(8088);
            Socket clientSocket = serverSocket.accept();
            out = clientSocket.getOutputStream();
            in = clientSocket.getInputStream();
        } catch (IOException e) {
            in = null;
            out = null;
        }
    }
}
