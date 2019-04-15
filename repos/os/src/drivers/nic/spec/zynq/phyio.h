/*
 * \brief  Phy driver for Marvell chips
 * \author Timo Wischer
 * \date   2015-05-11
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__NIC__GEM__PHYIO_H_
#define _INCLUDE__DRIVERS__NIC__GEM__PHYIO_H_


namespace Genode
{
	class Phyio
	{
		public:
			virtual void phy_write(const uint8_t phyaddr, const uint8_t regnum, const uint16_t data) = 0;
			virtual void phy_read(const uint8_t phyaddr, const uint8_t regnum, uint16_t &data) = 0;

			virtual ~Phyio() { }
	};
}

#endif /* _INCLUDE__DRIVERS__NIC__GEM__PHYIO_H_ */

