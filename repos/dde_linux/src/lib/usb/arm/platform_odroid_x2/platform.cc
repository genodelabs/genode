/*
 * \brief  EHCI for Odroid-x2 initializaion code
 * \author Sebastian Sumpf
 * \author Alexy Gallardo Segura <alexy@uclv.cu>
 * \author Humberto Lopez Leon <humberto@uclv.cu>
 * \author Reinir Millo Sanchez <rmillo@uclv.cu>
 * \date   2015-07-08
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <drivers/board_base.h>
#include <os/attached_io_mem_dataspace.h>
#include <io_mem_session/connection.h>
#include <regulator/consts.h>
#include <regulator_session/connection.h>
#include <timer_session/connection.h>
#include <irq_session/connection.h>
#include <util/mmio.h>

/* Emulation */
#include <platform/platform.h>
#include <extern_c_begin.h>
#include <lx_emul.h>
#include <extern_c_end.h>
#include <platform.h>

#include <usb_masks.h>

using namespace Genode;

enum {
	/*The EHCI base is taken  from linux kernel */
	EHCI_BASE     = 0x12580000,   
	GPIO_BASE     = 0x11000000,
	USBOTG	      = 0x125B0000,
	EHCI_IRQ      = Board_base::USB_HOST20_IRQ,
};

static resource _ehci[] =
{
	{ EHCI_BASE, EHCI_BASE + 0xfff, "ehci", IORESOURCE_MEM },
	{ EHCI_IRQ, EHCI_IRQ, "ehci-irq", IORESOURCE_IRQ },
};


/**
 * EHCI controller
 */
struct Ehci : Genode::Mmio
{
	Ehci(addr_t const mmio_base) : Mmio(mmio_base)
	{
		write<Cmd>(0);

		/* reset */
		write<Cmd::Reset>(1);

		while(read<Cmd::Reset>())
			msleep(1);
	}

	struct Cmd : Register<0x10, 32>
	{
		struct Reset : Bitfield<1, 1> { };
	};
};


/**
 * Gpio handling
 */
struct Gpio_bank
{
	unsigned con;
	unsigned dat;
};

/**
 * ETC handling 
 */
struct Etc6pud_bank
{
	unsigned value;	
};

/**
 * Usbotg handling
 */ 
struct Usbotg_bank
{
	unsigned phypwr;
	unsigned phyclk;
	unsigned rstcon;
};


static inline
unsigned con_mask(unsigned val) { return 0xf << ((val) << 2); } 


static inline
unsigned con_sfr(unsigned x, unsigned v) { return (v) << ((x) << 2); }


static void gpio_cfg_pin(Gpio_bank *bank, int gpio, int cfg)
{
	unsigned int value;

	value = readl(&bank->con);
	value &= ~con_mask(gpio);
	value |= con_sfr(gpio, cfg);
	writel(value, &bank->con);
}


static void gpio_direction_output(Gpio_bank *bank, int gpio, int en)
{
	unsigned int value;
	enum { GPIO_OUTPUT = 0x1 };

	value = readl(&bank->dat);
	value &= ~(0x1 << gpio);
	if (en)
		value |= 0x1 << gpio;
	writel(value, &bank->dat);
	gpio_cfg_pin(bank, gpio, GPIO_OUTPUT);
	writel(value, &bank->dat);
	
}

static void clock_pwr_init()
{
	enum Gpio_register {ETC6PUD = 0x0228};

	Io_mem_connection io_gpio(GPIO_BASE, 0x1000);
	addr_t gpio_base = (addr_t)env()->rm_session()->attach(io_gpio.dataspace());
	Etc6pud_bank *etc6pud = reinterpret_cast<Etc6pud_bank *>(gpio_base + ETC6PUD);
    
	unsigned int value;
	value = readl(&etc6pud->value);
	writel((value & ~(0x3 << 14)) | (0x3 << 14),&etc6pud->value);
	value = readl(&etc6pud->value);
	env()->rm_session()->detach(gpio_base);  
    
	/* enable USB2 clock and power up */
	static Regulator::Connection reg_clk(Regulator::CLK_USB20);
	reg_clk.state(true);

	static Regulator::Connection reg_pwr(Regulator::PWR_USB20);
	reg_pwr.state(true); 
}

