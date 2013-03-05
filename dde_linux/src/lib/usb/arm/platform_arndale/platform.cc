/*
 * \brief  EHCI for Arndale initializaion code
 * \author Sebastian Sumpf
 * \date   2013-02-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <io_mem_session/connection.h>
#include <util/mmio.h>

/* Emulation */
#include <platform/platform.h>
#include <lx_emul.h>

/* Linux */
#include <plat/ehci.h>

using namespace Genode;

enum {
	EHCI_BASE = 0x12110000,
	GPIO_BASE = 0x11400000,
	EHCI_IRQ  = 103,
};

static resource _ehci[] =
{
	{ EHCI_BASE, EHCI_BASE + 0xfff, "ehci", IORESOURCE_MEM },
	{ EHCI_IRQ, EHCI_IRQ, "ehci-irq", IORESOURCE_IRQ },
};

static struct s5p_ehci_platdata _ehci_data;


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
struct Gpio_bank {
	unsigned con;
	unsigned dat;
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

	gpio_cfg_pin(bank, gpio, GPIO_OUTPUT);

	value = readl(&bank->dat);
	value &= ~(0x1 << gpio);
	if (en)
		value |= 0x1 << gpio;
	writel(value, &bank->dat);
}


static void arndale_ehci_init()
{
	enum Gpio_offset { D1 = 0x180, X3 = 0xc60 };

	/* reset hub via GPIO */
	Io_mem_connection io_gpio(GPIO_BASE, 0x1000);
	addr_t gpio_base = (addr_t)env()->rm_session()->attach(io_gpio.dataspace());

	Gpio_bank *d1 = reinterpret_cast<Gpio_bank *>(gpio_base + D1);
	Gpio_bank *x3 = reinterpret_cast<Gpio_bank *>(gpio_base + X3);

	/* hub reset */
	gpio_direction_output(x3, 5, 0);
	/* hub connect */
	gpio_direction_output(d1, 7, 0);

	gpio_direction_output(x3, 5, 1);
	gpio_direction_output(d1, 7, 1);

	env()->rm_session()->detach(gpio_base);

	/* reset ehci controller */
	Io_mem_connection io_ehci(EHCI_BASE, 0x1000);
	addr_t ehci_base = (addr_t)env()->rm_session()->attach(io_ehci.dataspace());

	Ehci ehci(ehci_base);
	env()->rm_session()->detach(ehci_base);
}


extern "C" void module_ehci_hcd_init();
extern "C" int  module_usbnet_init();
extern "C" int  module_asix_init();

void platform_hcd_init(Services *services)
{
	/* register network */
	if (services->nic) {
		module_usbnet_init();
		module_asix_init();
	}

	/* register EHCI controller */
	module_ehci_hcd_init();

	/* setup controller */
	arndale_ehci_init();

	/* setup EHCI-controller platform device */
	platform_device *pdev   = (platform_device *)kzalloc(sizeof(platform_device), 0);
	pdev->name              = "s5p-ehci";
	pdev->id                = 0;
	pdev->num_resources     = 2;
	pdev->resource          = _ehci;
	pdev->dev.platform_data = &_ehci_data;

	/*needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c' */
	static u64 dma_mask         = ~(u64)0;
	pdev->dev.dma_mask          = &dma_mask;
	pdev->dev.coherent_dma_mask = ~0;

	platform_device_register(pdev);
}
