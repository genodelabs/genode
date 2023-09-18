/*
 * \brief  Dns protocol test
 * \author Alice Domage <alice.domage@gapfruit.com>
 * \date   2023-09-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 * Copyright (C) 2023 gapfruit ag
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/exception.h>
#include <base/env.h>
#include <net/dns.h>

namespace Dns_test {

	using namespace Genode;

	struct Test_failure: Exception { };

	void test_domain_name_format();
	void test_dns_request();
	void test_dns_response();
	void test_dns_malformated_response();

}


void Dns_test::test_domain_name_format()
{
	log("----- ", __FUNCTION__, " -----");

	Net::Domain_name dn;

	Net::ascii_to("pool.ntp.org", dn);
	if (dn == Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("Valide domain name: ", dn); }

	Net::ascii_to("genode-is.cool.org", dn);
	if (dn == Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("Valide domain name: ", dn); }

	Net::ascii_to("genode-is42.cool.org", dn);
	if (dn == Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("Valide domain name: ", dn); }

	/* clear dn var before proceeding with invalide domains tests */
	dn = Net::Domain_name();

	Net::ascii_to("wrong", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("wrong: is an invalide domain name"); }

	Net::ascii_to("-.wrong", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("-.wrong: is an invalide domain name"); }

	Net::ascii_to("-abc.wrong", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("-abc.wrong: is an invalide domain name"); }

	Net::ascii_to("abc-.wrong", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("abc-.wrong: is an invalide domain name"); }

	Net::ascii_to("6abc.wrong", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure(); }
	else {
		log("6abc.wrong: is an invalide domain name"); }

	Net::ascii_to("test..wrong", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("test..wrong: is an invalide domain name"); }

	Net::ascii_to("tooshort.a", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("tooshort.a: is an invalide domain name"); }

	Net::ascii_to("toolong.abcdefghijglmn", dn);
	if (dn != Net::Domain_name()) {
		throw Test_failure();
	} else {
		log("toolong.abcdefghijglmn: is an invalide domain name"); }
}

void Dns_test::test_dns_request()
{
	log("----- ", __FUNCTION__, " -----");

	const uint8_t CAPTURED_DATAGRAM[] = {
		0xE4, 0x45, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x04, 0x70, 0x6f, 0x6f, 0x6C, 0x03, 0x6e, 0x74, 0x70, 0x03,
		0x6f, 0x72, 0x67, 0x00, 0x00, 0x01, 0x00, 0x01
	};

	uint8_t DATAGRAM[sizeof(CAPTURED_DATAGRAM)] = {};
	Net::Size_guard size_guard { sizeof(DATAGRAM) };


	Net::Domain_name dn { "pool.ntp.org" };

	if (sizeof(Net::Dns_packet) + Net::Dns_packet::sizeof_question(dn) != sizeof(CAPTURED_DATAGRAM)) {
		error("Datagram size for the given DNS request is incorrect!");
		throw Test_failure(); }

	auto *dns = Genode::construct_at<Net::Dns_packet>(DATAGRAM);
	size_guard.consume_tail(sizeof(Net::Dns_packet));
	dns->id(0xe445);
	dns->recursion_desired(true);
	dns->question(size_guard, dn);

	if (memcmp(CAPTURED_DATAGRAM, DATAGRAM, sizeof(DATAGRAM)) != 0) {
		throw Test_failure();
	}
	log("DNS request successfully created");
}


void Dns_test::test_dns_response()
{
	log("----- ", __FUNCTION__, " -----");

	uint8_t CAPTURED_DATAGRAM[] = {
		0xe4, 0x45, 0x81, 0x80, 0x00, 0x01, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00, 0x04, 0x70, 0x6f, 0x6f,
		0x6c, 0x03, 0x6e, 0x74, 0x70, 0x03, 0x6f, 0x72,
		0x67, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xc0, 0x21, 0xd6, 0x2f, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xd4, 0x33, 0x90, 0x2e, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xd4, 0xf3, 0x60, 0x4c, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xc1, 0x21, 0x1e, 0x27
	};

	uint8_t DATAGRAM[sizeof(CAPTURED_DATAGRAM)] = {};
	Net::Size_guard size_guard { sizeof(DATAGRAM) };

	auto *dns = Genode::construct_at<Net::Dns_packet>(DATAGRAM);
	memcpy(DATAGRAM, CAPTURED_DATAGRAM, sizeof(CAPTURED_DATAGRAM));

	if (dns->id() != 0xe445) {
		error("Could not extract response ID properly");
		throw Test_failure(); }

	if (!dns->response()) {
		error("Querry bit is not interpreted correctly");
		throw Test_failure(); }

	if (dns->truncated()) {
		error("Truncated bit is not interpreted correctly");
		throw Test_failure(); }

	/* expected address from the CAPTURED_DATAGRAM */
	uint8_t addr1[4] { 192, 33, 214, 47 };
	uint8_t addr2[4] { 212, 51, 144, 46 };
	uint8_t addr3[4] { 212, 243, 96, 76 };
	uint8_t addr4[4] { 193, 33, 30, 39 };

	Net::Ipv4_address addrs[4] {
		addr1,
		addr2,
		addr3,
		addr4
	};

	size_t idx = 0;
	dns->for_each_entry(size_guard, [&addrs, &idx] (Net::Dns_packet::Dns_entry const &entry) {
		log(entry.name, " resolved to ", entry.addr);
		if (entry.addr != addrs[idx]) {
			throw Test_failure(); }
		++idx; });

	log("DNS response successfully parsed");
}

void Dns_test::test_dns_malformated_response()
{
	log("----- ", __FUNCTION__, " -----");

	/* the byte in uppercase (0xFF) is a malicious offset */
	uint8_t MALICIOUS_DATAGRAM[] = {
		0xe4, 0x45, 0x81, 0x80, 0x00, 0x01, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00, 0x04, 0x70, 0x6f, 0x6f,
		0x6c, 0x03, 0x6e, 0x74, 0x70, 0x03, 0x6f, 0x72,
		0x67, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xc0, 0x21, 0xd6, 0x2f, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xd4, 0x33, 0x90, 0x2e, 0xc0, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xd4, 0xf3, 0x60, 0x4c, 0xFF, 0x0c,
		0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24,
		0x00, 0x04, 0xc1, 0x21, 0x1e, 0x27
	};

	uint8_t DATAGRAM[sizeof(MALICIOUS_DATAGRAM)] = {};
	Net::Size_guard size_guard { sizeof(DATAGRAM) };

	auto *dns = Genode::construct_at<Net::Dns_packet>(DATAGRAM);
	memcpy(DATAGRAM, MALICIOUS_DATAGRAM, sizeof(MALICIOUS_DATAGRAM));

	size_t idx { 0 };
	try {
		dns->for_each_entry(size_guard, [&idx] (Net::Dns_packet::Dns_entry const &entry) {
			log(entry.name, " resolved to ", entry.addr);
			++idx; });
	} catch(...) { }

	if (idx != 3) {
		error("out of bound access not detected");
		throw Test_failure(); }

	log("DNS malformated response successfully parsed");
}


void Component::construct(Genode::Env &env)
{
	Dns_test::test_domain_name_format();
	Dns_test::test_dns_request();
	Dns_test::test_dns_response();
	Dns_test::test_dns_malformated_response();

	env.parent().exit(0);
}