static void usb_phy_init()
{ 
	Timer::Connection timer; 
    
	Io_mem_connection io_usbotg(USBOTG, 0x1000);
	addr_t usbotg_base = (addr_t)env()->rm_session()->attach(io_usbotg.dataspace());
	Usbotg_bank *usbotg = reinterpret_cast<Usbotg_bank *>(usbotg_base);
    
	int rstcon = 0;
	int phyclk = 5;
	int phypwr = 0;
    
	writel(phyclk,&usbotg->phyclk);
	rstcon = readl(&usbotg->phyclk);
    
	/* set to normal of Device */
	phypwr = readl(&usbotg->phypwr) & ~PHY0_NORMAL_MASK;
	writel(phypwr,&usbotg->phypwr);
    
	/* set to normal of Host */
	phypwr = readl(&usbotg->phypwr);
	phypwr &= ~(PHY1_STD_NORMAL_MASK
		|EXYNOS4X12_HSIC0_NORMAL_MASK
		|EXYNOS4X12_HSIC1_NORMAL_MASK);
    
	writel(phypwr,&usbotg->phypwr);
    
	/* reset both PHY and Link of Device */
	rstcon = readl(&usbotg->rstcon) | PHY0_SWRST_MASK;
	writel(rstcon,&usbotg->rstcon);
    
	timer.usleep(10);

	rstcon &= ~PHY0_SWRST_MASK;
	writel(rstcon,&usbotg->rstcon);
    
	/* reset both PHY and Link of Host */
	rstcon = readl(&usbotg->rstcon)
		|EXYNOS4X12_HOST_LINK_PORT_SWRST_MASK
		|EXYNOS4X12_PHY1_SWRST_MASK;	
	writel(rstcon,&usbotg->rstcon);
	timer.usleep(10);
	
	rstcon &= ~(EXYNOS4X12_HOST_LINK_PORT_SWRST_MASK
		|EXYNOS4X12_PHY1_SWRST_MASK);
	writel(rstcon,&usbotg->rstcon);
	timer.usleep(10);
	env()->rm_session()->detach(usbotg_base);
}

static void odroidx2_ehci_init()
{
	clock_pwr_init();
	usb_phy_init();

	/*X3 Register name GPX3CON in DataSheet*/    
	enum Gpio_offset {X3 = 0x0c60 };
	
	/* reset hub via GPIO */
	Io_mem_connection io_gpio(GPIO_BASE, 0x1000);
	addr_t gpio_base = (addr_t)env()->rm_session()->attach(io_gpio.dataspace());

	Gpio_bank *x3 = reinterpret_cast<Gpio_bank *>(gpio_base + X3);

	/* Set Ref freq 0 => 24MHz, 1 => 26MHz*/
	/* Odroid Us have it at 24MHz, Odroid Xs at 26MHz */
	gpio_direction_output(x3, 0, 1);

	/* Disconnect, Reset, Connect */
	gpio_direction_output(x3, 4, 0);
	gpio_direction_output(x3, 5, 0);
	gpio_direction_output(x3, 5, 1);
	gpio_direction_output(x3, 4, 1);

	
	env()->rm_session()->detach(gpio_base);

	/* reset ehci controller */
	Io_mem_connection io_ehci(EHCI_BASE, 0x1000);
	addr_t ehci_base = (addr_t)env()->rm_session()->attach(io_ehci.dataspace());

	Ehci ehci(ehci_base);
	
	env()->rm_session()->detach(ehci_base);
}

extern "C" void module_ehci_exynos_init();
extern "C" int  module_usbnet_init();
extern "C" int  module_smsc95xx_driver_init();

void ehci_setup(Services *services)
{

	/* register network */
	if (services->nic){
		module_usbnet_init();
		module_smsc95xx_driver_init();
	}

	/* register EHCI controller */
	module_ehci_exynos_init();

	/* setup controller */
	odroidx2_ehci_init();

	/* setup EHCI-controller platform device */
	platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
	pdev->name            = (char *)"exynos-ehci";
	pdev->id              = 0;
	pdev->num_resources   = 2;
	pdev->resource        = _ehci;

	/*needed for DMA buffer allocation */
	static u64 dma_mask         = ~(u64)0;
	pdev->dev.dma_mask          = &dma_mask;
	pdev->dev.coherent_dma_mask = ~0;

	platform_device_register(pdev);
}

void platform_hcd_init(Services *services)
{
	/* register network */
	if (services->nic){
		module_usbnet_init();
		module_smsc95xx_driver_init();
	}
	/* register ehci */
	if (services->ehci)
		ehci_setup(services);
}


Genode::Irq_session_capability platform_irq_activate(int irq)
{
	try {
		Genode::Irq_connection conn(irq);
		conn.on_destruction(Genode::Irq_connection::KEEP_OPEN);
		return conn;
	} catch (...) { }

	return Genode::Irq_session_capability();
}
