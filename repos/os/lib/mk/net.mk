SRC_CC += ethernet.cc ipv4.cc dhcp.cc arp.cc udp.cc tcp.cc mac_address.cc
SRC_CC += icmp.cc

vpath %.cc $(REP_DIR)/src/lib/net
