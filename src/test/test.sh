#!/bin/bash
gcc -g -Wall -pthread src/test/circ-buff.c -o bin/cir
for i in {1..100}; do ./bin/cir $i $i $i || break; done;
