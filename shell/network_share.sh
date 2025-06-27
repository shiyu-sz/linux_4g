#!/bin/bash

start_share() {
    ifconfig eth0 up
	ifconfig eth0 192.168.0.2
	echo 1 > /proc/sys/net/ipv4/ip_forward
	iptables -t nat -A POSTROUTING -o ppp0 -j MASQUERADE
    # 启动dnsmasq
}

stop_share() {
    iptables -t nat -D POSTROUTING -o ppp0 -j MASQUERADE
    echo 0 > /proc/sys/net/ipv4/ip_forward
}

if test "$1" = "start" ; then
    start_share
elif test "$1" = "stop" ; then
    stop_share
else
    echo "parameter error!"
fi
