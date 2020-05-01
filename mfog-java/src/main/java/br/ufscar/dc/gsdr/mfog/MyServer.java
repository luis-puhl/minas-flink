package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Point;

import java.io.*;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;

public class MyServer implements Runnable {
  public static void main(String[] args) throws IOException, ClassNotFoundException {
    ServerSocket server = new ServerSocket(15243, 0, InetAddress.getByName("localhost"));
    while (true) {
      System.out.println("accept");
      Socket socket = server.accept();

      // Write to client to tell him you are waiting.
      Object zero = new Serializable() {
        int campo = 22;
        @Override
        public int hashCode() {
          return super.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
          return super.equals(obj);
        }

        @Override
        protected Object clone() throws CloneNotSupportedException {
          return super.clone();
        }

        @Override
        public String toString() {
          return super.toString();
        }
      };
      System.out.println("new writer");
      ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
      System.out.println("write");
      writer.writeObject(Point.zero(22));
      System.out.println("flush");
      writer.flush();
//      System.out.println("close");
//      writer.close();
      System.out.println("new reader");
      ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));
      System.out.println("readObject");
      Object fromBytes = reader.readObject();
      System.out.println(fromBytes);

      reader.close();
      socket.close();
    }
  }

  public MyServer() throws IOException {

  }

  @Override
  public void run() {

  }
}