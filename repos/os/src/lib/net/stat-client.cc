/*
 * \brief  Send a raw Ethernet frame
 * \author Alexander Boettcher 
 * \date   2013-04-02
 *
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>

#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/if_packet.h>

enum {
	FRAG_DONT  = 0x40,

	IP_VERSION = 4,
	IP_HEADER_LENGTH = 5, /* 5 * 4Bytes */

	PROTOCOL_IP_UDP = 17,

	PACKET_BUFFER = 4096,
 };

static unsigned short checksum(unsigned short * data, unsigned short count) {
	unsigned short result = 0;

	for (unsigned i=0; i < count; i++) {
		result += htons(data[i]);
	}

	return ~result;
}

int main(int argc, char **argv)
{
	const char * net_if_name = argv[1];
	const char * net_mac_dst = argv[2];
	struct ifreq net_if, net_mac_src;

	if (argc < 5 || strlen(net_mac_dst) < 17) {
		printf("argument missing\n");
		printf("usage: '<net_dev> <mac_addr> <packet_size> <packet_count>'"
		       " - e.g. 'eth0 2e:60:90:0c:4e:01 256 65536'\n");
		return 1;
	}

	unsigned long const packet_size  = atol(argv[3]);
	unsigned long const packet_count = atol(argv[4]);

	char packet[PACKET_BUFFER];
	memset(packet, 0, sizeof(packet));

	if (packet_size < 64 || packet_size > sizeof(packet)) {
		printf("packet size must be in the range of 64 - %lu\n",
		       sizeof(packet));
		return 1;
	}
	printf("sending %lu packets a %lu Bytes via %s network interface\n",
	       packet_count, packet_size, net_if_name);

	int const sock_fd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if (sock_fd < 0) {
		perror("socket d");
		return 2;
	}

	/* get network interface */
	memset(&net_if, 0, sizeof(net_if));
	strncpy(net_if.ifr_name, net_if_name, strlen(net_if_name));
	if (ioctl(sock_fd, SIOCGIFINDEX, &net_if)) {
		perror("ioctl SIOCGIFINDEX");
		return 3;
	}

	/* get mac of network interface */
	memset(&net_mac_src, 0, sizeof(net_mac_src));
	strncpy(net_mac_src.ifr_name, net_if_name, strlen(net_if_name));
	if (ioctl(sock_fd, SIOCGIFHWADDR, &net_mac_src)) {
		perror("ioctl SIOCGIFHWADDR");
		return 4;
	}

	struct ether_header *eh = (struct ether_header *) packet;

	/* create ethernet frame */
	memcpy(eh->ether_shost, net_mac_src.ifr_hwaddr.sa_data, ETH_ALEN);
	for (unsigned i=0; i < ETH_ALEN; i++)
		eh->ether_dhost[i] =  strtoul(&net_mac_dst[i*3], 0, 16);
	eh->ether_type = htons(ETH_P_IP);

	/* debugging */
	if (true) {
		printf("%02x", eh->ether_shost[0]);
		for (unsigned i=1; i < ETH_ALEN; i++)
			printf(":%02x", eh->ether_shost[i]);

		printf(" -> %02x", eh->ether_dhost[0]);
		for (unsigned i=1; i < ETH_ALEN; i++)
			printf(":%02x", eh->ether_dhost[i]);
		printf("\n");
	}

	/* create IP frame */
	struct iphdr *ip = (struct iphdr *) (packet + sizeof(struct ether_header));

	ip->version  = IP_VERSION;
	ip->ihl      = IP_HEADER_LENGTH;
	ip->protocol = PROTOCOL_IP_UDP;
/*
	ip->daddr    = (10U) | (65U << 24);
	ip->saddr    = (10U) | (22U << 24);
*/
	ip->tot_len  = htons(packet_size - ((char *)ip - packet));
	ip->ttl      = 10;
	ip->frag_off = FRAG_DONT;

	ip->check    = htons(checksum((unsigned short *)ip,
	                     sizeof(*ip) / sizeof(unsigned short)));
	
	/* setup UDP header */
	struct udphdr *udp = (struct udphdr *) (ip + 1);
	udp->source = htons(7321);
	udp->dest   = htons(7323);

	/* add UDP data */
	char * data = (char *) (udp + 1);
	char txt_magic[] = "Hello world! Genode is greeting.";
	memcpy(data, txt_magic, sizeof(txt_magic)); 

	int len = data + sizeof(txt_magic) - packet;
	udp->len = htons(len - ((char *)udp - packet));

	if ((len + 0UL) > sizeof(packet)) {
		printf("packet size larger then buffer %d > %lu\n", len, sizeof(packet));
		return 5;
	}

	/* send packet */
	struct sockaddr_ll socket_address;
	socket_address.sll_ifindex = net_if.ifr_ifindex;
	socket_address.sll_halen   = ETH_ALEN;
	memcpy(socket_address.sll_addr, net_mac_dst, ETH_ALEN);

	int res = sendto(sock_fd, packet, len, 0, (struct sockaddr*)&socket_address,
	                 sizeof(struct sockaddr_ll));
	if (res != len) {
		perror("sending packet - start");
		return 6;
	}
	memset(data, 0, sizeof(txt_magic));

	for (unsigned long j=0; j < packet_count;j++)
	{
		res = sendto(sock_fd, packet, packet_size, 0,
		             (struct sockaddr*)&socket_address,
		             sizeof(struct sockaddr_ll));
	}

	memcpy(data, txt_magic, sizeof(txt_magic));

	usleep (5000);
	res = sendto(sock_fd, packet, len, 0, (struct sockaddr*)&socket_address,
	             sizeof(struct sockaddr_ll));
	if (res != len) {
		perror("sending packet - end");
		return 6;
	}

	printf("send %lu packets a %lu Bytes = %lu kiBytes\n",
	       packet_count, packet_size,
	       packet_count / 1024 * packet_size);
}
