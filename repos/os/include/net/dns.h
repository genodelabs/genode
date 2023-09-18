/*
 * \brief  DNS Request class and related
 * \author Alice Domage <alice.domage@gapfruit.com>
 * \date   2023-09-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 * Copyright (C) 2023 gapfruit ag
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__DNS_H_
#define _NET__DNS_H_

/* Genode */
#include <util/string.h>
#include <util/construct_at.h>
#include <util/endian.h>
#include <net/size_guard.h>
#include <net/ipv4.h>


namespace Net {

	using namespace Genode;

	class Domain_name;
	class Dns_packet;

	static inline size_t ascii_to(char const *, Domain_name&);

}


/*
 * Domain Name format following RFC 1035
 *
 * Various objects and parameters in the DNS have size limits.  They are
 * listed below.  Some could be easily changed, others are more
 * fundamental.
 *
 * labels          63 octets or less
 *
 * names           255 octets or less
 *
 * QNAME           a domain name represented as a sequence of labels, where
 *                 each label consists of a length octet followed by that
 *                 number of octets.  The domain name terminates with the
 *                 zero length octet for the null label of the root.  Note
 *                 that this field may be an odd number of octets; no
 *                 padding is used.*
 */
class Net::Domain_name
{
	public:

		static constexpr const size_t ZERO_LENGTH_OCTET = 1;
		static constexpr const size_t NAME_MAX_LEN      = 255;
		static constexpr const size_t LABEL_MAX_LEN     = 63;
		static constexpr const size_t MIN_ROOT_LABEL    = 3;
		static constexpr const size_t MAX_ROOT_LABEL    = 6;

	private:

		String<NAME_MAX_LEN>  _name { };

		static char const *_next_label(char const *label)
		{
			size_t length = static_cast<size_t>(*label);
			++label;
			return label + length;
		}

	public:

		Domain_name() = default;

		Domain_name(char const *name) { ascii_to(name, *this); }

		bool operator==(Domain_name const &other) const {
			return _name == other._name; }

		size_t length() const { return _name.length(); }

		void copy(void *dest) const {
			memcpy(dest, _name.string(), _name.length()); }

		void label(size_t label_length, char const *label)
		{
			if (label_length + _name.length() > NAME_MAX_LEN) {
				return; }

			if (label_length > LABEL_MAX_LEN) {
				return; }

			/*
			 * copy label into a separate buffer, first to easily avoid
			 * label_length to be converted to ascii. Also all the labels,
			 * including the bytes further than label_length would be copied
			 * into the string.
			 */
			char buff[LABEL_MAX_LEN + 1] { };
			buff[0] = static_cast<char>(label_length);
			memcpy(buff + 1, label, label_length);

			_name = String<NAME_MAX_LEN> { _name, String<LABEL_MAX_LEN>(buff) };
		}

		size_t label_count() const
		{ 
			char const     *label = _name.string();
			size_t  count { 0 };

			if (*label == 0) {
				return 0; }

			while(*label) {
				++count;
				label = _next_label(label); }

			return count;
		}

		void print(Output &output) const
		{
			char const *label = _name.string();

			if (*label == 0) {
				return; }

			size_t label_length = static_cast<size_t>(*label);
			for (size_t idx = 0; idx < label_length; ++idx) {
				output.out_char(*(label + 1 + idx)); }

			label = _next_label(label);
			while (*label) {
				label_length = static_cast<size_t>(*label);
				output.out_char('.');
				for (size_t idx = 0; idx < label_length; ++idx) {
					output.out_char(static_cast<char>(*(label + 1 + idx))); } 
				label = _next_label(label);
			}
		}

};


/**
 * Data layout of this class conforms to a DNS Request layout (RFC 1035)
 *
 * DNS request header:
 *
 * +---------------------+
 * |        Header       | the request header
 * +---------------------+
 * |       Question      | the question for the name server
 * +---------------------+
 * |        Answer       | RRs answering the question
 * +---------------------+
 * |      Authority      | RRs pointing toward an authority
 * +---------------------+
 * |      Additional     | RRs holding additional information
 * +---------------------+*
 */
