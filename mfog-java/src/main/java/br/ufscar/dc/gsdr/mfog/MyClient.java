package br.ufscar.dc.gsdr.mfog;

import br.ufscar.dc.gsdr.mfog.structs.Point;
import com.esotericsoftware.kryo.Kryo;

import java.io.*;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;

public class MyClient implements Runnable {
  public static void main(String[] args) throws IOException, ClassNotFoundException {
    int nbrTry = 0;
    while (true) {
        System.out.println("socket");
        Socket socket = new Socket(InetAddress.getByName("localhost"), 15243);
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
        System.out.println("reader");
        ObjectInputStream reader = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));
        System.out.println("readUTF");
        Object fromBytes = reader.readObject();
        System.out.println(fromBytes);
//        System.out.println("reader.close()");
//        reader.close();
        System.out.println("writer");
        ObjectOutputStream writer = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
        System.out.println("writeObject");
        writer.writeObject(Point.zero(22));
        System.out.println("flush");
        writer.flush();

        // Let user know you wrote to socket
        System.out.println("[Client] Hello " + nbrTry++ + " !! ");
    }
  }

  Socket socket;
  PrintWriter out;
  BufferedReader in;

  public MyClient() throws UnknownHostException, IOException {
  }

  @Override
  public void run() {

  }
}
