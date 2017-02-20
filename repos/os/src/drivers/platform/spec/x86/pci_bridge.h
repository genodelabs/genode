/*
 * \brief  PCI bridge discovery
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 */

 /*
  * Copyright (C) 2009-2017 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

#pragma once

namespace Platform { class Bridge; }

#include <util/list.h>


/**
 * List of PCI-bridge devices
 */
class Platform::Bridge : public Genode::List<Bridge>::Element
{
	private:

		/* PCI config space fields of bridge */
		unsigned char _bus;
		unsigned char _dev;
		unsigned char _fun;

		unsigned char _secondary_bus;
		unsigned char _subordinate_bus;

	public:

		Bridge(unsigned char bus, unsigned char dev, unsigned char fun,
		       unsigned char secondary_bus, unsigned char subordinate_bus)
		:
			_bus(bus), _dev(dev), _fun(fun), _secondary_bus(secondary_bus),
			_subordinate_bus(subordinate_bus)
		{ }

		bool part_of (unsigned char bus) const
		{
			return _secondary_bus <= bus && bus <= _subordinate_bus;
		}

		unsigned short bdf()
		{
			unsigned short bdf = _bus;
			bdf = (bdf << 8) | ((_dev & 0x1f) << 3) | (_fun & 0x7);
			return bdf;
		}
};
