#!/bin/bash

brctl addbr br0
tunctl -u root

ifconfig eth0 0.0.0.0 up
ifconfig tap0 0.0.0.0 up

brctl addif br0 eth0
brctl addif br0 tap0

ifconfig br0 141.223.83.13 netmask 255.255.255.0 broadcast 141.223.83.255
route add default gw 141.223.83.99 
