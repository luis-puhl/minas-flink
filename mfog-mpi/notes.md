
- N0-T0: dispach
    - 2: classify
    - 3: classify
    - ...
    - N: classify
- N0-T1: recv match -> ND


$ sudo vi /etc/resolv.conf 

...

nameserver 200.18.99.1
# nameserver 127.0.0.53
...
$ sudo ip route add default via 192.168.0.1