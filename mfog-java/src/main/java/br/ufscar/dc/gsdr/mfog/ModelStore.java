package br.ufscar.dc.gsdr.mfog;

import akka.actor.*;
import akka.event.Logging;
import akka.event.LoggingAdapter;
import akka.io.Tcp;
import akka.io.Tcp.CommandFailed;
import akka.io.Tcp.Connected;
import akka.io.Tcp.ConnectionClosed;
import akka.io.Tcp.Received;
import akka.io.TcpMessage;
import akka.util.ByteString;

import scala.concurrent.Future;

import java.io.File;
import java.net.InetSocketAddress;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.logging.Logger;

public class ModelStore {
    static Logger LOG = Logger.getLogger(ModelStore.class.getName());
    public static void main(String[] args) {
        String jobName = ModelStore.class.getName();
        String dateString = LocalDateTime.now().format(DateTimeFormatter.ISO_DATE_TIME).replaceAll(":", "-");
        String outDir = "./out/" + jobName + "/" + dateString + "/";
        File dir = new File(outDir);
        if (!dir.exists()) {
            if (!dir.mkdirs()) throw new RuntimeException("Output directory '" + outDir +"'could not be created.");
        }
        //

        ActorSystem serverActorSystem = ActorSystem.create("ServerActorSystem");
        ActorRef serverActor = serverActorSystem.actorOf(ModelServer.props(null), "serverActor");
    }

    static class ModelServer extends AbstractActor {
        private LoggingAdapter log = Logging.getLogger(getContext().system(), this);
        private ActorRef tcpActor;

        public ModelServer(ActorRef tcpActor) {
            this.tcpActor = tcpActor;
        }

        public static Props props(ActorRef tcpActor) {
            return Props.create(ServerActor.class, tcpActor);
        }

        @Override
        public void preStart() {
            if (tcpActor == null) {
                tcpActor = Tcp.get(getContext().system()).manager();
            }

            InetSocketAddress address = new InetSocketAddress("localhost", 9090);
            Tcp.Command bind = TcpMessage.bind(getSelf(), address, 100);
            tcpActor.tell(bind, getSelf());
        }

        @Override
        public Receive createReceive() {
            return receiveBuilder()
                           .match(Tcp.Bound.class, msg -> log.info("In ServerActor - received message: bound"))
                           .match(CommandFailed.class, msg -> getContext().stop(getSelf()))
                           .match(Connected.class, msg -> {
                               log.info("In ServerActor - received message: connected");
                               final ActorRef handler = getContext().actorOf(Props.create(ModelServerHandler.class));
                               sender().tell(TcpMessage.register(handler), getSelf());
                           })
                           .build();
        }
    }
    static class ModelServerHandler extends AbstractActor {
        private LoggingAdapter log = Logging.getLogger(getContext().system(), this);

        @Override
        public Receive createReceive() {
            return receiveBuilder()
                           .match(Received.class, msg -> {
                               final String data = msg.data().utf8String();
                               log.info("In SimplisticHandlerActor - Received message: " + data);
                               final String outbound = "";
                               getSender().tell(TcpMessage.write(ByteString.fromArray(outbound.getBytes())), getSelf());
                           })
                           .match(ConnectionClosed.class, msg -> getContext().stop(getSelf()))
                           .build();
        }
    }

    public static void actorSystem() throws InterruptedException {
        ActorSystem serverActorSystem = ActorSystem.create("ServerActorSystem");

        ActorRef serverActor = serverActorSystem.actorOf(ServerActor.props(null), "serverActor");

        ActorSystem clientActorSystem = ActorSystem.create("ClientActorSystem");

        ActorRef clientActor = clientActorSystem.actorOf(ClientActor.props(
                new InetSocketAddress("localhost", 9090), null), "clientActor");

        Future<Terminated> terminatedServerFuture = serverActorSystem.whenTerminated();
        Future<Terminated> terminatedClientFuture = clientActorSystem.whenTerminated();
        boolean terminated;
        do {
            Thread.sleep(100);
            if (serverActor.isTerminated()) {
                System.out.println("serverActor.isTerminated()");
            }
            if (clientActor.isTerminated()) {
                System.out.println("clientActor.isTerminated()");
            }
            if (terminatedServerFuture.isCompleted()) {
                System.out.println("terminated Server future");
            }
            if (terminatedClientFuture.isCompleted()) {
                System.out.println("terminated Client Future");
            }
            terminated = terminatedServerFuture.isCompleted() && terminatedClientFuture.isCompleted();
        } while (!terminated);
    }

