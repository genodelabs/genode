/*
 * \brief  Phy driver for Marvell chips
 * \author Timo Wischer
 * \date   2015-05-11
 */

#include "marvell_phy.h"


/* Generic MII registers. */
#define MII_BMCR	    0x00	/* Basic mode control register */
#define MII_BMSR	    0x01	/* Basic mode status register  */
#define MII_PHYSID1	    0x02	/* PHYS ID 1		       */
#define MII_PHYSID2	    0x03	/* PHYS ID 2		       */
#define MII_ADVERTISE	    0x04	/* Advertisement control reg   */
#define MII_LPA		    0x05	/* Link partner ability reg    */
#define MII_EXPANSION	    0x06	/* Expansion register	       */
#define MII_CTRL1000	    0x09	/* 1000BASE-T control	       */
#define MII_STAT1000	    0x0a	/* 1000BASE-T status	       */
#define MII_ESTATUS	    0x0f	/* Extended Status */
#define MII_DCOUNTER	    0x12	/* Disconnect counter	       */
#define MII_FCSCOUNTER	    0x13	/* False carrier counter       */
#define MII_NWAYTEST	    0x14	/* N-way auto-neg test reg     */
#define MII_RERRCOUNTER     0x15	/* Receive error counter       */
#define MII_SREVISION	    0x16	/* Silicon revision	       */
#define MII_RESV1	    0x17	/* Reserved...		       */
#define MII_LBRERROR	    0x18	/* Lpback, rx, bypass error    */
#define MII_PHYADDR	    0x19	/* PHY address		       */
#define MII_RESV2	    0x1a	/* Reserved...		       */
#define MII_TPISTATUS	    0x1b	/* TPI status for 10mbps       */
#define MII_NCONFIG	    0x1c	/* Network interface config    */


/* 88E1310 PHY defines */
#define MIIM_88E1310_PHY_LED_CTRL	16
#define MIIM_88E1310_PHY_IRQ_EN		18
#define MIIM_88E1310_PHY_RGMII_CTRL	21
#define MIIM_88E1310_PHY_PAGE		22


/* Basic mode control register. */
#define BMCR_RESV		0x003f	/* Unused...		       */
#define BMCR_SPEED1000		0x0040	/* MSB of Speed (1000)	       */
#define BMCR_CTST		0x0080	/* Collision test	       */
#define BMCR_FULLDPLX		0x0100	/* Full duplex		       */
#define BMCR_ANRESTART		0x0200	/* Auto negotiation restart    */
#define BMCR_ISOLATE		0x0400	/* Disconnect DP83840 from MII */
#define BMCR_PDOWN		0x0800	/* Powerdown the DP83840       */
#define BMCR_ANENABLE		0x1000	/* Enable auto negotiation     */
#define BMCR_SPEED100		0x2000	/* Select 100Mbps	       */
#define BMCR_LOOPBACK		0x4000	/* TXD loopback bits	       */
#define BMCR_RESET		0x8000	/* Reset the DP83840	       */

/* Basic mode status register. */
#define BMSR_ERCAP		0x0001	/* Ext-reg capability	       */
#define BMSR_JCD		0x0002	/* Jabber detected	       */
#define BMSR_LSTATUS		0x0004	/* Link status		       */
#define BMSR_ANEGCAPABLE	0x0008	/* Able to do auto-negotiation */
#define BMSR_RFAULT		0x0010	/* Remote fault detected       */
#define BMSR_ANEGCOMPLETE	0x0020	/* Auto-negotiation complete   */
#define BMSR_RESV		0x00c0	/* Unused...		       */
#define BMSR_ESTATEN		0x0100	/* Extended Status in R15 */
#define BMSR_100HALF2		0x0200	/* Can do 100BASE-T2 HDX */
#define BMSR_100FULL2		0x0400	/* Can do 100BASE-T2 FDX */
#define BMSR_10HALF		0x0800	/* Can do 10mbps, half-duplex  */
#define BMSR_10FULL		0x1000	/* Can do 10mbps, full-duplex  */
#define BMSR_100HALF		0x2000	/* Can do 100mbps, half-duplex */
#define BMSR_100FULL		0x4000	/* Can do 100mbps, full-duplex */
#define BMSR_100BASE4		0x8000	/* Can do 100mbps, 4k packets  */


