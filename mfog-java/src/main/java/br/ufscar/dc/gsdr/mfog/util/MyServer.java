package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;

public class MyServer {
    public static void main(String[] args) throws IOException, ClassNotFoundException {
        Logger log = Logger.getLogger("Server");
        ServerSocket server = new ServerSocket(15243, 0, InetAddress.getByName("localhost"));
        for (int i = 0; i < 3; i++) {
            log.info("accept");
            Socket socket = server.accept();
            log.info("outputStream");
            OutputStream outputStream = socket.getOutputStream();
            log.info("inputStream");
            InputStream inputStream = socket.getInputStream();
            //
            log.info("new writer");
            ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream));
            //
            for (int j = 0; j < 3; j++) {
                log.info("write");
                writer.writeObject(Point.zero(22));
                log.info("flush");
                writer.flush();
            }
            //
            log.info("new reader");
            ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(inputStream));
            for (int j = 0; j < 3; j++) {
                log.info("readObject");
                Point fromBytes = (Point) reader.readObject();
                log.info(fromBytes);
            }

            reader.close();
            socket.close();
        }
    }
}