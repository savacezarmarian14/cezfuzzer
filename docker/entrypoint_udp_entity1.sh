#!/bin/sh
iptables -t nat -A OUTPUT -d 192.168.1.10 -p udp --dport 5000 -j DNAT --to-destination 192.168.1.40:5000
exec "$@"