/* Advertisement control register. */
#define ADVERTISE_SLCT		0x001f	/* Selector bits	       */
#define ADVERTISE_CSMA		0x0001	/* Only selector supported     */
#define ADVERTISE_10HALF	0x0020	/* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL	0x0020	/* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL	0x0040	/* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF	0x0040	/* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF	0x0080	/* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE	0x0080	/* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL	0x0100	/* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM 0x0100	/* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4	0x0200	/* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP	0x0400	/* Try for pause	       */
#define ADVERTISE_PAUSE_ASYM	0x0800	/* Try for asymetric pause     */
#define ADVERTISE_RESV		0x1000	/* Unused...		       */
#define ADVERTISE_RFAULT	0x2000	/* Say we can detect faults    */
#define ADVERTISE_LPACK		0x4000	/* Ack link partners response  */
#define ADVERTISE_NPAGE		0x8000	/* Next page bit	       */

#define ADVERTISE_FULL (ADVERTISE_100FULL | ADVERTISE_10FULL | \
			ADVERTISE_CSMA)
#define ADVERTISE_ALL (ADVERTISE_10HALF | ADVERTISE_10FULL | \
			   ADVERTISE_100HALF | ADVERTISE_100FULL)

/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL	0x0200	/* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF	0x0100	/* Advertise 1000BASE-T half duplex */


#define PHY_AUTONEGOTIATE_TIMEOUT 5000

/* 88E1011 PHY Status Register */
#define MIIM_88E1xxx_PHY_STATUS		0x11
#define MIIM_88E1xxx_PHYSTAT_SPEED	0xc000
#define MIIM_88E1xxx_PHYSTAT_GBIT	0x8000
#define MIIM_88E1xxx_PHYSTAT_100	0x4000
#define MIIM_88E1xxx_PHYSTAT_DUPLEX	0x2000
#define MIIM_88E1xxx_PHYSTAT_SPDDONE	0x0800
#define MIIM_88E1xxx_PHYSTAT_LINK	0x0400

#define MIIM_88E1xxx_PHY_SCR		0x10
#define MIIM_88E1xxx_PHY_MDI_X_AUTO	0x0060


/* Mask used to verify certain PHY features (or register contents)
 * in the register above:
 *  0x1000: 10Mbps full duplex support
 *  0x0800: 10Mbps half duplex support
 *  0x0008: Auto-negotiation support
 */
#define PHY_DETECT_MASK 0x1808


