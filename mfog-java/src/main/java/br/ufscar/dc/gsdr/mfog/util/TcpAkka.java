package br.ufscar.dc.gsdr.mfog.util;

import akka.NotUsed;
import akka.actor.ActorSystem;
import akka.stream.*;
import akka.stream.javadsl.*;
import akka.stream.javadsl.Tcp.IncomingConnection;
import akka.util.ByteString;

import java.util.concurrent.CompletionStage;

public class TcpAkka {
    public static void main(String[] args) {
        ActorSystem system = ActorSystem.create("StreamTcpDocTest");
        ActorMaterializer materializer = ActorMaterializer.create(system);
        // IncomingConnection and ServerBinding imported from Tcp
        final Source<IncomingConnection, CompletionStage<Tcp.ServerBinding>> connections = Tcp.get(system).bind("127.0.0.1", 8888);
        // #echo-server-simple-handle
        connections.runForeach(
                connection -> {
                    System.out.println("New connection from: " + connection.remoteAddress());
                    Flow<ByteString, ByteString, NotUsed> delimiter = Framing.delimiter(ByteString.fromString("\n"), 256, FramingTruncation.DISALLOW);

                    final Flow<ByteString, ByteString, NotUsed> echo =
                            (Flow<ByteString, ByteString, NotUsed>) Flow.of(ByteString.class)
                                    .via(delimiter)
                                    .map(ByteString::utf8String)
                                    // .map(s -> s + "!!!\n")
                                    .map(ByteString::fromString);

                    connection.handleWith(echo, materializer);
                },
                materializer);
        /*
        final Flow<ByteString, ByteString, CompletionStage<Tcp.OutgoingConnection>> connection =
                Tcp.get(system).outgoingConnection("127.0.0.1", 8888);
        final Flow<String, ByteString, NotUsed> replParser =
                (Flow<String, ByteString, NotUsed>) Flow.<String>create()
                        .takeWhile(elem -> !elem.equals("q"))
                        .concat(Source.single("BYE")) // will run after the original flow completes
                        .map(elem -> ByteString.fromString(elem + "\n"));

        final Flow<ByteString, ByteString, NotUsed> repl =
                Flow.of(ByteString.class)
                        .via(Framing.delimiter(ByteString.fromString("\n"), 256, FramingTruncation.DISALLOW))
                        .map(ByteString::utf8String)
                        .map(
                                text -> {
                                    System.out.println("Server: " + text);
                                    return "next";
                                })
                        .map(elem -> readLine("> "))
                        .via((Graph<FlowShape<String, ByteString>, NotUsed>) replParser);

        CompletionStage<Tcp.OutgoingConnection> connectionCS = connection.join(repl).run(system);
         */
    }
    /*
    private static String readLine(String prompt) {
        String s = input.poll();
        return (s == null ? "q" : s);
    }
     */
}
