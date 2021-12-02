SRC_CC += ethernet.cc ipv4.cc dhcp.cc arp.cc udp.cc tcp.cc
SRC_CC += icmp.cc internet_checksum.cc

CC_CXX_WARN_STRICT_CONVERSION =

vpath %.cc $(REP_DIR)/src/lib/net
