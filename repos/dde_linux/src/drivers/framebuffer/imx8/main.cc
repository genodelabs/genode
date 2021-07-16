/*
 * \brief  i.MX8 framebuffer driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Christian Prochaska
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015-2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_io_mem_dataspace.h>
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <driver.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <legacy/lx_kit/env.h>
#include <legacy/lx_kit/malloc.h>
#include <legacy/lx_kit/scheduler.h>
#include <legacy/lx_kit/timer.h>
#include <legacy/lx_kit/irq.h>
#include <legacy/lx_kit/work.h>

/* Linux module functions */
extern "C" void radix_tree_init();        /* called by start_kernel(void) normally */
extern "C" void drm_connector_ida_init(); /* called by drm_core_init(void) normally */
extern "C" int module_irq_imx_irqsteer_init();
extern "C" int module_imx_drm_pdrv_init();
extern "C" int module_dcss_driver_init();
extern "C" int module_dcss_crtc_driver_init();
extern "C" int module_imx_hdp_imx_platform_driver_init();
extern "C" int module_mixel_mipi_phy_driver_init();
extern "C" int module_imx_nwl_dsi_driver_bridge_init();
extern "C" int module_imx_nwl_dsi_driver_init();
extern "C" int module_rad_panel_driver_init();
extern "C" void postcore_mipi_dsi_bus_init();

unsigned long jiffies;

namespace Framebuffer { struct Main; }


struct Framebuffer::Main
{
	void _run_linux();

	/**
	 * Entry for executing code in the Linux kernel context
	 */
	static void _run_linux_entry(void *m)
	{
		reinterpret_cast<Main*>(m)->_run_linux();
	}

	Env                   &_env;
	Entrypoint            &_ep     { _env.ep() };
	Attached_rom_dataspace _config { _env, "config" };
	Heap                   _heap   { _env.ram(), _env.rm() };
	Driver                 _driver { _env, _config };

	/* Linux task that handles the initialization */
	Constructible<Lx::Task> _linux;

	Signal_handler<Main> _policy_change_handler {
		_env.ep(), *this, &Main::_handle_policy_change };

	bool _policy_change_pending = false;

	void _handle_policy_change()
	{
		_policy_change_pending = true;
		_linux->unblock();
		Lx::scheduler().schedule();
	}

	bool _hdmi()
	{
		try {
			Xml_node config = _config.xml();
			Xml_node xn = config.sub_node();
			for (unsigned i = 0; i < config.num_sub_nodes(); xn = xn.next()) {
				if (!xn.has_type("connector"))
					continue;

				bool enabled = xn.attribute_value("enabled", true);
				if (!enabled)
					continue;

				/* check first connector only */
				typedef String<64> Name;
				Name const con_policy = xn.attribute_value("name", Name());
				if (con_policy == "DSI-1")
					return false;

				return true;
			}
		} catch (...) { }

		return true;
	}

	Main(Env &env) : _env(env)
	{
		log("--- i.MX8 framebuffer driver ---");

		Lx_kit::construct_env(_env);

		LX_MUTEX_INIT(bridge_lock);
		LX_MUTEX_INIT(core_lock);
		LX_MUTEX_INIT(component_mutex);
		LX_MUTEX_INIT(host_lock);

		/* init singleton Lx::Scheduler */
		Lx::scheduler(&_env);

		Lx::malloc_init(_env, _heap);

		/* init singleton Lx::Timer */
		Lx::timer(&_env, &_ep, &_heap, &jiffies);

		/* init singleton Lx::Irq */
		Lx::Irq::irq(&_ep, &_heap);

		/* init singleton Lx::Work */
		Lx::Work::work_queue(&_heap);

		_linux.construct(_run_linux_entry, reinterpret_cast<void*>(this),
		                 "linux", Lx::Task::PRIORITY_0, Lx::scheduler());

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}
};


