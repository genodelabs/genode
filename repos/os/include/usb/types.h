/**
 * \brief  Basic types for USB
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__USB__TYPES_H_
#define _INCLUDE__USB__TYPES_H_

#include <base/stdint.h>

namespace Usb {
	struct Device_descriptor;
	struct Config_descriptor;
	struct Interface_descriptor;
	struct Endpoint_descriptor;
	struct String;
}

namespace Usb {

	enum Endpoint_type {
		ENDPOINT_BULK      = 0x2,
		ENDPOINT_INTERRUPT = 0x3,
	};

	/**
	 * The following three enums are ORed together to form a control request
	 * type
	 */
	enum Endpoint_direction {
		ENDPOINT_OUT       = 0,
		ENDPOINT_IN        = 0x80,
	};

	enum Request_type {
		TYPE_STANDARD = 0,
		TYPE_CLASS    = (1U << 5),
		TYPE_VENDOR   = (2U << 5),
		TYPE_RESERVED = (3U << 5),
	};

	enum Recipient {
		RECIPIENT_DEVICE    = 0,
		RECIPIENT_INTERFACE = 0x1,
		RECIPIENT_ENDPOINT  = 0x2,
		RECIPIENT_OTHER     = 0x3,
	};

	/**
	 * UTF-16 string
	 */
	typedef Genode::uint16_t utf16_t;
}


/**
 * String containing UTF-16 plane 0 characters
 */
struct Usb::String
{
	utf16_t *string = nullptr;
	unsigned length = 0;

	void copy(unsigned len, void *from, Genode::Allocator *md_alloc)
	{
		length = len;

		if (len == 0)
			return;

		string = (utf16_t *)md_alloc->alloc(length * sizeof(utf16_t));
		Genode::memcpy(string, from, sizeof(utf16_t) * length);
	}

	void free(Genode::Allocator *md_alloc)
	{
		if (!string)
			return;

		md_alloc->free(string, length * sizeof(utf16_t));
		string = nullptr;
	}

	/**
	 * Create char version
	 */
	char *to_char(char *buffer, unsigned len)
	{
		if (!length)
			return (char *)"<unknown>";

		len = Genode::min(length, len);
		for (unsigned i = 0; i < len; i++)
			buffer[i] = string[i] & 0xff;

		buffer[len] = 0;
		return buffer;
	}

	/**
	 * Print for debugging
	 */
	void print()
	{
		char buffer[128];
		Genode::log(Genode::Cstring(to_char(buffer, 128)));
	}
};

/**
 * USB hardware device descriptor
 */
struct Usb::Device_descriptor
{
	Genode::uint8_t  length = 0;
	Genode::uint8_t  type   = 0x1;

	Genode::uint16_t usb             = 0;      /* USB version in BCD (binary-coded decimal ) */
	Genode::uint8_t  dclass          = 0;
	Genode::uint8_t  dsubclass       = 0;
	Genode::uint8_t  dprotocol       = 0;
	Genode::uint8_t  max_packet_size = 0;      /* of endpoint zero */

	Genode::uint16_t vendor_id      = 0;
	Genode::uint16_t product_id     = 0;
	Genode::uint16_t device_release = 0;       /* release number in BCD */

	Genode::uint8_t  manufactorer_index  = 0;  /* index of string describing manufacturer */
	Genode::uint8_t  product_index       = 0;
	Genode::uint8_t  serial_number_index = 0;

	Genode::uint8_t  num_configs = 0;

	/**
	 * Genode extensions (POD only)
	 */
	unsigned         bus   = 0;
	unsigned         num   = 0;
	unsigned         speed = 0;
} __attribute__((packed));

/**
 * USB hardware configuration descriptor
 */
struct Usb::Config_descriptor
{
	Genode::uint8_t  length = 0;
	Genode::uint8_t  type   = 0x2;

	/*
	 * Total length of data returned for this configuration. Includes the
	 * combined length of all descriptors (configuration, interface, endpoint,
	 * and class or vendor specific) returned for this configuration.
	 */
	Genode::uint16_t total_length   = 0;
	Genode::uint8_t  num_interfaces = 0;

	Genode::uint8_t  config_value = 0; /* value used to set this configuration */
	Genode::uint8_t  config_index = 0; /* index of string descriptor */

	Genode::uint8_t  attributes = 0;
	Genode::uint8_t  max_power  = 0;    /* maximum power consumption */
} __attribute__((packed));

/**
 * USB hardware interface descriptor
 */
struct Usb::Interface_descriptor
{
	Genode::uint8_t length = 0;
	Genode::uint8_t type   = 0x4;

	Genode::uint8_t number       = 0;    /* interface number */
	Genode::uint8_t alt_settings = 0;    /* value used for setting alternate setting 
	                                        using the 'number' field */
	Genode::uint8_t num_endpoints = 0;

	Genode::uint8_t iclass    = 0;
	Genode::uint8_t isubclass = 0;
	Genode::uint8_t iprotocol = 0;

	Genode::uint8_t interface_index = 0; /* index of string descriptor */

	/**
	 * Genode extensions (POD only)
	 */
	bool active = false;

} __attribute__((packed));

/**
 * USB hardware endpoint descriptor
 */
struct Usb::Endpoint_descriptor
{
	Genode::uint8_t  length           = 0;
	Genode::uint8_t  type             = 0x5;
	Genode::uint8_t  address          = 0;
	Genode::uint8_t  attributes       = 0;
	Genode::uint16_t max_packet_size  = 0;  /* for this endpoint */
	Genode::uint8_t  polling_interval = 0;
} __attribute__((packed));

#endif /* _INCLUDE__USB__TYPES_H_ */
