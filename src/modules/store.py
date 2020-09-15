import socket
import sys
import select

if __name__ == "__main__":
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_addr = ("0.0.0.0", 7000)
    server_socket.bind(server_addr)
    server_socket.listen()
    print('Listening for connections on {}:{}'.format(*server_addr))
    sockets_list = [server_socket]
    connections = {} #[{'addr': server_addr, 'sock': server_socket, 'ptr': 0}]

    model = []
    isOnline = True

    while isOnline:
        read_sockets, write_sockets, exception_sockets = select.select(sockets_list, sockets_list, sockets_list)
        #
        for sndSock in write_sockets:
            if sndSock not in connections:
                continue
            if connections[sndSock]['index'] < len(model):
                try:
                    sndSock.send(model[connections[sndSock]['index']].encode('utf-8'))
                    connections[sndSock]['index'] += 1
                    # print('.', end='', flush=True, file=sys.stderr)
                    # if connections[sndSock]['index'] == len(model):
                    #     print('done', flush=True, file=sys.stderr)
                except ConnectionError as connErr:
                    print(connErr, 'Closed connection from {}:{}'.format(*connections[sndSock]['addr']))
                    sndSock.close()
                    del connections[sndSock]
                    sockets_list.remove(sndSock)
        #
        for expSock in exception_sockets:
            if expSock not in connections:
                continue
            del connections[expSock]
            sockets_list.remove(expSock)
            print('Exception on connection {}:{}'.format(*connections[expSock]['addr']))
        #
        for rdSock in read_sockets:
            sock = rdSock
            if sock == server_socket:
                client_socket, client_address = server_socket.accept()
                if client_socket is False:
                    continue
                sockets_list.append(client_socket)
                connections[client_socket] = {'addr': client_address, 'sock': client_socket, 'index': 0}
                sock = client_socket
                # print('connections', connections)
                print('Accepted new connection from {}:{}'.format(*connections[sock]['addr']), flush=True)
                continue
            #
            if sock not in connections:
                continue
            message = ""
            # isMessageComplete = False
            while len(message) == 0 or message[-1] != '\n':
                try:
                    packet = sock.recv(256)
                except ConnectionError as connErr:
                    print(connErr, 'Closed connection from {}:{}'.format(*connections[sock]['addr']))
                    sock.close()
                    del connections[sock]
                    sockets_list.remove(sock)
                    break
                if message is False:
                    print('Closed connection from {}:{}'.format(*connections[sock]['addr']))
                    del connections[sock]
                    sockets_list.remove(sock)
                    continue
                if len(packet) == 0:
                    break
                message += packet.decode('utf-8')
            #
            if len(message) == 0:
                continue
            # print("Received message '{}' from {}:{}".format(message, *connections[sock]['addr']))
            if message == 'q\n':
                isOnline = False
                continue
            if message.endswith('\n\n'):
                message = message.replace('\n\n', '\n')
                print('Closed connection from {}:{}'.format(*connections[sock]['addr']))
                sock.close()
                del connections[sock]
                sockets_list.remove(sock)
            model += message.splitlines(keepends=True)


    for sock in sockets_list:
        if sock not in connections:
            continue
        print('Closed connection {}:{}'.format(*connections[sock]['addr']))
        sockets_list.remove(sock)
        del connections[sock]
        sock.close()
    server_socket.close()
    print('Closed server')
