/**
 * \brief  EHCI for OMAP4
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-06-20
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform/platform.h>

#include <io_mem_session/connection.h>
#include <util/mmio.h>

#include <lx_emul.h>

using namespace Genode;

/**
 * Base addresses
 */
enum {
	EHCI_BASE = 0x4a064c00,
	UHH_BASE  = 0x4a064000,
	TLL_BASE  = 0x4a062000,
	SCRM_BASE = 0x4a30a000,
	CAM_BASE  = 0x4a009000, /* used for L3INIT_CM2 */
};


/**
 * Inerrupt numbers
 */
enum {
 IRQ_GIC_START = 32,
 IRQ_EHCI      = IRQ_GIC_START + 77,
};


/**
 * Resources for platform device
 */
static resource _ehci[] =
{
	{ EHCI_BASE, EHCI_BASE + 0x400 - 1, "ehci", IORESOURCE_MEM },
	{ IRQ_EHCI, IRQ_EHCI, "ehci-irq", IORESOURCE_IRQ },
};


/**
 * Port informations for platform device
 */
static struct ehci_hcd_omap_platform_data _ehci_data
{
	{ OMAP_EHCI_PORT_MODE_PHY, OMAP_EHCI_PORT_MODE_NONE },
	{ 0, 0 }
};


/**
 * Enables USB clocks
 */
struct Clocks : Genode::Mmio
{
	Clocks(Genode::addr_t const mmio_base) : Mmio(mmio_base)
	{
		write<Usb_phy_clk>(0x101);
		write<Usb_host_clk>(0x1008002);
		write<Usb_tll_clk>(0x1);
	}

	struct Usb_host_clk : Register<0x358, 32> { };
	struct Usb_tll_clk : Register<0x368, 32> { };
	struct Usb_phy_clk : Register<0x3e0, 32> { };

	template <typename T> void update(unsigned val)
	{
		typename T::access_t access = read<T>();
		access |= val;
		write<T>(access);
	};

	void dump()
	{
		Usb_host_clk::access_t a1 = read<Usb_host_clk>();
		Usb_tll_clk::access_t a3 = read<Usb_tll_clk>();
		Usb_phy_clk::access_t a4 = read<Usb_phy_clk>();
	}
};


/**
 * Panda board reference USB clock
 */
struct Aux3 : Genode::Mmio
{
	Aux3(addr_t const mmio_base) : Mmio(mmio_base)
	{
		enable();
	}

	/* the clock register */
	struct Aux3_clk : Register<0x31c, 32>
	{
		struct Src_select : Bitfield<1, 2> { };
		struct Div : Bitfield<16, 4> { enum { DIV_2 = 1 }; };
		struct Enable : Bitfield<8, 1> { enum { ON = 1 }; };
	};

	/* clock source register */
	struct Aux_src : Register<0x110, 32, true> { };

	void enable()
	{
		/* select system clock */
		write<Aux3_clk::Src_select>(0);

		/* set to 19.2 Mhz */
		write<Aux3_clk::Div>(Aux3_clk::Div::DIV_2);

		/* enable clock */
		write<Aux3_clk::Enable>(Aux3_clk::Enable::ON);

	/* enable_ext = 1 | enable_int = 1| mode = 0x01 */
		write<Aux_src>(0xd);
	}
};


/**
 * ULPI transceiverless link
 */
struct Tll : Genode::Mmio
{
	Tll(addr_t const mmio_base) : Mmio(mmio_base)
	{
		reset();
	}

	struct Sys_config : Register<0x10, 32>
	{
		struct Soft_reset : Bitfield<1, 1> { };
		struct Cactivity : Bitfield<8, 1> { };
		struct Sidle_mode : Bitfield<3, 2> { };
		struct Ena_wakeup : Bitfield<2, 1> { };
	};

	struct Sys_status : Register<0x14, 32> { };

	void reset()
	{
		write<Sys_config>(0x0);

		/* reset */
		write<Sys_config::Soft_reset>(0x1);

		while(!read<Sys_status>())
			msleep(1);

		/* disable IDLE, enable wake up, enable auto gating  */
		write<Sys_config::Cactivity>(1);
		write<Sys_config::Sidle_mode>(1);
		write<Sys_config::Ena_wakeup>(1);
	}
};


/**
 * USB high-speed host
 */
struct Uhh : Genode::Mmio
{
	Uhh(addr_t const mmio_base) : Mmio(mmio_base)
	{
		/* diable idle and standby */
		write<Sys_config::Idle>(1);
		write<Sys_config::Standby>(1);

		/* set ports to external phy */
		write<Host_config::P1_mode>(0);
		write<Host_config::P2_mode>(0);
	}

	struct Sys_config : Register<0x10, 32>
	{
		struct Idle : Bitfield<2, 2> { };
		struct Standby : Bitfield<4, 2> { };
	};

	struct Host_config : Register<0x40, 32>
	{
		struct P1_mode : Bitfield<16, 2> { };
		struct P2_mode : Bitfield<18, 2> { };
	};
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
 * Panda board GPIO bases 1 - 6
 */
static addr_t omap44xx_gpio_base[] =
{
		0x4A310000, 0x48055000, 0x48057000, 0x48059000, 0x4805B000, 0x4805D000
};


/**
 * General purpose I/O
 */
struct Gpio
{
		enum { GPIO = 6 };