class Net::Dns_packet
{
	public:

		enum class Type: uint16_t {
			A        = 1,   /* a host address */
			NS       = 2,   /* an authoritative name server*/
			MD       = 3,   /* a mail destination (Obsolete - use MX) */
			MF       = 4,   /* a mail forwarder (Obsolete - use MX) */
			CNA      = 5,   /* the canonical name for an alias */
			SOA      = 6,   /* marks the start of a zone of authority */
			MB       = 7,   /* a mailbox domain name (EXPERIMENTAL) */
			MG       = 8,   /* a mail group member (EXPERIMENTAL) */
			MR       = 9,   /* a mail rename domain name (EXPERIMENTAL) */
			NUL      = 10,  /* a null RR (EXPERIMENTAL) */
			WKS      = 11,  /* a well known service description */
			PTR      = 12,  /* a domain name pointer */
			HIN      = 13,  /* host information */
			MIN      = 14,  /* mailbox or mail list information */
			MX       = 15,  /* mail exchange */
			TXT      = 16,  /* text strings */
			AXFR     = 252, /* A request for a transfer of an entire zone */
			MAILB    = 253, /* A request for mailbox-related records (MB, MG or MR) */
			MAILA    = 254, /* A request for mail agent RRs (Obsolete - see MX) */
			WILDCARD = 255, /* A request for all records */
		};

		enum class Class: uint16_t {
			IN       = 1,   /* the Internet */
			CS       = 2,   /* the CSNET class (Obsolete - used only for examples in some obsolete RFCs) */
			CH       = 3,   /* the CHAOS class */
			HS       = 4,   /* Hesiod [Dyer 87] */
			WILDCARD = 255, /* any class */
		};

		struct Dns_entry
		{
			Domain_name  name      { };
			Type         net_type  { };
			Class        net_class { };
			uint32_t     ttl       { };
			Ipv4_address addr      { };
		};

	private:

		/**
		 * DNS Request Header
		 *
		 *                                 1  1  1  1  1  1
		 *   0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                      ID                       |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                    QDCOUNT                    |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                    ANCOUNT                    |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                    NSCOUNT                    |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                    ARCOUNT                    |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+*
		 */
		struct Header_datagram {
			uint16_t id;
			uint16_t flags;
			uint16_t qdcount;
			uint16_t ancount;
			uint16_t nscount;
			uint16_t arcount;
		} __attribute__((packed));

		/*
		 * Flags struct to access Header::flags
		 */
		struct Flags: Register<16> {
			struct Rcode                : Register<16>::Bitfield<0, 4>  { };
			struct Recursion_available  : Register<16>::Bitfield<7, 1>  { };
			struct Recursion_desired    : Register<16>::Bitfield<8, 1>  { };
			struct Truncation           : Register<16>::Bitfield<9, 1>  { };
			struct Authoritative_answer : Register<16>::Bitfield<10, 1> { };
			struct Opcode               : Register<16>::Bitfield<11, 4> { };
			struct Querry               : Register<16>::Bitfield<15, 1> { };
		};

		/*
		 * Querry layout for dns request (RFC 1035)
		 *
		 *                                 1  1  1  1  1  1
		 *   0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                                               |
		 * /                     QNAME                     /
		 * /                                               /
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                     QTYPE                     |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                     QCLASS                    |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 */
		struct Question_datagram {
			uint16_t qtype;
			uint16_t qclass;
		} __attribute__((packed));

		/*
		 * Response layout for dns request (RFC 1035)
		 *
		 *                                 1  1  1  1  1  1
		 *   0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                                               |
		 * /                                               /
		 * /                      NAME                     /
		 * |                                               |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                      TYPE                     |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                     CLASS                     |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                      TTL                      |
		 * |                                               |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 * |                   RDLENGTH                    |
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
		 * /                     RDATA                     /
		 * /                                               /
		 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		 *
		 */
		struct Response_datagram {
			uint16_t rtype;
			uint16_t rclass;
			uint32_t rttl;
			uint16_t rdlength;
			uint8_t  rdata[0];
		} __attribute__((packed));

