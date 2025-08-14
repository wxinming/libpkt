#!/bin/bash


if [ "$1" = "-all" ]; then
    echo "build all"
    g++ -c libpkt.cpp -o libpkt.o
    ar rcs libpkt.a libpkt.o
fi

g++ main.cpp -o a.out -L. -lpkt

./a.out
