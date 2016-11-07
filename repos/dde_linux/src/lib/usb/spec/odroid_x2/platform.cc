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
#include <gpio_session/connection.h>

/* Emulation */
#include <lx_emul.h>
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
 * USB OTG handling
 */
struct Usb_Otg : Genode::Mmio
{
	Usb_Otg(Genode::addr_t base):Genode::Mmio (base)
	{
		Timer::Connection timer;
		unsigned int rstcon_mask = 0;
		unsigned int phyclk_mask = 5;
		unsigned int phypwr_mask = 0;

		/*set the clock of device*/
		write<Phyclk>(phyclk_mask);
		rstcon_mask= read<Phyclk>();

		/* set to normal of Device */
		phypwr_mask= read<Phypwr>() & ~PHY0_NORMAL_MASK;
		write<Phypwr>(phypwr_mask);

		/* set to normal of Host */
		phypwr_mask=read<Phypwr>();
		phypwr_mask &= ~(PHY1_STD_NORMAL_MASK
		        |EXYNOS4X12_HSIC0_NORMAL_MASK
		        |EXYNOS4X12_HSIC1_NORMAL_MASK);
		write<Phypwr>(phypwr_mask);

		/* reset both PHY and Link of Device */
		rstcon_mask = read<Rstcon>() | PHY0_SWRST_MASK;
		write<Rstcon>(rstcon_mask);
		timer.usleep(10);
		rstcon_mask &= ~PHY0_SWRST_MASK;
		write<Rstcon>(rstcon_mask);

		/* reset both PHY and Link of Host */
		rstcon_mask = read<Rstcon>()
			|EXYNOS4X12_HOST_LINK_PORT_SWRST_MASK
			|EXYNOS4X12_PHY1_SWRST_MASK;
		write<Rstcon>(rstcon_mask);
		timer.usleep(10);
		rstcon_mask &= ~(EXYNOS4X12_HOST_LINK_PORT_SWRST_MASK
			|EXYNOS4X12_PHY1_SWRST_MASK);
		write<Rstcon>(rstcon_mask);
		timer.usleep(10);
	}
	struct Phypwr : Register <0x0,32>{};
	struct Phyclk : Register <0x4,32>{};
	struct Rstcon : Register <0x8,32>{};
};

static void clock_pwr_init()
{
	/* enable USB2 clock and power up */
	static Regulator::Connection reg_clk(Regulator::CLK_USB20);
	reg_clk.state(true);

	static Regulator::Connection reg_pwr(Regulator::PWR_USB20);
	reg_pwr.state(true);
}

static void usb_phy_init(Genode::Env &env)
{
	Io_mem_connection io_usbotg(env, USBOTG, 0x1000);
	addr_t usbotg_base = (addr_t)env.rm().attach(io_usbotg.dataspace());
	Usb_Otg usbotg(usbotg_base);
	env.rm().detach(usbotg_base);
}

static void odroidx2_ehci_init(Genode::Env &env)
{
	clock_pwr_init();
	usb_phy_init(env);

	/* reset hub via GPIO */
	enum { X30 = 294, X34 = 298, X35 = 299 };

	Gpio::Connection gpio_x30(X30);
	Gpio::Connection gpio_x34(X34);
	Gpio::Connection gpio_x35(X35);

	/* Set Ref freq 0 => 24MHz, 1 => 26MHz*/
	/* Odroid Us have it at 24MHz, Odroid Xs at 26MHz */
	gpio_x30.write(true);

	/* Disconnect, Reset, Connect */
	gpio_x34.write(false);
	gpio_x35.write(false);
	gpio_x35.write(true);
	gpio_x34.write(true);

	/* reset ehci controller */
	Io_mem_connection io_ehci(env, EHCI_BASE, 0x1000);
	addr_t ehci_base = (addr_t)env.rm().attach(io_ehci.dataspace());

	Ehci ehci(ehci_base);

	env.rm().detach(ehci_base);
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
	odroidx2_ehci_init(services->env);

	/* setup EHCI-controller platform device */
	platform_device *pdev   = (platform_device *)kzalloc(sizeof(platform_device), 0);
	pdev->name              = (char *)"exynos-ehci";
	pdev->id                = 0;
	pdev->num_resources     = 2;
	pdev->resource          = _ehci;

	/*needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c' */
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
