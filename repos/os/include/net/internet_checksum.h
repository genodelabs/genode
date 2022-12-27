/*
 * \brief  Computing the Internet Checksum (conforms to RFC 1071)
 * \author Martin Stein
 * \date   2018-03-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__INTERNET_CHECKSUM_H_
#define _NET__INTERNET_CHECKSUM_H_

/* Genode includes */
#include <net/ipv4.h>
#include <base/stdint.h>

namespace Net {

	/**
	 * This struct helps avoiding the following compiler warning when using
	 * the internet checksum functions on packet classes (like
	 * Net::Ipv4_packet):
	 *
	 * warning: converting a packed ‘Net::[PACKET_CLASS]’ pointer
	 *          (alignment 1) to a ‘const uint16_t’ {aka ‘const short
	 *          unsigned int’} pointer (alignment 2) may result in an
	 *          unaligned pointer value
	 *
	 * Apparently, the 'packed' attribute normally used on packet classes sets
	 * the alignment of the packet class to 1. However, for the purpose of the
	 * internet-checksum functions, we can assume that the packet data has no
	 * alignment. This is expressed by casting the packet-object pointer to a
	 * Packed_uint16 pointer instead of a uint16_t pointer.
	 */
	struct Packed_uint16
	{
		Genode::uint16_t value;

	} __attribute__((packed));

	Genode::uint16_t internet_checksum(Packed_uint16 const *data_ptr,
	                                   Genode::size_t       data_sz);

	Genode::uint16_t
	internet_checksum_pseudo_ip(Packed_uint16 const   *data_ptr,
	                            Genode::size_t         data_sz,
	                            Genode::uint16_t       ip_data_sz_be,
	                            Ipv4_packet::Protocol  ip_prot,
	                            Ipv4_address          &ip_src,
	                            Ipv4_address          &ip_dst);

	/**
	 * Accumulating modifier for incremental updates of internet checksums
	 */
	class Internet_checksum_diff
	{
		private:

			signed long _value { 0 };

		public:

			/**
			 * Update modifier according to a data update in the target region
			 *
			 * PRECONDITIONS
			 *
			 * * The pointers must refer to data that is at an offset inside
			 *   the checksum'd region that is a multiple of 2 bytes (16 bits).
			 */
			void add_up_diff(Packed_uint16 const *new_data_ptr,
			                 Packed_uint16 const *old_data_ptr,
			                 Genode::size_t       data_sz);

			/**
			 * Update this modifier by adding up another modifier
			 */
			void add_up_diff(Internet_checksum_diff const &icd);

			/**
			 * Return the given checksum with this modifier applied
			 */
			Genode::uint16_t apply_to(signed long sum) const;
	};
}

#endif /* _NET__INTERNET_CHECKSUM_H_ */
