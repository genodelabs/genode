/*
 * \brief  Phy driver for Marvell chips
 * \author Timo Wischer
 * \date   2015-05-11
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__NIC__GEM__MARVELL_PHY_H_
#define _INCLUDE__DRIVERS__NIC__GEM__MARVELL_PHY_H_

/* Genode includes */
#include <os/attached_mmio.h>
#include <nic_session/nic_session.h>
#include <nic/driver.h>
#include <timer_session/connection.h>

#include "phyio.h"


namespace Genode
{

	enum Eth_speed {
		UNDEFINED,
		SPEED_10 = 10,
		SPEED_100 = 100,
		SPEED_1000 = 1000
	};


	class Marvel_phy
	{
		public:
			class Phy_timeout_after_reset : public Genode::Exception {};


		private:
			Timer::Connection _timer;

			Phyio& _phyio;
			uint16_t _phyaddr;
			bool _link_up;
			Eth_speed _eth_speed;

			void phy_read(const uint32_t regnum, uint16_t *val);
			void phy_write(const uint32_t regnum, const uint16_t data);
			void phy_detection();
			void get_phy_id(uint32_t *phy_id);
			void m88e1310_config();
			int genphy_config_aneg();
			int genphy_config_advert();
			int genphy_restart_aneg();
			int phy_reset();
			int m88e1011s_startup();
			int genphy_update_link();
			int m88e1xxx_parse_status();


		public:
			Marvel_phy(Phyio& phyio) :
				_phyio(phyio),
				_phyaddr(0),
				_link_up(false),
				_eth_speed(UNDEFINED)
			{ }

			void init();

			Eth_speed eth_speed() { return _eth_speed; }

	};
}

#endif /* _INCLUDE__DRIVERS__NIC__GEM__MARVELL_PHY_H_ */