    static class ClientActor extends AbstractActor {

        final InetSocketAddress remote;
        private LoggingAdapter log = Logging.getLogger(getContext().system(), this);

        public ClientActor(InetSocketAddress remote, ActorRef tcpActor) {
            this.remote = remote;

            if (tcpActor == null) {
                tcpActor = Tcp.get(getContext().system()).manager();
            }

            tcpActor.tell(TcpMessage.connect(remote), getSelf());
        }

        public static Props props(InetSocketAddress remote, ActorRef tcpActor) {
            return Props.create(ClientActor.class, remote, tcpActor);
        }

        @Override
        public Receive createReceive() {
            return receiveBuilder()
               .match(CommandFailed.class, o -> {
                   log.info("In ClientActor - received message: failed");
                   getContext().stop(getSelf());
               })
               .match(Connected.class, msg -> {
                   log.info("In ClientActor - received message: connected");

                   sender().tell(TcpMessage.register(getSelf()), getSelf());
                   ActorRef connection = sender();
                   getContext().become(
                       receiveBuilder()
                           .match(ByteString.class, o -> connection.tell(TcpMessage.write((ByteString) o), getSelf()))
                           .match(CommandFailed.class, o -> log.info("OS kernel socket buffer was full"))
                           .match(Received.class, o -> log.info("In ClientActor - Received message: " + o.data().utf8String()))
                           .matchEquals("close", o -> connection.tell(TcpMessage.close(), getSelf()))
                           .match(ConnectionClosed.class, o -> getContext().stop(getSelf()))
                           .build()
                   );
                   sender().tell(TcpMessage.write(ByteString.fromArray("hello".getBytes())), getSelf());
               }).build();
        }
    }

    static class ServerActor extends AbstractActor {
        private LoggingAdapter log = Logging.getLogger(getContext().system(), this);
        private ActorRef tcpActor;

        public ServerActor(ActorRef tcpActor) {
            this.tcpActor = tcpActor;
        }

        public static Props props(ActorRef tcpActor) {
            return Props.create(ServerActor.class, tcpActor);
        }

        @Override
        public void preStart() throws Exception {
            if (tcpActor == null) {
                tcpActor = Tcp.get(getContext().system()).manager();
            }

            tcpActor.tell(TcpMessage.bind(getSelf(),
                    new InetSocketAddress("localhost", 9090), 100
            ), getSelf());
        }

        @Override
        public Receive createReceive() {
            return receiveBuilder()
               .match(Tcp.Bound.class, msg -> log.info("In ServerActor - received message: bound"))
               .match(CommandFailed.class, msg -> getContext().stop(getSelf()))
               .match(Connected.class, msg -> {
                   log.info("In ServerActor - received message: connected");
                   final ActorRef handler = getContext().actorOf(Props.create(SimplisticHandlerActor.class));
                   getSender().tell(TcpMessage.register(handler), getSelf());
               })
               .build();
        }
    }

    static class SimplisticHandlerActor extends AbstractActor {
        private LoggingAdapter log = Logging.getLogger(getContext().system(), this);

        @Override
        public Receive createReceive() {
            return receiveBuilder()
               .match(Received.class, msg -> {
                   final String data = msg.data().utf8String();
                   log.info("In SimplisticHandlerActor - Received message: " + data);
                   getSender().tell(TcpMessage.write(ByteString.fromArray(("echo " + data).getBytes())), getSelf());
               })
               .match(ConnectionClosed.class, msg -> getContext().stop(getSelf()))
               .build();
        }
    }
}