namespace Genode {


void Marvel_phy::init()
{
	phy_detection();

	uint32_t phy_id = 0;
	get_phy_id(&phy_id);
	PDBG("The found phy has the id %08x", phy_id);

	phy_reset();
	m88e1310_config();
	m88e1011s_startup();
}


void Marvel_phy::phy_read(const uint32_t regnum, uint16_t *val)
{
	_phyio.phy_read(_phyaddr, regnum, *val);
}


void Marvel_phy::phy_write(const uint32_t regnum, const uint16_t data)
{
	_phyio.phy_write(_phyaddr, regnum, data);
}


void Marvel_phy::phy_detection()
{
	if (_phyaddr != -1) {
		uint16_t phyreg = 0xEEEE;
		phy_read(MII_BMSR, &phyreg);
		if ((phyreg != 0xFFFF) &&
			((phyreg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
			/* Found a valid PHY address */
			PDBG("Default phy address %d is valid\n", _phyaddr);
			return;
		} else {
			PDBG("PHY address is not setup correctly %d\n", _phyaddr);
			_phyaddr = -1;
		}
	}

	PDBG("detecting phy address\n");
	if (_phyaddr == -1) {
		/* detect the PHY address */
		for (int i = 31; i >= 0; i--) {
			uint16_t phyreg = 0xEEEE;
			_phyaddr = i;
			phy_read(MII_BMSR, &phyreg);
			if ((phyreg != 0xFFFF) &&
				((phyreg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
				/* Found a valid PHY address */
				PDBG("Found valid phy address, %d\n", i);
				return;
			}
		}
	}
	PDBG("PHY is not detected\n");
	_phyaddr = -1;
}

/**
 * get_phy_id - reads the specified addr for its ID.
 * @bus: the target MII bus
 * @addr: PHY address on the MII bus
 * @phy_id: where to store the ID retrieved.
 *
 * Description: Reads the ID registers of the PHY at @addr on the
 *   @bus, stores it in @phy_id and returns zero on success.
 */
void Marvel_phy::get_phy_id(uint32_t *phy_id)
{
	uint16_t phy_reg = 0;

	/* Grab the bits from PHYIR1, and put them
	 * in the upper half */
	phy_read(MII_PHYSID1, &phy_reg);
	*phy_id = (phy_reg & 0xffff) << 16;

	/* Grab the bits from PHYIR2, and put them in the lower half */
	phy_read(MII_PHYSID2, &phy_reg);
	*phy_id |= (phy_reg & 0xffff);
}


void Marvel_phy::m88e1310_config()
{
	uint16_t reg = 0;

	/* LED link and activity */
	phy_write(MIIM_88E1310_PHY_PAGE, 0x0003);
	phy_read(MIIM_88E1310_PHY_LED_CTRL, &reg);
	reg = (reg & ~0xf) | 0x1;
	phy_write(MIIM_88E1310_PHY_LED_CTRL, reg);

	/* Set LED2/INT to INT mode, low active */
	phy_write(MIIM_88E1310_PHY_PAGE, 0x0003);
	phy_read(MIIM_88E1310_PHY_IRQ_EN, &reg);
	reg = (reg & 0x77ff) | 0x0880;
	phy_write(MIIM_88E1310_PHY_IRQ_EN, reg);

	/* Set RGMII delay */
	phy_write(MIIM_88E1310_PHY_PAGE, 0x0002);
	phy_read(MIIM_88E1310_PHY_RGMII_CTRL, &reg);
	reg |= 0x0030;
	phy_write(MIIM_88E1310_PHY_RGMII_CTRL, reg);

	/* Ensure to return to page 0 */
	phy_write(MIIM_88E1310_PHY_PAGE, 0x0000);

	genphy_config_aneg();
	phy_reset();
}


/**
 * genphy_config_aneg - restart auto-negotiation or write BMCR
 * @phydev: target phy_device struct
 *
 * Description: If auto-negotiation is enabled, we configure the
 *   advertising, and then restart auto-negotiation.  If it is not
 *   enabled, then we write the BMCR.
 */
int Marvel_phy::genphy_config_aneg()
{
	int result = genphy_config_advert();

	if (result < 0) /* error */
		return result;

	if (result == 0) {
		PDBG("Config not changed");
		/* Advertisment hasn't changed, but maybe aneg was never on to
		 * begin with?  Or maybe phy was isolated? */
		uint16_t ctl = 0;
		phy_read(MII_BMCR, &ctl);

		if (!(ctl & BMCR_ANENABLE) || (ctl & BMCR_ISOLATE))
			result = 1; /* do restart aneg */
	} else {
		PDBG("Config changed");
	}

	/* Only restart aneg if we are advertising something different
	 * than we were before.	 */
	if (result > 0)
		result = genphy_restart_aneg();

	return result;
}


/**
 * genphy_config_advert - sanitize and advertise auto-negotation parameters
 * @phydev: target phy_device struct
 *
 * Description: Writes MII_ADVERTISE with the appropriate values,
 *   after sanitizing the values to make sure we only advertise
 *   what is supported.  Returns < 0 on error, 0 if the PHY's advertisement
 *   hasn't changed, and > 0 if it has changed.
 */
int Marvel_phy::genphy_config_advert()
{
	uint16_t adv = 0;
	int changed = 0;

	/* Setup standard advertisement */
	phy_read(MII_ADVERTISE, &adv);

	uint16_t oldadv = adv;

	adv &= ~(ADVERTISE_ALL | ADVERTISE_100BASE4 | ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);
	adv |= ADVERTISE_10HALF;
	adv |= ADVERTISE_10FULL;
	adv |= ADVERTISE_100HALF;
	adv |= ADVERTISE_100FULL;
	adv |= ADVERTISE_PAUSE_CAP;
	adv |= ADVERTISE_PAUSE_ASYM;

	if (adv != oldadv) {
		phy_write(MII_ADVERTISE, adv);
		changed = 1;
	}

	/* Configure gigabit if it's supported */
	phy_read(MII_CTRL1000, &adv);

	oldadv = adv;


	adv &= ~(ADVERTISE_1000FULL | ADVERTISE_1000HALF);
	adv |= ADVERTISE_1000HALF;
	adv |= ADVERTISE_1000FULL;

	if (adv != oldadv) {
		phy_write(MII_CTRL1000, adv);
		changed = 1;
	}

	return changed;
}

/**
 * genphy_restart_aneg - Enable and Restart Autonegotiation
 * @phydev: target phy_device struct
 */
int Marvel_phy::genphy_restart_aneg()
{
	uint16_t ctl = 0;
	phy_read(MII_BMCR, &ctl);

	ctl |= (BMCR_ANENABLE | BMCR_ANRESTART);

	/* Don't isolate the PHY if we're negotiating */
	ctl &= ~(BMCR_ISOLATE);

	phy_write(MII_BMCR, ctl);

	return 0;
}


int Marvel_phy::phy_reset()
{
	uint16_t reg = 0;
	int timeout = 500;

	phy_read(MII_BMCR, &reg);

	reg |= BMCR_RESET;

	phy_write(MII_BMCR, reg);

	/*
	 * Poll the control register for the reset bit to go to 0 (it is
	 * auto-clearing).  This should happen within 0.5 seconds per the
	 * IEEE spec.
	 */
	while ((reg & BMCR_RESET) && timeout--) {
		phy_read(MII_BMCR, &reg);
		_timer.msleep(1);
	}

	if (reg & BMCR_RESET) {
		PWRN("PHY reset timed out\n");
		throw Phy_timeout_after_reset();
	}

	return 0;
}


int Marvel_phy::m88e1011s_startup()
{
	genphy_update_link();
	m88e1xxx_parse_status();

	return 0;
}



/**
 * genphy_update_link - update link status in @phydev
 * @phydev: target phy_device struct
 *
 * Description: Update the value in phydev->link to reflect the
 *   current link value.  In order to do this, we need to read
 *   the status register twice, keeping the second value.
 */
int Marvel_phy::genphy_update_link()
{
	uint16_t mii_reg = 0;

	/*
	 * Wait if the link is up, and autonegotiation is in progress
	 * (ie - we're capable and it's not done)
	 */
	phy_read(MII_BMSR, &mii_reg);

	/*
	 * If we already saw the link up, and it hasn't gone down, then
	 * we don't need to wait for autoneg again
	 */
	if (_link_up && mii_reg & BMSR_LSTATUS)
		return 0;

	if ((mii_reg & BMSR_ANEGCAPABLE) && !(mii_reg & BMSR_ANEGCOMPLETE)) {
		int i = 0;

		Genode::printf("Waiting for PHY auto negotiation to complete");
		while (!(mii_reg & BMSR_ANEGCOMPLETE)) {
			/*
			 * Timeout reached ?
			 */
			if (i > PHY_AUTONEGOTIATE_TIMEOUT) {
				PWRN(" TIMEOUT !\n");
				_link_up = false;
				return 0;
			}

			if ((i++ % 500) == 0)
				Genode::printf(".");
			_timer.msleep(1);

			phy_read(MII_BMSR, &mii_reg);
		}
		Genode::printf(" done\n");
		_link_up = true;
	} else {
		/* Read the link a second time to clear the latched state */
		phy_read(MII_BMSR, &mii_reg);

		if (mii_reg & BMSR_LSTATUS)
			_link_up = true;
		else
			_link_up = false;
	}

	return 0;
}


/* Parse the 88E1011's status register for speed and duplex
 * information
 */
int Marvel_phy::m88e1xxx_parse_status()
{
	unsigned int speed;
	uint16_t mii_reg = 0;

	phy_read(MIIM_88E1xxx_PHY_STATUS, &mii_reg);

	if ((mii_reg & MIIM_88E1xxx_PHYSTAT_LINK) &&
		!(mii_reg & MIIM_88E1xxx_PHYSTAT_SPDDONE)) {
		int i = 0;

		PDBG("Waiting for PHY realtime link");
		while (!(mii_reg & MIIM_88E1xxx_PHYSTAT_SPDDONE)) {
			/* Timeout reached ? */
			if (i > PHY_AUTONEGOTIATE_TIMEOUT) {
				PWRN(" TIMEOUT !");
				_link_up = false;
				break;
			}

			if ((i++ % 1000) == 0)
				Genode::printf(".");
			_timer.msleep(1);
			phy_read(MIIM_88E1xxx_PHY_STATUS, &mii_reg);
		}
		PINF(" done");
		_timer.msleep(500);
	} else {
		if (mii_reg & MIIM_88E1xxx_PHYSTAT_LINK)
			_link_up = true;
		else
			_link_up = false;
	}

	// TODO change emac configuration if half duplex
//				if (mii_reg & MIIM_88E1xxx_PHYSTAT_DUPLEX)
//					phydev->duplex = DUPLEX_FULL;
//				else
//					phydev->duplex = DUPLEX_HALF;

	speed = mii_reg & MIIM_88E1xxx_PHYSTAT_SPEED;

	switch (speed) {
	case MIIM_88E1xxx_PHYSTAT_GBIT:
		_eth_speed = SPEED_1000;
		break;
	case MIIM_88E1xxx_PHYSTAT_100:
		_eth_speed = SPEED_100;
		break;
	default:
		_eth_speed = SPEED_10;
		break;
	}

	return 0;
}

}
