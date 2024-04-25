/*
 * \brief  Test the reachability of a host on an IP network
 * \author Martin Stein
 * \date   2018-03-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/ipv4.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/internet_checksum.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <base/attached_rom_dataspace.h>
#include <os/vfs.h>

using namespace Net;
using namespace Genode;

#define ASSERT(condition) \
	do { \
		if (!(condition)) { \
			Genode::error(__FILE__, ":", __LINE__, ": ", " assertion \"", #condition, "\" failed "); \
			Genode::sleep_forever(); \
		} \
	} while (false)

#define ASSERT_NEVER_REACHED \
	do { \
		Genode::error(__FILE__, ":", __LINE__, ": ", " should have never been reached"); \
		Genode::sleep_forever(); \
	} while (false)


struct Parser
{
	char *start;
	size_t num_bytes;

	void advance_by(size_t num_bytes)
	{
		ASSERT(num_bytes >= num_bytes);
		start += num_bytes;
		num_bytes -= num_bytes;
	}

	template <typename T>
	T const &fetch()
	{
		ASSERT(num_bytes >= sizeof(T));
		T const &obj = *(T const *)start;
		advance_by(sizeof(T));
		return obj;
	}

	void fetch(Byte_range_ptr const &dst)
	{
		ASSERT(num_bytes >= dst.num_bytes);
		memcpy(dst.start, start, dst.num_bytes);
		advance_by(dst.num_bytes);
	}
};


struct Pseudo_random_number_generator
{
	uint64_t seed;

	uint8_t random_byte()
	{
		seed = (16807 * seed) % 2147483647;
		return (uint8_t)seed;
	}
};


struct Pcap_file_header
{
	static constexpr uint32_t MAGIC_NUMBER = 0xA1B2C3D4;

	uint32_t magic_number;
	uint32_t unused[5];

	bool valid() const { return magic_number == MAGIC_NUMBER; }
};


struct Pcap_packet_record
{
	uint32_t unused_0[2];
	uint32_t captured_pkt_len;
	uint32_t original_pkt_len;
};


struct Main
{
	Env &env;
	Heap heap { env.ram(), env.rm() };
	Attached_rom_dataspace config_rom { env, "config" };
	Root_directory  root { env, heap, config_rom.xml().sub_node("vfs") };
	Reconstructible<Append_file> pcap_file { root, "/output.pcap" };
	Attached_rom_dataspace pcap_rom { env, "input.pcap" };
	Parser pcap_parser { pcap_rom.local_addr<char>(), pcap_rom.size() };
	unsigned long num_errors = 0;
	unsigned long num_packets = 0;
	unsigned long num_ip4_checksums = 0;
	unsigned long num_udp_checksums = 0;
	unsigned long num_tcp_checksums = 0;
	unsigned long num_icmp_checksums = 0;
	Pseudo_random_number_generator prng { config_rom.xml().attribute_value("seed", 0ULL) };

	Main(Env &env);

	void check_tcp(Tcp_packet &tcp, Ipv4_packet &ip, size_t tcp_size);

	void check_udp(Udp_packet &udp, Ipv4_packet &ip);

	void check_icmp(Icmp_packet &icmp, size_t icmp_size);

	void check_ip4(Ipv4_packet &ip);

	void modify_ip4(Ipv4_packet &ip, Internet_checksum_diff &ip_icd);

	void check_recalculated_checksum(char const *prot, uint16_t got_checksum, uint16_t expect_checksum)
	{
		if (got_checksum != expect_checksum) {
			error("frame ", num_packets + 1, ": ", prot, ": re-calculating initial checksum failed (got ",
			      Hex(got_checksum)," expected ", Hex(expect_checksum), " diff ", expect_checksum - got_checksum, ")");
			num_errors++;
		}
	}

	void validate_initial_checksum_error(char const *prot)
	{
		error("frame ", num_packets + 1, ": ", prot, ": validating initial checksum failed");
		num_errors++;
	}
};


void Main::check_icmp(Icmp_packet &icmp, size_t icmp_size)
{
	uint16_t initial_checksum = icmp.checksum();
	size_t l5_size = icmp_size - sizeof(Icmp_packet);
	if (icmp.checksum_error(l5_size))
		validate_initial_checksum_error("icmp");
	icmp.update_checksum(l5_size);
	check_recalculated_checksum("icmp", icmp.checksum(), initial_checksum);
	num_icmp_checksums++;
}


void Main::check_udp(Udp_packet &udp, Ipv4_packet &ip)
{
	uint16_t initial_checksum = udp.checksum();
	if (udp.checksum_error(ip.src(), ip.dst()))
		validate_initial_checksum_error("udp");
	udp.update_checksum(ip.src(), ip.dst());
	check_recalculated_checksum("udp", udp.checksum(), initial_checksum);
	num_udp_checksums++;
}


void Main::check_tcp(Tcp_packet &tcp, Ipv4_packet &ip, size_t tcp_size)
{
	uint16_t initial_checksum = tcp.checksum();
	tcp.update_checksum(ip.src(), ip.dst(), tcp_size);
	check_recalculated_checksum("tcp", tcp.checksum(), initial_checksum);
	num_tcp_checksums++;
}


void Main::check_ip4(Ipv4_packet &ip)
{
	uint16_t initial_checksum = ip.checksum();
	if (ip.checksum_error())
		validate_initial_checksum_error("ip");
	ip.update_checksum();
	check_recalculated_checksum("ip", ip.checksum(), initial_checksum);
	num_ip4_checksums++;
}


void Main::modify_ip4(Ipv4_packet &ip, Internet_checksum_diff &ip_icd)
{
	Ipv4_address ip_src = ip.src();
	ip_src.addr[0] &= prng.random_byte();
	ip_src.addr[1] |= prng.random_byte();
	ip_src.addr[2] *= prng.random_byte();
	ip_src.addr[3] += prng.random_byte();
	ip.src(ip_src, ip_icd);

	if (prng.random_byte() & 0b11)
		return;

	Ipv4_address ip_dst = ip.dst();
	ip_dst.addr[0] |= prng.random_byte();
	ip_dst.addr[1] += prng.random_byte();
	ip_dst.addr[2] &= prng.random_byte();
	ip_dst.addr[3] *= prng.random_byte();
	ip.dst(ip_dst, ip_icd);
}


Main::Main(Env &env) : env(env)
{
	using Append_result = Append_file::Append_result;
	static constexpr size_t BUF_SIZE = 1024;
	char buf[BUF_SIZE];

	Pcap_file_header const &header = pcap_parser.fetch<Pcap_file_header>();
	ASSERT(header.valid());
	ASSERT(pcap_file->append((char *)&header, sizeof(header)) == Append_result::OK);

	while(1) {
		if (pcap_parser.num_bytes < sizeof(Pcap_packet_record))
			break;

		Pcap_packet_record const &record = pcap_parser.fetch<Pcap_packet_record>();
		if (!record.captured_pkt_len)
			break;

		ASSERT(pcap_file->append((char *)&record, sizeof(record)) == Append_result::OK);
		ASSERT(record.captured_pkt_len == record.original_pkt_len);
		ASSERT(record.captured_pkt_len <= BUF_SIZE);
		pcap_parser.fetch({buf, record.captured_pkt_len});

		Size_guard size_guard(record.captured_pkt_len);
		Ethernet_frame &eth = Ethernet_frame::cast_from(buf, size_guard);
		ASSERT(eth.type() == Ethernet_frame::Type::IPV4);
		Ipv4_packet &ip = eth.data<Ipv4_packet>(size_guard);
		check_ip4(ip);
		size_t l4_size = ip.total_length() - sizeof(Ipv4_packet);
		switch (ip.protocol()) {
		case Ipv4_packet::Protocol::TCP: check_tcp(ip.data<Tcp_packet>(size_guard), ip, l4_size); break;
		case Ipv4_packet::Protocol::UDP: check_udp(ip.data<Udp_packet>(size_guard), ip); break;
		case Ipv4_packet::Protocol::ICMP: check_icmp(ip.data<Icmp_packet>(size_guard), l4_size); break;
		default: break;
		}
		Internet_checksum_diff ip_icd { };
		modify_ip4(ip, ip_icd);
		switch (ip.protocol()) {
		case Ipv4_packet::Protocol::TCP: ip.data<Tcp_packet>(size_guard).update_checksum(ip.src(), ip.dst(), l4_size); break;
		case Ipv4_packet::Protocol::UDP: ip.data<Udp_packet>(size_guard).update_checksum(ip.src(), ip.dst()); break;
		case Ipv4_packet::Protocol::ICMP: ip.data<Icmp_packet>(size_guard).update_checksum(l4_size - sizeof(Icmp_packet)); break;
		default: break;
		}
		ip.update_checksum(ip_icd);
		ip.checksum(ip.checksum());
		ASSERT(pcap_file->append((char *)&eth, record.captured_pkt_len) == Append_result::OK);
		num_packets++;
	}
	unsigned long num_checksums = num_ip4_checksums + num_udp_checksums + num_tcp_checksums + num_icmp_checksums;
	log("checked ", num_checksums, " checksum", num_checksums == 1 ? "" : "s",
	    " (ip4 ", num_ip4_checksums, " tcp ", num_tcp_checksums," udp ", num_udp_checksums, " icmp ", num_icmp_checksums,
	    ") in ", num_packets, " packet", num_packets == 1 ? "" : "s", " with ", num_errors, " error", num_errors == 1 ? "" : "s");

	pcap_file.destruct();
	env.parent().exit(num_errors ? -1 : 0);
}


void Component::construct(Env &env) { static Main main(env); }
