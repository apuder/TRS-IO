package org.trsio.fs;


import com.fazecast.jSerialComm.SerialPort;

public class SerialIO extends BaseIO implements ChannelIO {

    public SerialIO(String portName) {
        connect(portName);
    }

    private void connect(String portName) {
        SerialPort comPort = SerialPort.getCommPort(portName);
        comPort.setBaudRate(115200);
        comPort.setNumDataBits(8);
        comPort.setNumStopBits(1);
        comPort.setParity(SerialPort.NO_PARITY);
        comPort.openPort();
        comPort.setComPortTimeouts(SerialPort.TIMEOUT_READ_SEMI_BLOCKING, 0, 0);
        in = comPort.getInputStream();
        out = comPort.getOutputStream();
    }
}
