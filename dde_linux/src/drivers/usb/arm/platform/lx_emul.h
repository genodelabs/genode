/*
 * \brief  Platform specific part of the Linux API emulation
 * \author Sebastian Sumpf
 * \date   2012-06-18
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _ARM__PLATFORM__LX_EMUL_H_
#define _ARM__PLATFORM__LX_EMUL_H_

/*************************
 ** asm-generic/sizes.h **
 *************************/

enum {
	SZ_1K = 0x00000400,
	SZ_4K = 0x00001000a,
};

struct platform_device
{
	const char      *name;
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

/*******************************************
 ** arch/arm/plat-omap/include/plat/usb.h **
 *******************************************/

enum { OMAP3_HS_USB_PORTS = 2 };

enum usbhs_omap_port_mode
{
	OMAP_EHCI_PORT_MODE_NONE,
	OMAP_EHCI_PORT_MODE_PHY,
};


struct ehci_hcd_omap_platform_data
{
	enum usbhs_omap_port_mode  port_mode[OMAP3_HS_USB_PORTS];
	struct regulator          *regulator[OMAP3_HS_USB_PORTS];
};

struct regulator;
/*********************************************
 ** arch/arm/plat-omap/include/plat/board.h **
 *********************************************/

struct omap_usb_config;


/************************************************
 ** arch/arm/plat-omap/include/plat/omap34xx.h **
 ************************************************/

#define OMAP34XX_UHH_CONFIG_BASE 0
#define OMAP34XX_EHCI_BASE       0
#define OMAP34XX_USBTLL_BASE     0
#define INT_34XX_EHCI_IRQ        0
#define OMAP34XX_OHCI_BASE       0
#define INT_34XX_OHCI_IRQ        0
#define OMAP3430_REV_ES2_1       0


static inline int cpu_is_omap34xx(void) { return 0; }
static inline int cpu_is_omap3430(void) { return 0; }

/***************************************************
 ** platform definition os for OMAP44xx all under **
 ** 'arch/arm/plat-omap/include                   **
 ***************************************************/

enum {
	OMAP44XX_IRQ_GIC_START = 32,
	OMAP44XX_IRQ_EHCI      = 77 + OMAP44XX_IRQ_GIC_START,
	OMAP44XX_IRQ_OHCI      =76  + OMAP44XX_IRQ_GIC_START,
};

enum {
	L4_44XX_BASE = 0x4a000000,
	OMAP44XX_USBTLL_BASE     = L4_44XX_BASE + 0x62000,
	OMAP44XX_UHH_CONFIG_BASE = L4_44XX_BASE + 0x64000,
	OMAP44XX_HSUSB_OHCI_BASE = L4_44XX_BASE + 0x64800,
	OMAP44XX_HSUSB_EHCI_BASE = L4_44XX_BASE + 0x64C00,
};

static inline int cpu_is_omap44xx(void) { return 1; }

/*****************************
 ** linux/platform_device.h **
 *****************************/

struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
};

struct resource *platform_get_resource_byname(struct platform_device *, unsigned int, const char *);
int platform_get_irq_byname(struct platform_device *, const char *);

int platform_driver_register(struct platform_driver *);
int platform_device_register(struct platform_device *);

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

int    regulator_enable(struct regulator *);
int    regulator_disable(struct regulator *);
void   regulator_put(struct regulator *regulator);
struct regulator *regulator_get(struct device *dev, const char *id);


/*******************************************
 ** arch/arm/plat-omap/include/plat/usb.h **
 *******************************************/

int omap_usbhs_enable(struct device *dev);
void omap_usbhs_disable(struct device *dev);

#endif /* _ARM__PLATFORM__LX_EMUL_H_ */
