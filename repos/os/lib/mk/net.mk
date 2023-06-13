SRC_CC += ethernet.cc ipv4.cc dhcp.cc arp.cc udp.cc tcp.cc
SRC_CC += icmp.cc internet_checksum.cc

vpath %.cc $(REP_DIR)/src/lib/net