		Header_datagram  _header;
		uint16_t         _data[0];

		uint16_t *_next_question_entry(void *curr_field)
		{
			uint8_t *label = reinterpret_cast<uint8_t*>(curr_field);
			while(*label) ++label;
			++label;
			label += sizeof(Question_datagram);
			return reinterpret_cast<uint16_t*>(label);
		}

		void _check_size_guard(Size_guard size_guard, void *ptr)
		{
			if (reinterpret_cast<size_t>(ptr) > reinterpret_cast<size_t>(_data)) {
				if (reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(_data) > size_guard.unconsumed()) {
					throw Size_guard::Exceeded(); } }
		}

	public:

		enum { UDP_PORT = 53, };

		void id(uint16_t id) { _header.id = host_to_big_endian(id); }

		uint16_t id() const { return big_endian_to_host(_header.id); }

		bool truncated() const {
			return Flags::Truncation::get(big_endian_to_host(_header.flags)); }

		bool response() const {
			return Flags::Querry::get(big_endian_to_host(_header.flags)); }

		static inline size_t sizeof_question(Domain_name const &dn) {
			return sizeof(Question_datagram) + dn.length(); }

		uint16_t qdcount() const { return big_endian_to_host(_header.qdcount); }

		uint16_t ancount() const { return big_endian_to_host(_header.ancount); }

		void recursion_desired(bool value)
		{
			uint16_t flags = big_endian_to_host(_header.flags);
			Flags::Recursion_desired::set(flags, value);
			_header.flags = host_to_big_endian(flags);
		}

		void question(Size_guard &size_guard,
		              Domain_name const &dn,
		              Type qtype = Type::A,
		              Class qclass = Class::IN)
		{
			/* only populate questions, when the message is a querry */
			if (response()) return;

			void *qslot = _data;

			/* skip existing question entries */
			for (size_t count = 0; count < big_endian_to_host(_header.qdcount); ++count) {
				qslot = _next_question_entry(qslot); }

			size_guard.consume_head(dn.length() + sizeof(Question_datagram));
			dn.copy(qslot);
			auto *q = construct_at<Question_datagram>((uint8_t*)qslot + dn.length());
			q->qtype = host_to_big_endian(static_cast<uint16_t>(qtype));
			q->qclass = host_to_big_endian(static_cast<uint16_t>(qclass));

			/* adjust header's question count */
			_header.qdcount = host_to_big_endian(
				static_cast<uint16_t>(big_endian_to_host(_header.qdcount) + 1));
		}

