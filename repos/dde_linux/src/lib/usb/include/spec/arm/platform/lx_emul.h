/*
 * \brief  Platform specific part of the Linux API emulation
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ARM__PLATFORM__LX_EMUL_H_
#define _ARM__PLATFORM__LX_EMUL_H_

#include <platform/lx_emul_barrier.h>

/*************************
 ** asm-generic/sizes.h **
 *************************/

enum {
	SZ_1K = 0x00000400,
	SZ_4K = 0x00001000,
};

struct platform_device
{
	char            *name;
	int              id;
	struct device    dev;
	u32              num_resources;
	struct resource *resource;
};


/**********************
 ** linux/usb/ulpi.h **
 **********************/

enum {
	ULPI_FUNC_CTRL_RESET = (1 << 5),
	ULPI_FUNC_CTRL       = (1 << 2),
};

/*
 * Macros for Set and Clear
 * See ULPI 1.1 specification to find the registers with Set and Clear offsets
 */

#define ULPI_SET(a) (a + 1)


/*****************************
 ** linux/platform_device.h **
 *****************************/

#define module_platform_driver(__platform_driver) \
        module_driver(__platform_driver, platform_driver_register, \
                      platform_driver_unregister)

enum { PLATFORM_DEVID_AUTO = -2 };

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
};

struct resource *platform_get_resource(struct platform_device *, unsigned, unsigned);
struct resource *platform_get_resource_byname(struct platform_device *, unsigned int, const char *);

int platform_get_irq(struct platform_device *, unsigned int);
int platform_get_irq_byname(struct platform_device *, const char *);

int platform_driver_register(struct platform_driver *);
int platform_device_register(struct platform_device *);
void platform_device_unregister(struct platform_device *);

struct platform_device *platform_device_alloc(const char *name, int id);
int platform_device_add_data(struct platform_device *pdev, const void *data,
                             size_t size);
int platform_device_add_resources(struct platform_device *pdev,
                                  const struct resource *res, unsigned int num);

int platform_device_add(struct platform_device *pdev);
int platform_device_del(struct platform_device *pdev);

int platform_device_put(struct platform_device *pdev);


#define to_platform_device(x) container_of((x), struct platform_device, dev)

/**********************
 ** asm/generic/io.h **
 **********************/

static inline u32 __raw_readl(const volatile void __iomem *addr)
{
	return *(const volatile u32 __force *) addr;
}

static inline void __raw_writel(u32 b, volatile void __iomem *addr)
{
	*(volatile u32 __force *) addr = b;
}


static inline u8 __raw_readb(const volatile void __iomem *addr)
{
	return *(const volatile u8 __force *) addr;
}

static inline void __raw_writeb(u8 b, volatile void __iomem *addr)
{
	*(volatile u8 __force *) addr = b;
}


/********************************
 ** linux/regulator/consumer.h **
 ********************************/

struct regulator { };
int    regulator_enable(struct regulator *);
int    regulator_disable(struct regulator *);
void   regulator_put(struct regulator *regulator);
struct regulator *regulator_get(struct device *dev, const char *id);

struct regulator *__must_check devm_regulator_get(struct device *dev,
                                                  const char *id);

/*******************************************
 ** arch/arm/plat-omap/include/plat/usb.h **
 *******************************************/

int omap_usbhs_enable(struct device *dev);
void omap_usbhs_disable(struct device *dev);


/*****************
 ** linux/clk.h **
 *****************/

struct clk *clk_get(struct device *, const char *);
int    clk_enable(struct clk *);
void   clk_disable(struct clk *);
void   clk_put(struct clk *);
struct clk *devm_clk_get(struct device *dev, const char *id);
int    clk_prepare_enable(struct clk *);
void   clk_disable_unprepare(struct clk *);


/******************
 ** linux/gpio.h **
 ******************/

enum { GPIOF_OUT_INIT_HIGH = 0x2 };

bool gpio_is_valid(int);
void gpio_set_value_cansleep(unsigned, int);
int  gpio_request_one(unsigned, unsigned long, const char *);

int devm_gpio_request_one(struct device *dev, unsigned gpio,
                          unsigned long flags, const char *label);




/****************
 ** linux/of.h **
 ****************/

#define of_match_ptr(ptr) NULL
#define for_each_available_child_of_node(parent, child) while (0)


unsigned of_usb_get_maximum_speed(struct device_node *np);
unsigned of_usb_get_dr_mode(struct device_node *np);
int      of_device_is_compatible(const struct device_node *device, const char *);
void     of_node_put(struct device_node *node);

