/*
 * \brief  Phy driver for Marvell chips
 * \author Johannes Schlatow
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

	class MII_phy
	{
		protected:
			/**
			 * \param _OFFSET        Offset/number of the register.
			 *
			 * For further details See 'Genode::Register'.
			 */
			template <uint8_t _OFFSET>
			struct Phy_register : public Genode::Register<16>
			{
				enum {
					OFFSET       = _OFFSET,
				};

				typedef Phy_register<_OFFSET>
					Register_base;

				/**
				 * A region within a register
				 *
				 * \param _SHIFT  Bit shift of the first bit within the
				 *                compound register.
				 * \param _WIDTH  bit width of the region
				 *
				 * For details see 'Genode::Register::Bitfield'.
				 */
				template <unsigned long _SHIFT, unsigned long _WIDTH>
				struct Bitfield : public Genode::Register<16>::
												 template Bitfield<_SHIFT, _WIDTH>
				{
					typedef Bitfield<_SHIFT, _WIDTH> Bitfield_base;

					/* back reference to containing register */
					typedef Phy_register<_OFFSET>
						Compound_reg;
				};
			};

			/*************************
			 * Generic MII registers *
			 *************************/

			/* Basic mode control register */
			struct Bmcr : Phy_register<0x00>
			{
				struct Resv      : Bitfield<0, 6> { }; /* Unused...		       */
				struct Speed1000 : Bitfield<6, 1> { }; /* MSB of Speed (1000)	       */
				struct Ctst      : Bitfield<7, 1> { }; /* Collision test	       */
				struct Fulldplx  : Bitfield<8, 1> { }; /* Full duplex		       */
				struct Anrestart : Bitfield<9, 1> { }; /* Auto negotiation restart    */
				struct Isolate   : Bitfield<10,1> { }; /* Disconnect DP83840 from MII */
				struct Pdown     : Bitfield<11,1> { }; /* Powerdown the DP83840       */
				struct Anenable  : Bitfield<12,1> { }; /* Enable auto negotiation     */
				struct Speed100  : Bitfield<13,1> { }; /* Select 100Mbps	       */
				struct Loopback  : Bitfield<14,1> { }; /* TXD loopback bits	       */
				struct Reset     : Bitfield<15,1> { }; /* Reset the DP83840	       */
			};

			/* Basic mode status register */
			struct Bmsr : Phy_register<0x01>
			{
				enum {
					INVALID        = 0xFFFF
				};

				struct Ercap       : Bitfield<0, 1> { }; /* Ext-reg capability      */
				struct Jcd         : Bitfield<1, 1> { }; /* Jabber detected         */
				struct Lstatus     : Bitfield<2, 1> { }; /* Link status             */
				struct Anegcapable : Bitfield<3, 1> { }; /* Able to do auto-negotiation */
				struct Rfault      : Bitfield<4, 1> { }; /* Remote fault detected       */
				struct Anegcomplete: Bitfield<5, 1> { }; /* Auto-negotiation complete   */
				struct Resv        : Bitfield<6, 1> { }; /* Unused...		       */
				struct Estaten     : Bitfield<7, 1> { }; /* Extended Status in R15 */
				struct Half2_100   : Bitfield<8, 1> { }; /* Can do 100BASE-T2 HDX */
				struct Full2_100   : Bitfield<9, 1> { }; /* Can do 100BASE-T2 FDX */
				struct Half_10     : Bitfield<10,1> { }; /* Can do 10mbps, half-duplex  */
				struct Full_10     : Bitfield<11,1> { }; /* Can do 10mbps, full-duplex  */
				struct Half_100    : Bitfield<12,1> { }; /* Can do 100mbps, half-duplex */
				struct Full_100    : Bitfield<13,1> { }; /* Can do 100mbps, full-duplex */
				struct Base4_100   : Bitfield<14,1> { }; /* Can do 100mbps, 4k packets  */
			};

			/* ID register 1 */
			struct Idr1 : Phy_register<0x02> { };

			/* ID register 2 */
			struct Idr2 : Phy_register<0x03> { };

			/* Advertisement control reg   */
			struct Advertise : Phy_register<0x04>
			{
				struct Csma         : Bitfield<0, 1> { };
				struct Half_10      : Bitfield<5, 1> { }; /* Try for 10mbps half-duplex  */
				struct FullX_1000   : Bitfield<5, 1> { }; /* Try for 1000BASE-X full-duplex */
				struct Full_10      : Bitfield<6, 1> { }; /* Try for 10mbps full-duplex  */
				struct HalfX_1000   : Bitfield<6, 1> { }; /* Try for 1000BASE-X half-duplex */
				struct Half_100     : Bitfield<7, 1> { }; /* Try for 100mbps half-duplex  */
				struct PauseX_1000  : Bitfield<7, 1> { }; /* Try for 1000BASE-X pause */
				struct Full_100     : Bitfield<8, 1> { }; /* Try for 100mbps full-duplex  */
				struct AsymXPSE_1000: Bitfield<8, 1> { }; /* Try for 1000BASE-X asym pause */
				struct Base4_100    : Bitfield<9, 1> { }; /* Try for 100mbps 4k packets  */
				struct Pause_cap    : Bitfield<10,1> { }; /* Try for pause	       */
				struct Pause_asym   : Bitfield<11,1> { }; /* Try for asymetrict pause */
				struct Rfault       : Bitfield<13,1> { }; /* Say we can detect faults    */
				struct Lpack        : Bitfield<14,1> { }; /* Ack link partners response  */
				struct Npage        : Bitfield<15,1> { }; /* Next page bit	       */
			};

			/* 1000BASE-T control	       */
			struct Ctrl1000 : Phy_register<0x09>
			{
				struct Half_1000 : Bitfield<8, 1> { }; /* Advertise 1000BASE-T full duplex */
				struct Full_1000 : Bitfield<9, 1> { }; /* Advertise 1000BASE-T half duplex */
			}; 
	};

	class Marvel_phy : public MII_phy
	{
		public:
			class Phy_timeout_after_reset : public Genode::Exception {};


		private:

			enum {
				PHY_AUTONEGOTIATE_TIMEOUT = 5000
			};

			Timer::Connection _timer;
			Phyio& _phyio;
			int8_t _phyaddr;
			bool _link_up;
			Eth_speed _eth_speed;


			/*************************
			 * 88E1310 PHY registers *
			 *************************/

			struct Led_ctrl : Phy_register<16>
			{
				struct Data : Bitfield<0, 4> { };
			};

			struct Irq_en : Phy_register<18> { };

			struct RGMII_ctrl : Phy_register<21> { };

			struct Page_select : Phy_register<22> { };

			/* 88E1011 PHY Status Register */
			struct Phy_stat : Phy_register<0x11>
			{
				struct Link        : Bitfield<10,1> { };
				struct Spddone     : Bitfield<11,1> { };
				struct Duplex      : Bitfield<13,1> { };
				struct Speed_100   : Bitfield<14,1> { };
				struct Speed_1000  : Bitfield<15,1> { };
			};

			/**
			 * Read register of detected PHY.
			 */
			inline uint16_t _phy_read(const uint8_t regnum) const
			{
				uint16_t val;
				_phyio.phy_read(_phyaddr, regnum, val);
				return val;
			}

			/**
			 * Write register of detected PHY.
			 */
			inline void _phy_write(const uint8_t regnum, const uint16_t data) const
			{
				_phyio.phy_write(_phyaddr, regnum, data);
			}

			/**
			 * Read PHY register 'T'
			 */
			template <typename T>
			inline typename T::Register_base::access_t phy_read() const
			{
				typedef typename T::Register_base Register;
				return _phy_read(Register::OFFSET);
			}

			/**
			 * Read the bitfield 'T' of PHY register
			 */
			template <typename T>
			inline typename T::Bitfield_base::Compound_reg::access_t
			phy_read() const
			{
				typedef typename T::Bitfield_base Bitfield;
				typedef typename Bitfield::Compound_reg Register;
				return Bitfield::get(_phy_read(Register::OFFSET));
			}

			/**
			 * Write PHY register 'T'
			 */
			template <typename T>
			inline void phy_write(uint16_t const val) const
			{
				typedef typename T::Register_base Register;
				return _phy_write(Register::OFFSET, val);
			}

			/**
			 * Detect PHY address.
			 */
			void phy_detection()
			{
				/* check _phyaddr */
				if (_phyaddr != -1) {
					Bmsr::access_t phyreg = phy_read<Bmsr>();
					if ((phyreg != Bmsr::INVALID) &&
						 Bmsr::Full_10::get(phyreg) &&
						 Bmsr::Anegcapable::get(phyreg)) {
						/* Found a valid PHY address */
						log("default phy address ", (int)_phyaddr, " is valid");
						return;
					} else {
						log("PHY address is not setup correctly ", (int)_phyaddr);
						_phyaddr = -1;
					}
				}

				log("detecting phy address");
				if (_phyaddr == -1) {
					/* detect the PHY address */
					for (int i = 31; i >= 0; i--) {
						_phyaddr = i;
						Bmsr::access_t phyreg = phy_read<Bmsr>();
						if ((phyreg != Bmsr::INVALID) &&
							 Bmsr::Full_10::get(phyreg) &&
							 Bmsr::Anegcapable::get(phyreg)) {
							/* Found a valid PHY address */
							log("found valid phy address, ", i);
							return;
						}
					}
				}
				warning("PHY is not detected");
				_phyaddr = -1;
			}

			uint32_t get_phy_id()
			{
				uint32_t phy_id = 0;

				/* Grab the bits from PHYIR1, and put them
				 * in the upper half */
				phy_id = phy_read<Idr1>() << 16;

				/* Grab the bits from PHYIR2, and put them in the lower half */
				phy_id |= phy_read<Idr2>();

				return phy_id;
			}

			void m88e1310_config()
			{
				/* LED link and activity */
				phy_write<Page_select>(0x0003);
				Led_ctrl::access_t led = phy_read<Led_ctrl>();
				Led_ctrl::Data::set(led, 0x1);
				phy_write<Led_ctrl>(led);

				/* Set LED2/INT to INT mode, low active */
				phy_write<Page_select>(0x0003);
				Irq_en::access_t irq = phy_read<Irq_en>();
				irq = (irq & 0x77ff) | 0x0880;
				phy_write<Irq_en>(irq);

				/* Set RGMII delay */
				phy_write<Page_select>(0x0002);
				RGMII_ctrl::access_t ctrl = phy_read<RGMII_ctrl>();
				ctrl |= 0x0030;
				phy_write<RGMII_ctrl>(ctrl);

				/* Ensure to return to page 0 */
				phy_write<Page_select>(0x0000);

				genphy_config_aneg();
				phy_reset();
			}

			int genphy_config_aneg()
			{
				/**
				 * Description: If auto-negotiation is enabled, we configure the
				 *   advertising, and then restart auto-negotiation.  If it is not
				 *   enabled, then we write the BMCR.
				 */
				int result = genphy_config_advert();

				if (result < 0) /* error */
					return result;

				if (result == 0) {
					log("config not changed");
					/* Advertisment hasn't changed, but maybe aneg was never on to
					 * begin with?  Or maybe phy was isolated? */
					uint16_t ctl = phy_read<Bmcr>();

					if (!Bmcr::Anenable::get(ctl) || Bmcr::Isolate::get(ctl))
						result = 1; /* do restart aneg */
				} else {
					log("config changed");
				}

				/* Only restart aneg if we are advertising something different
				 * than we were before.	 */
				if (result > 0)
					result = genphy_restart_aneg();

				return result;
			}

			int genphy_config_advert()
			{
				/**
				 * Description: Writes MII_ADVERTISE with the appropriate values,
				 *   after sanitizing the values to make sure we only advertise
				 *   what is supported.  Returns < 0 on error, 0 if the PHY's advertisement
				 *   hasn't changed, and > 0 if it has changed.
				 */
				int changed = 0;

				/* Setup standard advertisement */
				Advertise::access_t adv = phy_read<Advertise>();

				Advertise::access_t oldadv = adv;

				Advertise::Base4_100::set(adv, 0);
				Advertise::Pause_cap::set(adv, 1);
				Advertise::Pause_asym::set(adv, 1);
				Advertise::Half_10::set(adv, 1);
				Advertise::Full_10::set(adv, 1);
				Advertise::Half_100::set(adv, 1);
				Advertise::Full_100::set(adv, 1);

				if (adv != oldadv) {
					phy_write<Advertise>(adv);
					changed = 1;
				}

				/* Configure gigabit if it's supported */
				adv = phy_read<Ctrl1000>();

				oldadv = adv;

				Ctrl1000::Full_1000::set(adv, 1);
				Ctrl1000::Half_1000::set(adv, 1);

				if (adv != oldadv) {
					phy_write<Ctrl1000>(adv);
					changed = 1;
				}

				return changed;
			}

			int genphy_restart_aneg()
			{
				Bmcr::access_t ctl = phy_read<Bmcr>();
				Bmcr::Anenable::set(ctl, 1);
				Bmcr::Anrestart::set(ctl, 1);

				/* Don't isolate the PHY if we're negotiating */
				Bmcr::Isolate::set(ctl, 0);

				phy_write<Bmcr>(ctl);

				return 0;
			}

			int phy_reset()
			{
				int timeout = 500;

				Bmcr::access_t reg = phy_read<Bmcr>();
				Bmcr::Reset::set(reg, 1);
				phy_write<Bmcr>(reg);

				/*
				 * Poll the control register for the reset bit to go to 0 (it is
				 * auto-clearing).  This should happen within 0.5 seconds per the
				 * IEEE spec.
				 */
				while (phy_read<Bmcr::Reset>() && timeout--) {
					_timer.msleep(1);
				}

				if (phy_read<Bmcr::Reset>()) {
					warning("PHY reset timed out");
					throw Phy_timeout_after_reset();
				}

				return 0;
			}

			int m88e1011s_startup()
			{
				genphy_update_link();
				m88e1xxx_parse_status();

				return 0;
			}

			int genphy_update_link()
			{
				/**
				 * Description: Update the value in phydev->link to reflect the
				 *   current link value.  In order to do this, we need to read
				 *   the status register twice, keeping the second value.
				 */

				/*
				 * Wait if the link is up, and autonegotiation is in progress
				 * (ie - we're capable and it's not done)
				 */
				Bmsr::access_t mii_reg = phy_read<Bmsr>();

				/*
				 * If we already saw the link up, and it hasn't gone down, then
				 * we don't need to wait for autoneg again
				 */
				if (_link_up && Bmsr::Lstatus::get(mii_reg))
					return 0;

				if ( Bmsr::Anegcapable::get(mii_reg) && !Bmsr::Anegcomplete::get(mii_reg) ) {
					int i = 0;

					Genode::log("waiting for PHY auto negotiation to complete");
					while (!Bmsr::Anegcomplete::get(mii_reg)) {
						/*
						 * Timeout reached ?
						 */
						if (i > PHY_AUTONEGOTIATE_TIMEOUT) {
							warning(" TIMEOUT !");
							_link_up = false;
							return 0;
						}

						if ((i++ % 500) == 0)
							Genode::printf(".");
						_timer.msleep(1);

						mii_reg = phy_read<Bmsr>();
					}
					Genode::log(" done");
					_link_up = true;
				} else {
					/* Read the link a second time to clear the latched state */
					mii_reg = phy_read<Bmsr>();

					if (Bmsr::Lstatus::get(mii_reg))
						_link_up = true;
					else
						_link_up = false;
				}

				return 0;
			}

			int m88e1xxx_parse_status()
			{
				/**
				 * Parse the 88E1011's status register for speed and duplex
				 * information
				 */
				Phy_stat::access_t stat = phy_read<Phy_stat>();

				if ( Phy_stat::Link::get(stat) &&
					 !Phy_stat::Spddone::get(stat)) {
					int i = 0;

					log("waiting for PHY realtime link");
					while (!phy_read<Phy_stat::Spddone>()) {
						/* Timeout reached ? */
						if (i > PHY_AUTONEGOTIATE_TIMEOUT) {
							warning(" TIMEOUT !");
							_link_up = false;
							break;
						}

						if ((i++ % 1000) == 0)
							Genode::printf(".");
						_timer.msleep(1);
					}
					log(" done");
					_timer.msleep(500);
				} else {
					if (Phy_stat::Link::get(stat))
						_link_up = true;
					else
						_link_up = false;
				}

				// TODO change emac configuration if half duplex

				if (Phy_stat::Speed_1000::get(stat))
					_eth_speed = SPEED_1000;
				else if (Phy_stat::Speed_100::get(stat))
					_eth_speed = SPEED_100;
				else
					_eth_speed = SPEED_10;

				return 0;
			}


		public:
			Marvel_phy(Phyio& phyio) :
				_phyio(phyio),
				_phyaddr(0),
				_link_up(false),
				_eth_speed(UNDEFINED)
			{ }

			void init()
			{
				phy_detection();

				uint32_t phy_id = get_phy_id();
				log("the found phy has the id ", Hex(phy_id));

				phy_reset();
				m88e1310_config();
				m88e1011s_startup();
			}

			Eth_speed eth_speed() { return _eth_speed; }

	};
}

#endif /* _INCLUDE__DRIVERS__NIC__GEM__MARVELL_PHY_H_ */