		addr_t _io[GPIO];
		Io_mem_session_capability _cap[GPIO];

		void map()
		{
			for (int i = 0; i < GPIO; i++) {
				Io_mem_connection io(omap44xx_gpio_base[i], 0x1000);
				io.on_destruction(Io_mem_connection::KEEP_OPEN);
				_io[i]  = (addr_t)env()->rm_session()->attach(io.dataspace());
				_cap[i] = io.cap();
			}
		}

		Gpio()
		{
			map();
		}

		~Gpio()
		{
			for (int i = 0; i < GPIO; i++) {
				env()->rm_session()->detach(_io[i]);
				env()->parent()->close(_cap[i]);
			}
		}

		addr_t base(unsigned gpio) { return _io[gpio >> 5]; }
		int index(unsigned gpio)   { return gpio & 0x1f; }

		void _set_data_out(addr_t base, unsigned gpio, bool enable)
		{
			enum { SETDATAOUT = 0x194, CLEARDATAOUT = 0x190 };
			writel(1U << gpio, base + (enable ? SETDATAOUT : CLEARDATAOUT));
		}

		void _set_gpio_direction(addr_t base, unsigned gpio, bool input)
		{
			enum { OE = 0x134 };
			base += OE;

			u32 val = readl(base);

			if (input)
				val |= 1U << gpio;
			else
				val &= ~(1U << gpio);

			writel(val, base);
		}

		void direction_output(unsigned gpio, bool enable)
		{
			_set_data_out(base(gpio), index(gpio), enable);
			_set_gpio_direction(base(gpio), index(gpio), false);
		}

		void direction_input(unsigned gpio)
		{
			_set_gpio_direction(base(gpio), index(gpio), true);
		}

		void set_value(unsigned gpio, int val)
		{
			_set_data_out(base(gpio), index(gpio), val);
		}

		unsigned get_value(int gpio)
		{
			enum  { DATAIN = 0x138 };
			return (readl(base(gpio) + DATAIN) & (1 << index(gpio))) != 0;
		}
};


/**
 * Initialize the USB controller from scratch, since the boot loader might not
 * do it or even disable USB.
 */
static void omap_ehci_init()
{
	/* taken from the Panda board manual */
	enum { HUB_POWER = 1, HUB_NRESET = 62, ULPI_PHY_TYPE = 182 };

	/* SCRM */
	Io_mem_connection io_scrm(SCRM_BASE, 0x1000);
	addr_t scrm_base = (addr_t)env()->rm_session()->attach(io_scrm.dataspace());

	/* enable reference clock */
	Aux3 aux3(scrm_base);

	/* init GPIO */
	Gpio gpio;
	/* disable the hub power and reset before init */
	gpio.direction_output(HUB_POWER, false);
	gpio.direction_output(HUB_NRESET, false);
	gpio.set_value(HUB_POWER, 0);
	gpio.set_value(HUB_NRESET, 1);

	/* enable clocks */
	Io_mem_connection io_clock(CAM_BASE, 0x1000);
	addr_t clock_base = (addr_t)env()->rm_session()->attach(io_clock.dataspace());
	Clocks c(clock_base);

	/* reset TLL */
	Io_mem_connection io_tll(TLL_BASE, 0x1000);
	addr_t tll_base = (addr_t)env()->rm_session()->attach(io_tll.dataspace());
	Tll t(tll_base);

	/* reset host */
	Io_mem_connection io_uhh(UHH_BASE, 0x1000);
	addr_t uhh_base = (addr_t)env()->rm_session()->attach(io_uhh.dataspace());
	Uhh uhh(uhh_base);

	/* enable hub power */
	gpio.set_value(HUB_POWER, 1);

	/* reset EHCI */
	addr_t ehci_base = uhh_base + 0xc00;
	Ehci ehci(ehci_base);

	addr_t base[] = { scrm_base, clock_base, tll_base, uhh_base, 0 };
	for (int i = 0; base[i]; i++)
		env()->rm_session()->detach(base[i]);
}


extern "C" void module_ehci_hcd_init();
extern "C" int  module_usbnet_init();
extern "C" int  module_smsc95xx_init();

void platform_hcd_init(Services *services)
{
	/* register netowrk */
	if (services->nic) {
		module_usbnet_init();
		module_smsc95xx_init();
	}

	/* register EHCI controller */
	module_ehci_hcd_init();

	/* initialize EHCI */
	omap_ehci_init();

	/* setup EHCI-controller platform device */
	platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
	pdev->name = "ehci-omap";
	pdev->id   = 0;
	pdev->num_resources = 2;
	pdev->resource = _ehci;
	pdev->dev.platform_data = &_ehci_data;

	/*
	 * Needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c'
	 */
	static u64 dma_mask = ~(u64)0;
	pdev->dev.dma_mask = &dma_mask;
	pdev->dev.coherent_dma_mask = ~0;

	platform_device_register(pdev);
}