int of_property_read_u32(const struct device_node *np, const char *propname,
                         u32 *out_value);


/*************************
 ** linux/of_platform.h **
 *************************/

struct of_dev_auxdata;

int of_platform_populate(struct device_node *, const struct of_device_id *,
                         const struct of_dev_auxdata *, struct device *);


/*********************
 ** linux/of_gpio.h **
 *********************/

int of_get_named_gpio(struct device_node *, const char *, int);


/******************
 ** linux/phy.h  **
 ******************/

enum {
	MII_BUS_ID_SIZE = 17,
	PHY_MAX_ADDR    = 32,
	PHY_POLL        = -1,
};


#define PHY_ID_FMT "%s:%02x"

struct mii_bus
{
	const char *name;
	char id[MII_BUS_ID_SIZE];
	int (*read)(struct mii_bus *bus, int phy_id, int regnum);
	int (*write)(struct mii_bus *bus, int phy_id, int regnum, u16 val);
	void *priv;
	int *irq;
};

struct phy_device
{
	int speed;
	int duplex;
	int link;
};

struct mii_bus *mdiobus_alloc(void);
int  mdiobus_register(struct mii_bus *bus);
void mdiobus_unregister(struct mii_bus *bus);
void mdiobus_free(struct mii_bus *bus);

int  phy_mii_ioctl(struct phy_device *phydev, struct ifreq *ifr, int cmd);
void phy_print_status(struct phy_device *phydev);
int  phy_ethtool_sset(struct phy_device *phydev, struct ethtool_cmd *cmd);
int  phy_ethtool_gset(struct phy_device *phydev, struct ethtool_cmd *cmd);
void phy_start(struct phy_device *phydev);
int  phy_start_aneg(struct phy_device *phydev);
void phy_stop(struct phy_device *phydev);
int  phy_create_lookup(struct phy *phy, const char *con_id, const char *dev_id);
void phy_remove_lookup(struct phy *phy, const char *con_id, const char *dev_id);


int  genphy_resume(struct phy_device *phydev);

struct phy_device * phy_connect(struct net_device *dev, const char *bus_id,
                                void (*handler)(struct net_device *),
                                phy_interface_t interface);
void phy_disconnect(struct phy_device *phydev);

struct phy *devm_phy_get(struct device *dev, const char *string);
struct phy *devm_of_phy_get(struct device *dev, struct device_node *np,
                            const char *con_id);


/*********************************
 ** linux/usb/usb_phy_generic.h **
 *********************************/

#include <linux/usb/ch9.h>
#include <linux/usb/phy.h>

struct usb_phy_generic_platform_data
{
	enum usb_phy_type type;
	int  gpio_reset;
};



/*******************************
 ** linux/usb/nop-usb-xceiv.h **
 *******************************/

struct nop_usb_xceiv_platform_data { int type; };


/*******************************
 ** linux/usb/samsung_usb_phy **
 *******************************/

enum samsung_usb_phy_type { USB_PHY_TYPE_HOST = 1 };


/***********************
 ** asm/dma-mapping.h **
 ***********************/

/* needed by 'dwc_otg_hcd_linux.c' */
void *dma_to_virt(struct device *dev, dma_addr_t addr);


/********************
 ** asm/irqflags.h **
 ********************/

void local_fiq_disable();
void local_fiq_enable();
unsigned smp_processor_id(void);

/***************
 ** asm/fiq.h **
 ***************/

struct pt_regs;
int claim_fiq(struct fiq_handler *f);
void set_fiq_regs(struct pt_regs const *regs);
void enable_fiq();
void set_fiq_handler(void *start, unsigned int length);


/************************************
 ** /linux/usb/usb_phy_gen_xceiv.h **
 ************************************/


struct usb_phy_gen_xceiv_platform_data
{
 unsigned type;
 int      gpio_reset;
};


/******************************
 ** linux/usb/xhci_pdriver.h **
 ******************************/

struct usb_xhci_pdata {
	unsigned usb3_lpm_capable:1;
};


/******************
 ** asm/memory.h **
 ******************/

#define __bus_to_virt phys_to_virt


/********************************************************
 ** drivers/usb/host/dwc_otg/dwc_otg/dwc_otg_fiq_fsm.h **
 ********************************************************/

extern bool fiq_enable, fiq_fsm_enable;

#endif /* _ARM__PLATFORM__LX_EMUL_H_ */