void Framebuffer::Main::_run_linux()
{
	system_wq  = alloc_workqueue("system_wq", 0, 0);

	radix_tree_init();
	drm_connector_ida_init();

	module_irq_imx_irqsteer_init();
	module_dcss_driver_init();
	module_imx_drm_pdrv_init();
	module_dcss_crtc_driver_init();
	module_imx_hdp_imx_platform_driver_init();

	/* MIPI DSI */
	module_mixel_mipi_phy_driver_init();
	module_imx_nwl_dsi_driver_bridge_init();
	module_imx_nwl_dsi_driver_init();
	postcore_mipi_dsi_bus_init();
	module_rad_panel_driver_init();

	/**
	 * This device is originally created with the name '32e2d000.irqsteer'
	 * via 'of_platform_bus_create()'. Here it is called 'imx-irqsteer' to match
	 * the driver name.
	 */

	struct platform_device *imx_irqsteer_pdev =
		platform_device_alloc("imx-irqsteer", 0);
	
	static resource imx_irqsteer_resources[] = 	{
		{ IOMEM_BASE_IRQSTEER, IOMEM_END_IRQSTEER,
		  "imx-irqsteer", IORESOURCE_MEM },
		{ IRQ_IRQSTEER, IRQ_IRQSTEER, "imx-irqsteer", IORESOURCE_IRQ },
	};

	imx_irqsteer_pdev->num_resources = 2;
	imx_irqsteer_pdev->resource = imx_irqsteer_resources;

	imx_irqsteer_pdev->dev.of_node             = (device_node*)kzalloc(sizeof(device_node), 0);
	imx_irqsteer_pdev->dev.of_node->name       = "imx-irqsteer";
	imx_irqsteer_pdev->dev.of_node->full_name  = "imx-irqsteer";

	platform_device_register(imx_irqsteer_pdev);


	/**
	 * This device is originally created with the name '32c00000.hdmi'
	 * via 'of_platform_bus_create()'. Here it is called 'i.mx8-hdp' to match
	 * the driver name.
	 */

	struct platform_device *hdp_pdev =
		platform_device_alloc("i.mx8-hdp", 0);

	static resource hdp_resources[] = 	{
		{ IOMEM_BASE_HDMI_CTRL, IOMEM_END_HDMI_CTRL,
		  "hdp_ctrl",  IORESOURCE_MEM },
		{ IOMEM_BASE_HDMI_CRS, IOMEM_END_HDMI_CRS,
		  "hdp_crs",   IORESOURCE_MEM },
		{ IOMEM_BASE_HDMI_RST, IOMEM_END_HDMI_RST,
		  "hdp_reset", IORESOURCE_MEM },
		{       33,       33, "plug_in",   IORESOURCE_IRQ },
		{       34,       34, "plug_out",  IORESOURCE_IRQ },
	};

	hdp_pdev->num_resources = 5;
	hdp_pdev->resource = hdp_resources;

	hdp_pdev->dev.of_node                    = (device_node*)kzalloc(sizeof(device_node), 0);
	hdp_pdev->dev.of_node->name              = "hdmi";
	hdp_pdev->dev.of_node->full_name         = "hdmi";
	hdp_pdev->dev.of_node->properties        = (property*)kzalloc(sizeof(property), 0);
	hdp_pdev->dev.of_node->properties->name  = "compatible";
	hdp_pdev->dev.of_node->properties->value = (void*)"fsl,imx8mq-hdmi";

	bool hdmi = _hdmi();
	if (hdmi)
		platform_device_register(hdp_pdev);

	struct platform_device *mipi_dsi_phy_pdev =
		platform_device_alloc("mixel-mipi-dsi-phy", 0);

	static resource mipi_dsi_phy_resources[] = {
		{ IOMEM_BASE_MIPI_DPHY, IOMEM_BASE_MIPI_DPHY+0xff, "dsi_phy", IORESOURCE_MEM }
	};

	mipi_dsi_phy_pdev->num_resources = 1;
	mipi_dsi_phy_pdev->resource      = mipi_dsi_phy_resources;

	mipi_dsi_phy_pdev->dev.of_node                      = (device_node*)kzalloc(sizeof(device_node), 0);
	mipi_dsi_phy_pdev->dev.of_node->properties          = (property*)kzalloc(2*sizeof(property), 0);
	mipi_dsi_phy_pdev->dev.of_node->properties[0].name  = "compatible";
	mipi_dsi_phy_pdev->dev.of_node->properties[0].value = (void*)"mixel,imx8mq-mipi-dsi-phy";
	mipi_dsi_phy_pdev->dev.of_node->properties[0].next  = &mipi_dsi_phy_pdev->dev.of_node->properties[1];
	mipi_dsi_phy_pdev->dev.of_node->properties[1].name  = "dsi_phy";
	mipi_dsi_phy_pdev->dev.of_node->properties[1].value = (void*)0;

	mipi_dsi_phy_pdev->dev.parent = &mipi_dsi_phy_pdev->dev;

	if (hdmi == false) {
		platform_device_register(mipi_dsi_phy_pdev);
	}
	/**
	 * This device is originally created with the name '32e00000.dcss'
	 * via 'of_platform_bus_create()'. Here it is called 'dcss-core' to match
	 * the driver name.
	 */
	struct platform_device *dcss_pdev =
		platform_device_alloc("dcss-core", 0);

	static resource dcss_resources[] = 	{
		{ IOMEM_BASE_DCSS, IOMEM_END_DCSS, "dcss", IORESOURCE_MEM },
		{        3,        3, "dpr_dc_ch0", IORESOURCE_IRQ },
		{        4,        4, "dpr_dc_ch1", IORESOURCE_IRQ },
		{        5,        5, "dpr_dc_ch2", IORESOURCE_IRQ },
		{        6,        6, "ctx_ld",     IORESOURCE_IRQ },
		{        8,        8, "ctxld_kick", IORESOURCE_IRQ },
		{        9,        9, "dtg_prg1",   IORESOURCE_IRQ },
		{       16,       16, "dtrc_ch1",   IORESOURCE_IRQ },
		{       17,       17, "dtrc_ch2",   IORESOURCE_IRQ },
	};

	dcss_pdev->num_resources = 9;
	dcss_pdev->resource = dcss_resources;

	dcss_pdev->dev.of_node                    = (device_node*)kzalloc(sizeof(device_node), 0);
	dcss_pdev->dev.of_node->name              = "dcss";
	dcss_pdev->dev.of_node->full_name         = "dcss";
	dcss_pdev->dev.of_node->properties        = (property*)kzalloc(sizeof(property), 0);
	dcss_pdev->dev.of_node->properties->name  = "disp-dev";
	dcss_pdev->dev.of_node->properties->value = hdmi ? (void*)"hdmi_disp" : (void *)"mipi_disp";

	platform_device_register(dcss_pdev);


	struct platform_device *mipi_dsi_bridge_pdev =
		platform_device_alloc("nwl-mipi-dsi", 0);

	static resource mipi_dsi_bridge_resources[] = {
		{ IOMEM_BASE_MIPI_DSI, IOMEM_END_MIPI_DSI, "mipi_dsi_bridge", IORESOURCE_MEM },
		{ IRQ_MIPI_DSI,        IRQ_MIPI_DSI,       "mipi_dsi",        IORESOURCE_IRQ }
	};

	mipi_dsi_bridge_pdev->num_resources = 2;
	mipi_dsi_bridge_pdev->resource      = mipi_dsi_bridge_resources;

	Genode::addr_t **phy_ptr =
	  (Genode::addr_t **)devres_find(&mipi_dsi_phy_pdev->dev, devm_phy_consume, nullptr, nullptr);
	mipi_dsi_bridge_pdev->dev.of_node                      = (device_node*)kzalloc(sizeof(device_node), 0);
	mipi_dsi_bridge_pdev->dev.of_node->name                = "mipi_dsi_bridge";
	mipi_dsi_bridge_pdev->dev.of_node->properties          = (property*)kzalloc(sizeof(property), 0);
	mipi_dsi_bridge_pdev->dev.of_node->properties[0].name  = "dphy";
	mipi_dsi_bridge_pdev->dev.of_node->properties[0].value = phy_ptr ? (void*)*phy_ptr : nullptr;
	mipi_dsi_bridge_pdev->dev.of_node->properties[0].next  = nullptr;

	if (hdmi == false)
		platform_device_register(mipi_dsi_bridge_pdev);

	/*
	 * This device is originally created with the name 'display-subsystem'
	 * via 'of_platform_bus_create()'. Here it is called 'imx-drm' to match
	 * the driver name.
	 */

	struct platform_device *display_subsystem_pdev =
		platform_device_alloc("imx-drm", 0);

	static device_node display_subsystem_of_node = { "display-subsystem" };

	display_subsystem_pdev->dev.of_node = &display_subsystem_of_node;

	platform_device_register(display_subsystem_pdev);

	struct platform_device *mipi_dsi_imx_pdev =
		platform_device_alloc("nwl_dsi-imx", 0);

	mipi_dsi_imx_pdev->dev.of_node                      = (device_node*)kzalloc(sizeof(device_node), 0);
	mipi_dsi_imx_pdev->dev.of_node->name                = "mipi_dsi";
	mipi_dsi_imx_pdev->dev.of_node->properties          = (property*)kzalloc(2*sizeof(property), 0);
	mipi_dsi_imx_pdev->dev.of_node->properties[0].name  = "compatible";
	mipi_dsi_imx_pdev->dev.of_node->properties[0].value = (void *)"fsl,imx8mq-mipi-dsi_drm";
	mipi_dsi_imx_pdev->dev.of_node->properties[0].next  = &mipi_dsi_imx_pdev->dev.of_node->properties[1];
	mipi_dsi_imx_pdev->dev.of_node->properties[1].name  = "dphy";
	mipi_dsi_imx_pdev->dev.of_node->properties[1].value = phy_ptr ? (void *)*phy_ptr : nullptr;

	if (hdmi == false)
		platform_device_register(mipi_dsi_imx_pdev);

	_driver.finish_initialization();
	_driver.config_sigh(_policy_change_handler);

	_config.sigh(_policy_change_handler);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
		while (_policy_change_pending) {
			_policy_change_pending = false;
			_driver.config_changed();
		}
	}
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Framebuffer::Main main(env);
}
