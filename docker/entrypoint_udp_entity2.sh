#!/bin/sh
iptables -t nat -A OUTPUT -d 192.168.1.11 -p udp --dport 5001 -j DNAT --to-destination 192.168.1.40:5001
exec "$@"
