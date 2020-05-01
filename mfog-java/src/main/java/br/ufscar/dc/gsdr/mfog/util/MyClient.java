package br.ufscar.dc.gsdr.mfog.util;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.Socket;

public class MyClient {
    public static void main(String[] args) throws IOException, ClassNotFoundException {
        int nbrTry = 0;
        Logger log = Logger.getLogger("Client");
        for (int i = 0; i < 3; i++) {
            log.info("socket");
            Socket socket = new Socket(InetAddress.getByName("localhost"), 15243);
            log.info("outputStream");
            OutputStream outputStream = socket.getOutputStream();
            log.info("inputStream");
            InputStream inputStream = socket.getInputStream();
            //
            log.info("new reader");
            ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(inputStream));
            for (int j = 0; j < 3; j++) {
                log.info("readObject");
                Object fromBytes = reader.readObject();
                log.info(fromBytes);
            }
            //
            log.info("new writer");
            ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(outputStream));
            for (int j = 0; j < 3; j++) {
                log.info("writeObject");
                writer.writeObject(Point.zero(22));
                log.info("flush");
                writer.flush();
            }

            // Let user know you wrote to socket
            log.info("[Client] Hello " + nbrTry++ + " !! ");
        }
    }
}
