/*
 * \brief  PCI basic types
 * \author Stefan Kalkowski
 * \date   2021-12-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __INCLUDE__PCI__TYPES_H__
#define __INCLUDE__PCI__TYPES_H__

#include <base/stdint.h>
#include <util/register.h>
#include <util/string.h>

namespace Pci {
	/**
	 * Topology POD-types: bus, device, function
	 */
	using bus_t  = Genode::uint8_t;
	using dev_t  = Genode::uint8_t;
	using func_t = Genode::uint8_t;

	/* Bus, device, function encoded as routing ID */
	using rid_t  = Genode::uint16_t;

	/**
	 * Bus, device, function as C++ object representation
	 */
	struct Bdf;

	/**
	 * Further POD-types found in PCI configuration space
	 */
	using irq_line_t = Genode::uint8_t;
	using irq_pin_t  = Genode::uint8_t;
	using vendor_t   = Genode::uint16_t;
	using device_t   = Genode::uint16_t;
	using class_t    = Genode::uint32_t;
	using rev_t      = Genode::uint8_t;
}


struct Pci::Bdf
{
	bus_t  bus;
	dev_t  dev;
	func_t fn;

	struct Routing_id : Genode::Register<16>
	{
		struct Function : Bitfield<0,3> {};
		struct Device   : Bitfield<3,5> {};
		struct Bus      : Bitfield<8,8> {};
	};

	static Bdf bdf(rid_t rid)
	{
		return { (Pci::bus_t)  Routing_id::Bus::get(rid),
		         (Pci::dev_t)  Routing_id::Device::get(rid),
		         (Pci::func_t) Routing_id::Function::get(rid) };
	}

	static rid_t rid(Bdf bdf)
	{
		Routing_id::access_t rid = 0;
		Routing_id::Bus::set(rid, bdf.bus);
		Routing_id::Device::set(rid, bdf.dev);
		Routing_id::Function::set(rid, bdf.fn);
		return rid;
	}

	bool operator == (Bdf const &id) const {
		return id.bus == bus && id.dev == dev && id.fn == fn; }


	static Genode::String<16> string(Bdf bdf)
	{
		using namespace Genode;
		return String<16>(Hex(bdf.bus, Hex::OMIT_PREFIX, Hex::PAD), ":",
		                  Hex(bdf.dev, Hex::OMIT_PREFIX, Hex::PAD), ".",
		                  bdf.fn);
	}

	void print(Genode::Output &out) const {
		string(*this).print(out); }
};

#endif /* __INCLUDE__PCI__TYPES_H__ */