		template<typename FN = void(*)(Dns_entry const &)>
		void for_each_entry(Size_guard &size_guard, FN fn)
		{
			/* only read responses, when the message is a response */
			if (!response()) return;

			/* by-pass questions entries */
			void *rslot = _data;

			Domain_name name;

			/* skip question entries */
			for (size_t idx = 0; idx < big_endian_to_host(_header.qdcount); ++idx) {
				_check_size_guard(size_guard, rslot);
				rslot = _next_question_entry(rslot); }

			/* for each answer entries */
			for (size_t idx = 0; idx < big_endian_to_host(_header.ancount); ++idx) {

				_check_size_guard(size_guard, rslot);

				Dns_entry entry { };

				/* read domain name, adjusting methods if compression bits are set */
				uint16_t compression_octets =
					big_endian_to_host(*reinterpret_cast<uint16_t*>(rslot));

				char *label = reinterpret_cast<char*>(rslot);

				/* if compression bits are set, adjust label pointer */
				if (compression_octets & 0xC000) {
					size_t offset = compression_octets & 0x3FFF;
					label = reinterpret_cast<char*>(&_header) + offset;
					_check_size_guard(size_guard, label);
				}

				/* read domain name labels */
				while(*label) {
					size_t size = *label;
					char *str = label + 1;
					entry.name.label(size, str);
					label = str + size;
					_check_size_guard(size_guard, label); }

				/* if compression bits are set, adjust rslot pointer */
				if (compression_octets & 0xC000) {
					rslot = reinterpret_cast<uint16_t*>(rslot) + 1;
				} else {
					/* if no compersion bits are set, rslot continue after the domain name */
					rslot = label + 1;
				}

				Response_datagram *rd = reinterpret_cast<Response_datagram*>(rslot);

				_check_size_guard(size_guard, rd->rdata);

				entry.net_class = static_cast<Class>(big_endian_to_host(rd->rclass));
				entry.net_type = static_cast<Type>(big_endian_to_host(rd->rtype));
				entry.ttl = big_endian_to_host(rd->rttl);

				/* currently only support IPV4 formated response data */
				if (big_endian_to_host(rd->rdlength) != IPV4_ADDR_LEN) {
					log("Dns address data length is unsupported, ignoring entry for ", entry.name);
					continue; }

				entry.addr = Ipv4_address { rd->rdata };

				fn(entry);
				rslot = rd->rdata + big_endian_to_host(rd->rdlength);
			}
		}

} __attribute__((packed));


/*
 * Domain Name Grammar (RFC 1035)
 *
 * <domain> ::= <subdomain> | " "
 *
 * <subdomain> ::= <label> | <subdomain> "." <label>
 *
 * <label> ::= <letter> [ [ <ldh-str> ] <let-dig> ]
 *
 * <ldh-str> ::= <let-dig-hyp> | <let-dig-hyp> <ldh-str>
 *
 * <let-dig-hyp> ::= <let-dig> | "-"
 *
 * <let-dig> ::= <letter> | <digit>
 *
 * <letter> ::= any one of the 52 alphabetic characters A through Z in upper
 *              case and a through z in lower case
 *
 * <digit> ::= any one of the ten digits 0 through 9
 *
 * Note that while upper and lower case letters are allowed in domain
 * names, no significance is attached to the case.  That is, two names with
 * the same spelling but different case are to be treated as if identical.
 * 
 * The labels must follow the rules for ARPANET host names.  They must
 * start with a letter, end with a letter or digit, and have as interior
 * characters only letters, digits, and hyphen.  There are also some
 * restrictions on the length.  Labels must be 63 characters or less.
 */
Genode::size_t Net::ascii_to(char const *str, Net::Domain_name &result)
{
	char const *label = str;
	size_t      label_length = 0;
	Domain_name domain_name;

	for (;;) {
		/* label must start by <letter>  */
		if (label_length == 0 && !is_letter(*label)) {
			return 0; }

		/* last label character is <let-dig> */
		if (*(str + 1) == '.' || *(str + 1) == '\"' || *(str + 1) == '\0') {
			if (!is_letter(*str) && !is_digit(*str)) {
				return 0; } }

		/* label is <subdomain> */
		if (*str == '.') {
			if (label_length == 0) return 0;
			if (label_length > 63) return 0;
			domain_name.label(label_length, label);
			label_length = 0;
			++str;
			label = str; }

		/* label is <domain> */
		if (*str == '\"' || *str == '\0') {
			if (label_length < Domain_name::MIN_ROOT_LABEL ||
			    label_length > Domain_name::MAX_ROOT_LABEL) {
				return 0; }

			if (domain_name.length() + label_length > Domain_name::NAME_MAX_LEN) {
				return 0; }

			domain_name.label(label_length, label);
			if (domain_name.label_count() < 2) return 0;
			result = domain_name; 
			return result.length(); }

		/* label character is <let-dig-hyp> */
		if (!is_letter(*str) && !is_digit(*str) && *str != '-') {
			return 0; }

		++label_length;
		++str;
	}
	return 0;
}

#endif /* _NET__DNS_H_ */

