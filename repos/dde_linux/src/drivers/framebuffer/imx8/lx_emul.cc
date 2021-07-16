/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Christian Prochaska
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <platform_session/device.h>

/* local includes */
#include <driver.h>

#include <lx_emul.h>
#include <lx_emul_c.h>

/* DRM-specific includes */
#include <legacy/lx_emul/extern_c_begin.h>
#include <drm/drmP.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include "drm_crtc_internal.h"
#include "drm_internal.h"
#include <linux/component.h>
#include <linux/phy/phy.h>
#include <video/videomode.h>
#include <legacy/lx_emul/extern_c_end.h>

#include <legacy/lx_kit/scheduler.h> /* dependency of lx_emul/impl/completion.h */

#include <legacy/lx_emul/impl/completion.h>
#include <legacy/lx_emul/impl/delay.h>
#include <legacy/lx_emul/impl/gfp.h>
#include <legacy/lx_emul/impl/kernel.h>
#include <legacy/lx_emul/impl/mutex.h>
#include <legacy/lx_emul/impl/sched.h>
#include <legacy/lx_emul/impl/slab.h>
#include <legacy/lx_emul/impl/spinlock.h>
#include <legacy/lx_emul/impl/timer.h>
#include <legacy/lx_emul/impl/wait.h> /* dependency of lx_emul/impl/work.h */
#include <legacy/lx_emul/impl/work.h>

#include <legacy/lx_kit/irq.h>
#include <legacy/lx_kit/malloc.h>

enum Device_id { DCSS, HDMI, MIPI, SRC, UNKNOWN };


namespace Platform { struct Device_client; }


struct Platform::Device_client : Rpc_client<Device_interface>
{
	Device_client(Capability<Device_interface> cap)
	: Rpc_client<Device_interface>(cap) { }

	Irq_session_capability irq(unsigned id = 0)
	{
		return call<Rpc_irq>(id);
	}

	Io_mem_session_capability io_mem(unsigned id, Range &range, Cache cache)
	{
		return call<Rpc_io_mem>(id, range, cache);
	}

	Dataspace_capability io_mem_dataspace(unsigned id = 0)
	{
		Range range { };
		return Io_mem_session_client(io_mem(id, range, UNCACHED)).dataspace();
	}
};


namespace Lx_kit {
	Platform::Connection    & platform_connection();
	Platform::Device_client & platform_device(Device_id);
}


Platform::Connection & Lx_kit::platform_connection()
{
	static Platform::Connection plat { Lx_kit::env().env() };
	return plat;
}


Platform::Device_client & Lx_kit::platform_device(Device_id id)
{
	if (id == DCSS) {
		static Platform::Device_client dcss {
			platform_connection().device_by_type("nxp,imx8mq-dcss") };
		return dcss;
	}

	if (id == HDMI) {
		static Platform::Device_client hdmi {
			platform_connection().device_by_type("fsl,imx8mq-hdmi") };
		return hdmi;
	}

	if (id == MIPI) {
		static Platform::Device_client mipi {
			platform_connection().device_by_type("fsl,imx8mq-mipi-dsi_drm") };
		static bool update = true;
		if (update) {
			platform_connection().update();
			update = false;
		}
		return mipi;
	}

	if (id == SRC){
		static Platform::Device_client src {
			platform_connection().acquire_device("src") };
		return src;
	}

	throw 1;
}


/********************************
 ** drivers/base/dma-mapping.c **
 ********************************/

void *dmam_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp)
{
	dma_addr_t dma_addr;
	void *addr;
	if (size > 2048) {
		addr = Lx::Malloc::dma().alloc_large(size);
		dma_addr = (dma_addr_t) Lx::Malloc::dma().phys_addr(addr);
	} else
		addr = Lx::Malloc::dma().alloc(size, 12, &dma_addr);

	*dma_handle = dma_addr;
	return addr;
}


/*****************************
 ** drivers/base/platform.c **
 *****************************/

int platform_get_irq(struct platform_device *dev, unsigned int num)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_IRQ, 0);
	return r ? r->start : -1;
}

int platform_get_irq_byname(struct platform_device *dev, const char *name)
{
	struct resource *r = platform_get_resource_byname(dev, IORESOURCE_IRQ, name);
	return r ? r->start : -1;
}

struct resource *platform_get_resource(struct platform_device *dev,
                                       unsigned int type, unsigned int num)
{
	unsigned i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

		if ((type & r->flags) && num-- == 0)
			return r;
	}

	return NULL;
}

struct resource *platform_get_resource_byname(struct platform_device *dev,
                                              unsigned int type,
                                              const char *name)
{
	unsigned i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

	if (type == r->flags && !Genode::strcmp(r->name, name))
		return r;
	}

	if (DEBUG_DRIVER)
		Genode::error("RESOURCE: ", name, " not found");

	return NULL;
}


static int platform_match(struct device *dev, struct device_driver *drv)
{
	if (!dev->name)
		return 0;

	return (Genode::strcmp(dev->name, drv->name) == 0);
}


#define to_platform_driver(drv) (container_of((drv), struct platform_driver, \
                                 driver))

static int platform_drv_probe(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->probe(dev);
}


struct bus_type platform_bus_type = {
	.name  = "platform",
};


int platform_driver_register(struct platform_driver * drv)
{
	/* init platform_bus_type */
	platform_bus_type.match = platform_match;
	platform_bus_type.probe = platform_drv_probe;

	drv->driver.bus = &platform_bus_type;
	if (drv->probe)
		drv->driver.probe = platform_drv_probe;

	printk("Register: %s\n", drv->driver.name);
	return driver_register(&drv->driver);
}


int platform_device_add(struct platform_device *pdev)
{
	return platform_device_register(pdev);
}


int platform_device_add_data(struct platform_device *pdev, const void *data,
                             size_t size)
{
	void *d = NULL;

	if (data && !(d = kmemdup(data, size, GFP_KERNEL)))
		return -ENOMEM;

	kfree(pdev->dev.platform_data);
	pdev->dev.platform_data = d;

	return 0;
}


int platform_device_register(struct platform_device *pdev)
{
	if (pdev->dev.bus == nullptr)
		pdev->dev.bus  = &platform_bus_type;

	pdev->dev.name = pdev->name;
	/*Set parent to ourselfs */
	if (!pdev->dev.parent)
		pdev->dev.parent = &pdev->dev;
	device_add(&pdev->dev);

	return 0;
}


struct platform_device *platform_device_alloc(const char *name, int id)
{
	platform_device *pdev = (platform_device *)kzalloc(sizeof(struct platform_device), GFP_KERNEL);

	if (!pdev)
		return 0;

	int len    = strlen(name);
	pdev->name = (char *)kzalloc(len + 1, GFP_KERNEL);

	if (!pdev->name) {
		kfree(pdev);
		return 0;
	}

	memcpy(pdev->name, name, len);
	pdev->name[len] = 0;
	pdev->id = id;
	pdev->dev.dma_mask = (u64*)kzalloc(sizeof(u64),  GFP_KERNEL);

	spin_lock_init(&pdev->dev.devres_lock);
	INIT_LIST_HEAD(&pdev->dev.devres_head);

	return pdev;
}


/***********************
 ** drivers/clk/clk.c **
 ***********************/

unsigned long clk_get_rate(struct clk * clk)
{
	if (!clk) return 0;
	return clk->rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	if (DEBUG_DRIVER)
		Genode::warning(__func__, "() not implemented");


	if (!clk) return -1;
	clk->rate = rate;
	return 0;
}


int clk_prepare_enable(struct clk *clk)
{
	TRACE;
	return 0;
}


/******************************
 ** drivers/clk/clk-devres.c **
 ******************************/

struct clk *devm_clk_get(struct device *dev, const char *id)
{
	/* numbers from running Linux system */

	using namespace Genode;

	const char * clock_name = id;
	if (String<32>("ipg") == id) { clock_name = "apb"; }
	if (String<32>("tx_esc") == id) { clock_name = "rx_esc"; }

	unsigned long rate = 0;
	Lx_kit::platform_connection().with_xml([&] (Xml_node node) {
		node.for_each_sub_node("device", [&] (Xml_node node) {
			node.for_each_sub_node("clock", [&] (Xml_node node) {
				if (node.attribute_value("name", String<64>()) != clock_name) {
					return; }
				rate = node.attribute_value<unsigned long>("rate", 0);
			});
		});
	});

	if (!rate) {
		if (DEBUG_DRIVER)
			Genode::error(__func__, " clock not found ", id);
		return nullptr;
	}

	struct clk * clock = (struct clk*) kzalloc(sizeof(struct clk), GFP_KERNEL);
	clock->name = id;
	clock->rate = rate;

	if (String<32>("tx_esc") == id)
		clock->rate /= 4;

	return clock;
}


/*******************************
 ** drivers/gpu/drm/drm_drv.c **
 *******************************/

void drm_dev_printk(const struct device *dev, const char *level,
                    unsigned int category, const char *function_name,
                    const char *prefix, const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (category && !(drm_debug & category))
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	if (dev)
		dev_printk(level, dev, "[drm:%s]%s %pV", function_name, prefix,
			   &vaf);
	else
		printk("%s" "[drm:%s]%s %pV", level, function_name, prefix, &vaf);

	va_end(args);
}

/*****************************************
 ** drivers/gpu/drm/drm_fb_cma_helper.c **
 *****************************************/

struct drm_gem_cma_object *drm_fb_cma_get_gem_obj(struct drm_framebuffer *fb,
						  unsigned int plane)
{
	struct drm_gem_object *gem;

	gem = drm_gem_fb_get_obj(fb, plane);
	if (!gem)
		return NULL;

	return to_drm_gem_cma_obj(gem);
}

int drm_fb_cma_prepare_fb(struct drm_plane *plane,
                          struct drm_plane_state *state)
{
	return drm_gem_fb_prepare_fb(plane, state);
}


/*********************************************
 ** drivers/gpu/drm/imx/hdp/imx-hdp-audio.c **
 *********************************************/

void imx_hdp_register_audio_driver(struct device *dev)
{
	/* not supported */
}


/***********************
 ** drivers/of/base.c **
 ***********************/

static struct device_node root_device_node {
	.name = "",
	.full_name = "",
};

static struct device_node hdmi_device_node {
	.name = "hdmi",
	.full_name = "hdmi",
	.parent = &root_device_node
};

static struct device_node hdmi_endpoint_device_node {
	.name = "hdmi-endpoint",
	.full_name = "hdmi-endpoint",
};

static struct device_node endpoint_device_node {
	.name = "endpoint",
	.full_name = "endpoint",
};

static struct device_node port_device_node {
	.name = "port",
	.full_name = "port",
};

static struct device_node mipi_endpoint_device_node {
	.name = "mipi-endpoint",
	.full_name = "mipi-endpoint",
	.parent = &root_device_node
};

static struct device_node mipi_device_node {
	.name = "mipi_dsi",
	.full_name = "mipi_dsi",
	.parent = &root_device_node
};

static struct device_node mipi_panel_node {
	.name      = "panel",
	.full_name = "panel"
};

int of_device_is_compatible(const struct device_node *device,
                            const char *compat)
{
	if (!device)
		return false;

	if (Genode::strcmp(compat, "nxp,imx8mq-dcss") == 0)
		return true;

	return false;
}

struct device_node *of_get_next_child(const struct device_node *node,
                                      struct device_node *prev)
{
	if (Genode::strcmp(node->name, "port", strlen(node->name)) == 0) {
		if (prev)
			return NULL;

		if (port_device_node.parent == &mipi_device_node) {
			return &port_device_node;
		}

		return &hdmi_endpoint_device_node;
	}

	if (Genode::strcmp(node->name, "mipi_dsi_bridge") == 0) {
		if (prev) return NULL;
		/* create panel device node */

		device_node *np = &mipi_panel_node;
		np->properties = (property *)kzalloc(6*sizeof(property), 0);
		np->properties[0].name  = "panel",
		np->properties[0].value = NULL,
		np->properties[0].next  = &np->properties[1];
		np->properties[1].name  = "reg";
		np->properties[1].value = 0;
		np->properties[1].next  = &np->properties[2];
		np->properties[2].name  = "compatible";
		np->properties[2].value = (void *)"raydium,rm67191";
		np->properties[2].next  = &np->properties[3];
		np->properties[3].name  = "dsi-lanes";
		np->properties[3].value = (void *)4;
		np->properties[3].next  = &np->properties[4];
		np->properties[4].name  = "panel-width-mm";
		np->properties[4].value = (void *)68;
		np->properties[4].next  = &np->properties[5];
		np->properties[5].name  = "panel-height-mm";
		np->properties[5].value = (void *)121;

		return np;
	}

	if (DEBUG_DRIVER)
		Genode::error("of_get_next_child(): unhandled node");

	return NULL;
}

struct device_node *of_get_child_by_name(const struct device_node *node,
                                         const char *name)
{
	if (Genode::strcmp(name, "display-timings") == 0)
		return (device_node *)1;

	return nullptr;
}

struct device_node *of_get_parent(const struct device_node *node)
{
	static device_node dcss_device_node { "dcss", "dcss" };

	if (!node)
		return NULL;

	if (Genode::strcmp(node->name, "port", strlen("port")) == 0)
		return &dcss_device_node;

	if (DEBUG_DRIVER)
		Genode::error("of_get_parent(): unhandled node");

	return NULL;
}

const void *of_get_property(const struct device_node *node, const char *name, int *lenp)
{
	*lenp = 0;
	for (property * p = node ? node->properties : nullptr; p; p = p->next) {
		if (!p) break;
		if (Genode::strcmp(name, p->name) == 0) {
			*lenp = sizeof(void *);
			return p->value;
		}
	}

	if (DEBUG_DRIVER)
		Genode::warning("OF property ", name, " not found");
	return nullptr;
}


int of_alias_get_id(struct device_node *np, const char *stem)
{
	int len = 0;
	return (long)of_get_property(np, stem, &len);
}


struct device_node *of_parse_phandle(const struct device_node *np, const char *phandle_name, int index)
{
	/* device node information from fsl-imx8mq.dtsi */

	static device_node dcss_device_node {
		.name = "dcss",
		.full_name = "dcss",
	};

	static device_node port_device_node {
		.name = "port",
		.full_name = "port",
		.parent = &dcss_device_node
	};

	if ((Genode::strcmp(phandle_name, "ports", strlen(phandle_name)) == 0) &&
	    (index == 0))
		return &port_device_node;

	if (DEBUG_DRIVER)
		Genode::warning("of_parse_phandle(): unhandled phandle or index");

	return NULL;
}


struct property *of_find_property(const struct device_node *np,
                                  const char *name, int *lenp)
{
	TRACE;
	return np->properties;
}


int of_modalias_node(struct device_node *node, char *modalias, int len)
{
	TRACE;
	return 0;
}


/**************************
 ** linux/of_videomode.h **
 **************************/

int of_get_videomode(struct device_node *np, struct videomode *vm, int index)
{
	/* taken from device tree */
	if (Genode::strcmp(np->name, "panel") == 0) {
		vm->pixelclock   = 0x7de2900;
		vm->hactive      = 0x438;
		vm->hfront_porch = 0x14;
		vm->hback_porch  = 0x22;
		vm->hsync_len    = 0x2;

		vm->vactive      = 0x780;
		vm->vfront_porch = 0x1e;
		vm->vback_porch  = 0x4;
		vm->vsync_len    = 0x2;

		vm->flags = (display_flags)0x1095;

		return 0;
	}

	return -1;
}



/*************************
 ** drivers/of/device.c **
 *************************/

extern struct dcss_devtype dcss_type_imx8m; 

const void *of_device_get_match_data(const struct device *dev)
{
	if (Genode::strcmp(dev->name, "dcss-core", strlen(dev->name)) == 0)
		return &dcss_type_imx8m;

	return NULL;
}

const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev)
{
	int len = 0;
	const char * compatible = (const char*) of_get_property(dev->of_node, "compatible", &len);

	for (; matches && matches->compatible[0]; matches++) {
		if (Genode::strcmp(matches->compatible, compatible) == 0)
			return matches;
	}
	return nullptr;
}


int of_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	if (of_match_device(drv->of_match_table, dev))
		return 1;

	return 0;
}


/***************************
 ** drivers/of/property.c **
 ***************************/

struct device_node *of_graph_get_next_endpoint(const struct device_node *parent,
                                               struct device_node *prev)
{
	if (Genode::strcmp(parent->name, "hdmi", strlen(parent->name)) == 0) {

		if (!prev)
			return &endpoint_device_node;

		return nullptr;
	}

	if(Genode::strcmp(parent->name, "mipi_dsi", strlen(parent->name)) == 0) {

		if (!prev)
			return &mipi_endpoint_device_node;

		return nullptr;
	}

	if (DEBUG_DRIVER)
		Genode::error(__func__, "(): unhandled parent '", parent->name, "'");

	return nullptr;
}

struct device_node *of_graph_get_port_by_id(struct device_node *parent, u32 id)
{
	if ((Genode::strcmp(parent->name, "dcss", strlen(parent->name)) == 0) &&
	    (id == 0))
		return &port_device_node;

	if (DEBUG_DRIVER)
		Genode::error("of_graph_get_port_by_id(): unhandled parent or id\n");

	return NULL;
}

struct device_node *of_graph_get_remote_port(const struct device_node *node)
{
	if (Genode::strcmp(node->name, "endpoint", strlen(node->name)) == 0)
		return &port_device_node;

	if (Genode::strcmp(node->name, "mipi-endpoint", strlen(node->name)) == 0) {
		return &port_device_node;
	}

	if (DEBUG_DRIVER)
		Genode::error("of_graph_get_remote_port(): unhandled node '", node->name, "'\n");

	return NULL;
}

struct device_node *of_graph_get_remote_port_parent(const struct device_node *node)
{
	if (Genode::strcmp(node->name, "hdmi-endpoint") == 0)
		return &hdmi_device_node;

	if (Genode::strcmp(node->name, "mipi-endpoint") == 0) {

		int len;
		void const *np = of_get_property(&mipi_endpoint_device_node, "mipi_dsi_bridge_np", &len);
		return (device_node *)np;
	}

	if (Genode::strcmp(node->name, "port") == 0)
		return &mipi_device_node;

	if (DEBUG_DRIVER)
		Genode::error("of_graph_get_remote_port_parent(): unhandled node: ", node->name);

	return NULL;
}


/********************************
 ** drivers/soc/imx/soc-imx8.c **
 ********************************/

bool check_hdcp_enabled(void)
{
	return false;
}

bool cpu_is_imx8mq(void)
{
	return true;
}

bool cpu_is_imx8qm(void)
{
	return false;
}

unsigned int imx8_get_soc_revision(void)
{
	/* XXX: acquire from firmware if this becomes necessary */
	return SOC_REVISION;
}


/***********************
 ** kernel/irq/chip.c **
 ***********************/

static struct irq_chip *irqsteer_chip = nullptr;

static struct irq_desc irqsteer_irq_desc;

static irqreturn_t irqsteer_irq_handler(int irq, void *data)
{
	irqsteer_irq_desc.handle_irq(&irqsteer_irq_desc);
	return IRQ_HANDLED;
}

void irq_set_chained_handler_and_data(unsigned int irq,
                                      irq_flow_handler_t handle,
                                      void *data)
{
	irqsteer_irq_desc.irq_common_data.handler_data = data;
	irqsteer_irq_desc.irq_data.chip = irqsteer_chip;
	irqsteer_irq_desc.handle_irq = handle;

	Lx::Irq::irq().request_irq(Lx_kit::platform_device(DCSS).irq(), irq,
	                           irqsteer_irq_handler, nullptr, nullptr);
}


/*************************
 ** kernel/irq/devres.c **
 *************************/

int devm_request_threaded_irq(struct device *dev, unsigned int irq,
			      irq_handler_t handler, irq_handler_t thread_fn,
			      unsigned long irqflags, const char *devname,
			      void *dev_id)
{
	if (irq < 32) {
		Genode::error(__func__, "(): unexpected irq ", irq);
		return -1;
	}

	Device_id id = UNKNOWN;
	unsigned off = 0;
	switch (irq) {
	case IRQ_IRQSTEER: id = DCSS; break;
	case IRQ_HDMI_IN:  id = HDMI; break;
	case IRQ_HDMI_OUT: id = HDMI; off = 1; break;
	default: Genode::error(__func__, " IRQ: ", irq, " not found");
	};

	Lx::Irq::irq().request_irq(Lx_kit::platform_device(id).irq(off),
	                           irq, handler, dev_id, thread_fn);

	return 0;
}


/**************************
 ** kernel/irq/irqdesc.c **
 **************************/

static irq_handler_t irqsteer_handler[32];
static void *irqsteer_dev_id[32];

int generic_handle_irq(unsigned int irq)
{
	/* only irqsteer irqs (< 32) are expected */

	if (irq > 31) {
		Genode::error(__func__, "(): unexpected irq ", irq);
		Genode::sleep_forever();
	}

	if (!irqsteer_handler[irq]) {
		Genode::error(__func__, "(): missing handler for irq ", irq);
		return -1;
	}
	
	irqsteer_handler[irq](irq, irqsteer_dev_id[irq]);

	return 0;
}


/****************************
 ** kernel/irq/irqdomain.c **
 ****************************/

struct irq_domain *__irq_domain_add(struct fwnode_handle *fwnode, int size,
				    irq_hw_number_t hwirq_max, int direct_max,
				    const struct irq_domain_ops *ops,
				    void *host_data)
{
	static struct irq_domain domain = {
		.ops = ops,
		.host_data = host_data,
	};

	{
		/* trigger a call of 'irq_set_chip_and_handler()' to get access to the irq_chip struct */
		static bool mapped = false;
		if (!mapped) {
			mapped = true;
			ops->map(&domain, 0, 0);
		}
	}

	return &domain;
}


/*************************
 ** kernel/irq/manage.c **
 *************************/

void enable_irq(unsigned int irq)
{
	if (irq < 32) {
		if (!irqsteer_chip)
			panic("'irqsteer_chip' uninitialized");

		struct irq_data irq_data {
			.hwirq = irq,
			.chip = irqsteer_chip,
			.chip_data = irqsteer_irq_desc.irq_common_data.handler_data,
		};

		irqsteer_chip->irq_unmask(&irq_data);
		return;
	}

	Lx::Irq::irq().enable_irq(irq);
}

void disable_irq(unsigned int irq)
{
	if (irq < 32) {
		if (!irqsteer_chip)
			panic("'irqsteer_chip' uninitialized");

		struct irq_data irq_data {
			.hwirq = irq,
			.chip = irqsteer_chip,
			.chip_data = irqsteer_irq_desc.irq_common_data.handler_data,
		};

		irqsteer_chip->irq_mask(&irq_data);
		return;
	}

	Lx::Irq::irq().disable_irq(irq);
}

int disable_irq_nosync(unsigned int irq)
{
	disable_irq(irq);
	return 0;
}


/******************
 ** lib/devres.c **
 ******************/

static void *_ioremap(phys_addr_t phys_addr, unsigned long size, int wc)
{
	using namespace Genode;

	Region_map & rm = Lx_kit::env().env().rm();

	if (phys_addr          >= IOMEM_BASE_DCSS &&
		(phys_addr+size-1) <= IOMEM_END_DCSS) {
		static Attached_dataspace ds {
			rm, Lx_kit::platform_device(DCSS).io_mem_dataspace() };
		addr_t off = phys_addr - IOMEM_BASE_DCSS;
		return (void*)(((addr_t)ds.local_addr<void>()) + off);
	};

	if (phys_addr          >= IOMEM_BASE_HDMI_CTRL &&
		(phys_addr+size-1) <= IOMEM_END_HDMI_RST) {
		switch (phys_addr) {
			case IOMEM_BASE_HDMI_CTRL:
				{
					static Attached_dataspace ds {
						rm, Lx_kit::platform_device(HDMI).io_mem_dataspace(0) };
					return ds.local_addr<void>();
				}
			case IOMEM_BASE_HDMI_CRS:
				{
					static Attached_dataspace ds {
						rm, Lx_kit::platform_device(HDMI).io_mem_dataspace(1) };
					return ds.local_addr<void>();
				}
			case IOMEM_BASE_HDMI_RST:
				{
					static Attached_dataspace ds {
						rm, Lx_kit::platform_device(HDMI).io_mem_dataspace(2) };
					return ds.local_addr<void>();
				}
			default: ;
		};
	};

	if (phys_addr >= IOMEM_BASE_MIPI_DSI &&
	   (phys_addr+size-1) <= IOMEM_END_MIPI_DSI) {

		/*
		 * Set parent of 'port' to 'mipi_dsi' in order to distinguish between HDMI
		 * and MIPI
		 */
		port_device_node.parent = &mipi_device_node;

		static Attached_dataspace ds {
			rm, Lx_kit::platform_device(MIPI).io_mem_dataspace(0) };
		addr_t off = phys_addr - IOMEM_BASE_MIPI_DSI;

		return (void*)(((addr_t)ds.local_addr<void>()) + off);
	}

	if (phys_addr >= IOMEM_BASE_SRC &&
	   (phys_addr+size-1) <= IOMEM_END_SRC) {
		static Attached_dataspace ds {
			rm, Lx_kit::platform_device(SRC).io_mem_dataspace(0) };
		return ds.local_addr<void>();
	}

	panic("Failed to request I/O memory: [%lx,%lx)\n",
	      phys_addr, phys_addr + size);
	return nullptr;
}

void *devm_ioremap(struct device *dev, resource_size_t offset,
                   unsigned long size)
{
	return _ioremap(offset, size, 0);
}

void *devm_ioremap_resource(struct device *dev, struct resource *res)
{
	return _ioremap(res->start, (res->end - res->start) + 1, 0);
}


/************************
 ** linux/mfd/syscon.h **
 ************************/

struct regmap *syscon_regmap_lookup_by_phandle( struct device_node *np, const char *property)
{
	bool src = strcmp(property, "src") == 0;

	if (!src) {
		if (DEBUG_DRIVER)
			Genode::warning(__func__, " property '", property, "' not found.");
		return 0;
	}

	regmap *map = (regmap *)kzalloc(sizeof(regmap), GFP_KERNEL);
	map->base   = (u8 *)_ioremap(IOMEM_BASE_SRC, 0x10000, 0);
	return map;
}


/********************
 ** linux/regmap.h **
 ********************/

int regmap_update_bits(struct regmap *map, unsigned reg, unsigned mask,
                       unsigned val)
{
	if (map == nullptr) return 0;

	unsigned volatile current = *((unsigned *)(map->base + reg));

	current &= ~mask;
	current |= val;
	*((volatile unsigned *)(map->base + reg)) = current;

	return 0;
}



/******************
 ** lib/string.c **
 ******************/

int strcmp(const char *a,const char *b)
{
	return Genode::strcmp(a, b);
}


/************************
 ** linux/completion.h **
 ************************/

void reinit_completion(struct completion *work)
{
	init_completion(work);
}


/********************
 ** linux/device.h **
 ********************/

/**
 * Simple driver management class
 */
class Driver : public Genode::List<Driver>::Element
{
	public:

		struct device_driver *_drv; /* Linux driver */

	public:

		Driver(struct device_driver *drv) : _drv(drv)
		{
			list()->insert(this);
		}

		/**
		 * List of all currently registered drivers
		 */
		static Genode::List<Driver> *list()
		{
			static Genode::List<Driver> _list;
			return &_list;
		}

		/**
		 * Match device and drivers
		 */
		bool match(struct device *dev)
		{
			/*
			 *  Don't try if buses don't match, since drivers often use 'container_of'
			 *  which might cast the device to non-matching type
			 */
			if (_drv->bus != dev->bus)
				return false;

			return _drv->bus->match ? _drv->bus->match(dev, _drv) : true;
		}

		/**
		 * Probe device with driver
		 */
		int probe(struct device *dev)
		{
			dev->driver = _drv;

			if (dev->bus->probe)
				return dev->bus->probe(dev);
			else if (_drv->probe)
				return _drv->probe(dev);

			return 0;
		}
};


int driver_register(struct device_driver *drv)
{
	new (Lx::Malloc::mem()) Driver(drv);
	return 0;
}


int device_add(struct device *dev)
{
	if (dev->driver)
		return 0;

	/* foreach driver match and probe device */
	for (Driver *driver = Driver::list()->first(); driver; driver = driver->next()) {
		if (driver->match(dev)) {
			int ret = driver->probe(dev);
			if (!ret) return 0;
		}
	}

	return 0;
}


static bus_type *_bus = nullptr;

int bus_register(struct bus_type *bus)
{
	if (_bus) {
		Genode::error(__func__, " called twice, implement list");
		return  -ENOMEM;
	}

	_bus = bus;
	return 0;
}


void *devm_kcalloc(struct device * /* dev */, size_t n, size_t size, gfp_t flags)
{
	return kcalloc(n, size, flags);
}


/*************************
 ** linux/dma-mapping.h **
 *************************/

struct Dma_wc_dataspace : Genode::Attached_ram_dataspace,
                          Genode::List<Dma_wc_dataspace>::Element
{
	Dma_wc_dataspace(size_t size)
	: Genode::Attached_ram_dataspace(Lx_kit::env().ram(),
	                                 Lx_kit::env().rm(),
	                                 size,
	                                 Genode::Cache::WRITE_COMBINED) { }
};

static Genode::List<Dma_wc_dataspace> &_dma_wc_ds_list()
{
	static Genode::List<Dma_wc_dataspace> inst;
	return inst;
}

void *dma_alloc_wc(struct device *dev, size_t size,
                   dma_addr_t *dma_addr, gfp_t gfp)
{
	Dma_wc_dataspace *dma_wc_ds = new (Lx::Malloc::mem()) Dma_wc_dataspace(size);

	_dma_wc_ds_list().insert(dma_wc_ds);

	*dma_addr = Genode::Dataspace_client(dma_wc_ds->cap()).phys_addr();
	return dma_wc_ds->local_addr<void>();
}

void dma_free_wc(struct device *dev, size_t size,
                 void *cpu_addr, dma_addr_t dma_addr)
{
	for (Dma_wc_dataspace *ds = _dma_wc_ds_list().first(); ds; ds = ds->next()) {
		if (ds->local_addr<void>() == cpu_addr) {
			_dma_wc_ds_list().remove(ds);
			destroy(Lx::Malloc::mem(), ds);
			return;
		}
	}

	Genode::error("dma_free_wc(): unknown address");
}


/***********************
 ** linux/interrupt.h **
 ***********************/

int devm_request_irq(struct device *dev, unsigned int irq,
                     irq_handler_t handler, unsigned long irqflags,
                     const char *devname, void *dev_id)
{
	if (irq < 32) {
		irqsteer_handler[irq] = handler;
		irqsteer_dev_id[irq] = dev_id;
		enable_irq(irq);
	} else {
		Device_id id = UNKNOWN;
		unsigned off = 0;
		switch (irq) {
			case IRQ_IRQSTEER: id = DCSS; break;
			case IRQ_HDMI_IN:  id = HDMI; break;
			case IRQ_HDMI_OUT: id = HDMI; off = 1; break;
			case IRQ_MIPI_DSI: id = MIPI; break;
			default: Genode::error(__func__, " IRQ: ", irq, " not found");
		};
		Lx::Irq::irq().request_irq(Lx_kit::platform_device(id).irq(off),
		                           irq, handler, dev_id);
	}

	return 0;
}


/*****************
 ** linux/irq.h **
 *****************/

void irq_set_chip_and_handler(unsigned int irq, struct irq_chip *chip,
                              irq_flow_handler_t)
{
	irqsteer_chip = chip;
}


/****************
 ** linux/of.h **
 ****************/

bool of_property_read_bool(const struct device_node *np, const char *propname)
{
	if ((Genode::strcmp(np->name, "hdmi", strlen(np->name)) == 0)) {

		if ((Genode::strcmp(propname, "fsl,cec", strlen(np->name)) == 0) ||
		    (Genode::strcmp(propname, "fsl,use_digpll_pclock", strlen(np->name)) == 0) ||
		    (Genode::strcmp(propname, "fsl,no_edid", strlen(np->name)) == 0))
			return false;

		if (DEBUG_DRIVER)
			Genode::error(__func__, "(): hdmi unhandled property '", propname,
			                        "' of device '", Genode::Cstring(np->name), "'");

		return false;
	}

	if ((Genode::strcmp(np->name, "mipi_dsi_bridge") == 0)) {
		if (Genode::strcmp(propname, "no_clk_reset") == 0) {
			/* set np in bridge endpoint */
			mipi_endpoint_device_node.properties = (property*)kzalloc(sizeof(property), 0);
			mipi_endpoint_device_node.properties->name  = "mipi_dsi_bridge_np";
			mipi_endpoint_device_node.properties->value = (void *)np;
			return true;
		}

		if (DEBUG_DRIVER)
			Genode::error(__func__, "(): mipi_dsi_bridge unhandled property '", propname,
			                        "' of device '", Genode::Cstring(np->name), "'");
		return false;
	}

	if (Genode::strcmp(np->name, "mipi_dsi") == 0) {
		if (Genode::strcmp(propname, "no_clk_reset") == 0)
			return true;

		if (DEBUG_DRIVER)
			Genode::error(__func__, "(): mipi_dsi unhandled property '", propname,
			                        "' of device '", Genode::Cstring(np->name), "'");
		return false;
	}

	if (DEBUG_DRIVER)
		Genode::error(__func__, "(): unhandled device '", Genode::Cstring(np->name),
		                        "' (property: '", Genode::Cstring(propname), "')");

	return false;
}

int of_property_read_string(const struct device_node *np, const char *propname,
                            const char **out_string)
{
	if (Genode::strcmp(np->name, "hdmi", strlen(np->name)) == 0) {

	    if (Genode::strcmp(propname, "compatible") == 0) {
			*out_string = "fsl,imx8mq-hdmi";
			return 0;
		}

		if (DEBUG_DRIVER)
			Genode::error(__func__, "(): unhandled property '", propname,
			                        "' of device '", Genode::Cstring(np->name), "'");
		return -1;
	}

	if (DEBUG_DRIVER)
		Genode::error(__func__, "(): unhandled device '", Genode::Cstring(np->name),
		                        "' (property: '", Genode::Cstring(propname), "')");
	return -1;
}

int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value)
{

	if ((Genode::strcmp(np->name, "imx-irqsteer", strlen(np->name)) == 0)) {

		if (Genode::strcmp(propname, "nxp,irqsteer_chans") == 0) {

			*out_value = 2;
			return 0;

		} else if (Genode::strcmp(propname, "nxp,endian") == 0) {

			*out_value = 1;
			return 0;
		}

		if (DEBUG_DRIVER)
			Genode::error(__func__, "(): unhandled property '", propname,
			                        "' of device '", Genode::Cstring(np->name), "'");
		return -1;

	} else if (Genode::strcmp(np->name, "hdmi", strlen(np->name)) == 0) {

	    if (Genode::strcmp(propname, "hdcp-config") == 0) {
		    /* no such property in original device tree */
			return -1;
		}


		if (DEBUG_DRIVER)
			Genode::error(__func__, "(): unhandled property '", propname,
			                        "' of device '", Genode::Cstring(np->name), "'");
		return -1;
	}

	int len = 0;
	void const *value = of_get_property(np, propname, &len);
	if (len > 0) {
		*out_value = (unsigned long)value;
		return 0;
	}

	if (DEBUG_DRIVER)
		Genode::error(__func__, "(): unhandled device '", Genode::Cstring(np->name),
		                        "' (property: '", Genode::Cstring(propname), "')");

	return -1;
}


/***************
 ** mm/util.c **
 ***************/

void kvfree(const void *p)
{
	kfree(p);
}


static struct drm_device * lx_drm_device = nullptr;

struct irq_chip dummy_irq_chip;

enum { MAX_BRIGHTNESS = 100U }; /* we prefer percentage */

struct Mutex_guard
{
	struct mutex &_mutex;
	Mutex_guard(struct mutex &m) : _mutex(m) { mutex_lock(&_mutex); }
	~Mutex_guard() { mutex_unlock(&_mutex); }
};

struct Drm_guard
{
	drm_device * _dev;

	Drm_guard(drm_device *dev) : _dev(dev)
	{
		if (dev) {
			mutex_lock(&dev->mode_config.mutex);
			mutex_lock(&dev->mode_config.blob_lock);
			drm_modeset_lock_all(dev);
		}
	}

	~Drm_guard()
	{
		if (_dev) {
			drm_modeset_unlock_all(_dev);
			mutex_unlock(&_dev->mode_config.mutex);
			mutex_unlock(&_dev->mode_config.blob_lock);
		}
	}
};


template <typename FUNCTOR>
static inline void lx_for_each_connector(drm_device * dev, FUNCTOR f)
{
	struct drm_connector *connector;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		f(connector);
}


drm_display_mode *
Framebuffer::Driver::_preferred_mode(drm_connector *connector,
                                     unsigned &brightness)
{
	using namespace Genode;
	using Genode::size_t;

	/* try to read configuration for connector */
	try {
		Xml_node config = _config.xml();
		Xml_node xn = config.sub_node();
		for (unsigned i = 0; i < config.num_sub_nodes(); xn = xn.next()) {
			if (!xn.has_type("connector"))
				continue;

			typedef String<64> Name;
			Name const con_policy = xn.attribute_value("name", Name());
			if (con_policy != connector->name)
				continue;

			bool enabled = xn.attribute_value("enabled", true);
			if (!enabled)
				return nullptr;

			brightness = xn.attribute_value("brightness",
			                                (unsigned)MAX_BRIGHTNESS + 1);

			unsigned long const width  = xn.attribute_value("width",  0UL);
			unsigned long const height = xn.attribute_value("height", 0UL);
			long          const hz     = xn.attribute_value("hz",     0L);

			struct drm_display_mode *mode;
			list_for_each_entry(mode, &connector->modes, head) {
			if (mode->hdisplay == (int) width &&
				mode->vdisplay == (int) height)
				if (!hz || hz == mode->vrefresh)
					return mode;
		};
		}
	} catch (...) {

		/**
		 * If no config is given, we take the most wide mode of a
		 * connector as long as it is connected at all
		 */
		if (connector->status != connector_status_connected)
			return nullptr;

		struct drm_display_mode *mode = nullptr, *tmp;
		list_for_each_entry(tmp, &connector->modes, head) {
			if (!mode || tmp->hdisplay > mode->hdisplay) mode = tmp;
		};
		return mode;
   	}

	return nullptr;
}


void Framebuffer::Driver::finish_initialization()
{
	if (!lx_drm_device) {
		Genode::error("no drm device");
		return;
	}

	lx_c_set_driver(lx_drm_device, (void*)this);

	generate_report();

	config_changed();
}


void Framebuffer::Driver::update_mode()
{
	using namespace Genode;

	Configuration old = _lx_config;
	_lx_config = Configuration();

	lx_for_each_connector(lx_drm_device, [&] (drm_connector *c) {
		unsigned brightness;
		drm_display_mode * mode = _preferred_mode(c, brightness);
		if (!mode) return;
		if (mode->hdisplay > _lx_config._lx.width)  _lx_config._lx.width  = mode->hdisplay;
		if (mode->vdisplay > _lx_config._lx.height) _lx_config._lx.height = mode->vdisplay;

	});

	lx_c_allocate_framebuffer(lx_drm_device, &_lx_config._lx);

	if (!_lx_config._lx.lx_fb) {
		Genode::error("updating framebuffer failed");
		return;
	}

	{
		Drm_guard guard(lx_drm_device);
		lx_for_each_connector(lx_drm_device, [&] (drm_connector *c) {
			unsigned brightness = MAX_BRIGHTNESS + 1;

			/* set mode */
			lx_c_set_mode(lx_drm_device, c, _lx_config._lx.lx_fb,
			              _preferred_mode(c, brightness));
		});
	}

	/* force virtual framebuffer size if requested */
	if (int w = _force_width_from_config())
		_lx_config._lx.width = min(_lx_config._lx.width, w);
	if (int h = _force_height_from_config())
		_lx_config._lx.height = min(_lx_config._lx.height, h);

	if (old._lx.lx_fb) {
		if (drm_framebuffer_read_refcount(old._lx.lx_fb) > 1) {
			/*
			 * If one sees this message, we are going to leak a lot of
			 * memory (e.g. framebuffer) and this will cause later on
			 * resource requests by this driver ...
			 */
			Genode::warning("framebuffer refcount ",
			                drm_framebuffer_read_refcount(old._lx.lx_fb));
		}
		drm_framebuffer_remove(old._lx.lx_fb);
	}
}


void Framebuffer::Driver::generate_report()
{
	/* detect mode information per connector */
	{
		Mutex_guard mutex(lx_drm_device->mode_config.mutex);

		struct drm_connector *c;
		list_for_each_entry(c, &lx_drm_device->mode_config.connector_list,
		                    head)
		{
			/*
			 * All states unequal to disconnected are handled as connected,
			 * since some displays stay in unknown state if not fill_modes()
			 * is called at least one time.
			 */
			bool connected = c->status != connector_status_disconnected;
			if ((connected && list_empty(&c->modes)) ||
			    (!connected && !list_empty(&c->modes)))
				c->funcs->fill_modes(c, 0, 0);
		}
	}

	/* check for report configuration option */
	try {
		_reporter.enabled(_config.xml().sub_node("report")
		                  .attribute_value(_reporter.name().string(), false));
	} catch (...) {
		_reporter.enabled(false);
	}
	if (!_reporter.enabled()) return;

	/* write new report */
	try {
		Genode::Reporter::Xml_generator xml(_reporter, [&] ()
		{
			Drm_guard guard(lx_drm_device);
			struct drm_connector *c;
			list_for_each_entry(c, &lx_drm_device->mode_config.connector_list,
			                    head) {
				xml.node("connector", [&] ()
				{
					bool connected = c->status == connector_status_connected;
					xml.attribute("name", c->name);
					xml.attribute("connected", connected);

					if (!connected) return;
					struct drm_display_mode *mode;
					list_for_each_entry(mode, &c->modes, head) {
						xml.node("mode", [&] ()
						{
							xml.attribute("width",  mode->hdisplay);
							xml.attribute("height", mode->vdisplay);
							xml.attribute("hz",     mode->vrefresh);
						});
					}
				});
			}
		});
	} catch (...) {
		Genode::warning("Failed to generate report");
	}
}


extern "C" {


/****************************
 ** kernel/printk/printk.c **
 ****************************/
 
int oops_in_progress;


/********************
 ** linux/string.h **
 ********************/

char *strncpy(char *dst, const char* src, size_t n)
{
	Genode::copy_cstring(dst, src, n);
	return dst;
}

int strncmp(const char *cs, const char *ct, size_t count)
{
	return Genode::strcmp(cs, ct, count);
}

int memcmp(const void *cs, const void *ct, size_t count)
{
	/* original implementation from lib/string.c */
	const unsigned char *su1, *su2;
	int res = 0;

	for (su1 = (unsigned char*)cs, su2 = (unsigned char*)ct;
	     0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}

void *memchr_inv(const void *s, int cc, size_t n)
{
	if (!s)
		return NULL;

	uint8_t const c = cc;
	uint8_t const * start = (uint8_t const *)s;

	for (uint8_t const *i = start; i >= start && i < start + n; i++)
		if (*i != c)
			return (void *)i;

	return NULL;
}

size_t strlen(const char *s)
{
	return Genode::strlen(s);
}

long simple_strtol(const char *cp, char **endp, unsigned int base)
{
	unsigned long result = 0;
	size_t ret = Genode::ascii_to_unsigned(cp, result, base);
	if (endp) *endp = (char*)cp + ret;
	return result;
}

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		Genode::memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}


/*******************
 ** Kernel memory **
 *******************/

void *krealloc(const void *p, size_t size, gfp_t flags)
{
	/* use const-less version from <impl/slab.h> */
	return krealloc(const_cast<void*>(p), size, flags);
}


/*******************************
 ** arch/x86/include/asm/io.h **
 *******************************/

void memset_io(void *addr, int val, size_t count)
{
	memset((void __force *)addr, val, count);
}


/********************
 ** linux/device.h **
 ********************/

int dev_set_name(struct device *dev, const char *name, ...)
{
	TRACE;
	return 0;
}

void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return kzalloc(size, gfp);
}


/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct *system_wq = nullptr;

struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), fmt);
	wq->task       = (void *)work;

	return wq;
}

struct workqueue_struct *alloc_ordered_workqueue(char const *fmt , unsigned int flags, ...)
{
	return alloc_workqueue(fmt, flags, 1);
}

bool mod_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork,
                      unsigned long delay)
{
	TRACE;
	return queue_delayed_work(wq, dwork, delay);
}

void flush_workqueue(struct workqueue_struct *wq)
{
	Lx::Task *current = Lx::scheduler().current();
	if (!current) {
		Genode::error("BUG: flush_workqueue executed without task");
		Genode::sleep_forever();
	}

	Lx::Work *lx_work = (wq && wq->task) ? (Lx::Work*) wq->task
	                                     : &Lx::Work::work_queue();

	lx_work->flush(*current);
	Lx::scheduler().current()->block_and_schedule();
}


/***************
 ** Execution **
 ***************/

void preempt_enable(void)
{
	TRACE;
}

void preempt_disable(void)
{
	TRACE;
}

void usleep_range(unsigned long min, unsigned long max)
{
	udelay(min);
}


/*******************
 ** linux/timer.h **
 *******************/

struct callback_timer {
	void (*function)(unsigned long);
	unsigned long data;
};

/*
 * For compatibility with 4.4.3 drivers, the argument of this callback function
 * is the 'data' member of the 'timer_list' object, which normally points to
 * the 'timer_list' object itself when initialized with 'timer_setup()', but
 * here it was overridden in 'setup_timer()' to point to the 'callback_timer'
 * object instead.
 */
static void timer_callback(struct timer_list *t)
{
	struct callback_timer * tc = (struct callback_timer *)t;
	tc->function(tc->data);
}

extern "C" void setup_timer(struct timer_list *timer, void (*function)(unsigned long),
                            unsigned long data)
{
	callback_timer * tc = new (Lx::Malloc::mem()) callback_timer;
	tc->function = function;
	tc->data     = data;

	timer_setup(timer, timer_callback, 0u);
	timer->data = (unsigned long)tc;
}


/************************
 ** DRM implementation **
 ************************/

unsigned int drm_debug = 0x0;

struct drm_device *drm_dev_alloc(struct drm_driver *driver,
				 struct device *parent)
{
	struct drm_device *dev;
	int ret;

	dev = (struct drm_device*)kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return (struct drm_device*)ERR_PTR(-ENOMEM);

	ret = drm_dev_init(dev, driver, parent);
	if (ret) {
		kfree(dev);
		return (struct drm_device*)ERR_PTR(ret);
	}

	return dev;
}


int drm_dev_init(struct drm_device *dev, struct drm_driver *driver,
                 struct device *parent)
{
	TRACE;

	kref_init(&dev->ref);
	dev->dev = parent;
	dev->driver = driver;

	INIT_LIST_HEAD(&dev->filelist);
	INIT_LIST_HEAD(&dev->ctxlist);
	INIT_LIST_HEAD(&dev->vmalist);
	INIT_LIST_HEAD(&dev->maplist);
	INIT_LIST_HEAD(&dev->vblank_event_list);

	spin_lock_init(&dev->buf_lock);
	spin_lock_init(&dev->event_lock);
	mutex_init(&dev->struct_mutex);
	mutex_init(&dev->filelist_mutex);
	mutex_init(&dev->ctxlist_mutex);
	mutex_init(&dev->master_mutex);

	if (drm_gem_init(dev) != 0)
		DRM_ERROR("Cannot initialize graphics execution manager (GEM)\n");

	return 0;
}


void drm_send_event_locked(struct drm_device *dev, struct drm_pending_event *e)
{
	if (e->completion) {
		complete_all(e->completion);
		e->completion_release(e->completion);
		e->completion = NULL;
	}

	if (e->fence) {
		TRACE_AND_STOP;
	}
}

static void drm_get_minor(struct drm_device *dev, struct drm_minor **minor, int type)
{
	struct drm_minor *new_minor = (struct drm_minor*)
		kzalloc(sizeof(struct drm_minor), GFP_KERNEL);
	ASSERT(new_minor);
	new_minor->type = type;
	new_minor->dev = dev;
	*minor = new_minor;
}

int drm_dev_register(struct drm_device *dev, unsigned long flags)
{
	drm_get_minor(dev, &dev->primary, DRM_MINOR_PRIMARY);

	int ret = 0;

	ASSERT(!lx_drm_device);
	lx_drm_device = dev;

	dev->registered = true;

	if (dev->driver->load) {
		ret = dev->driver->load(dev, flags);
		if (ret)
			return ret;
	}

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_modeset_register_all(dev);

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 dev->driver->name, dev->driver->major, dev->driver->minor,
		 dev->driver->patchlevel, dev->driver->date,
		 dev->primary->index);

	return 0;
}


/**************************************
 ** arch/arm64/include/asm/processor.h **
 **************************************/

void cpu_relax(void)
{
	Lx::timer_update_jiffies();
	asm volatile("yield" ::: "memory");
}


/******************
 ** linux/kref.h **
 ******************/

void kref_init(struct kref *kref) {
	kref->refcount.counter = 1; }

void kref_get(struct kref *kref)
{
	if (!kref->refcount.counter)
		Genode::error(__func__, " kref already zero");

	kref->refcount.counter++;
}

int kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	if (!kref->refcount.counter) {
		Genode::error(__func__, " kref already zero");
		return 1;
	}

	kref->refcount.counter--;
	if (kref->refcount.counter == 0) {
		release(kref);
		return 1;
	}
	return 0;
}

int kref_put_mutex(struct kref *kref, void (*release)(struct kref *kref), struct mutex *lock)
{
	if (kref_put(kref, release)) {
		mutex_lock(lock);
		return 1;
	}
	return 0;
}

int kref_get_unless_zero(struct kref *kref)
{
	if (!kref->refcount.counter)
		return 0;

	kref_get(kref);
	return 1;
}

void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > SIZE_MAX / size) return NULL;
	return kmalloc(n * size, flags);
}

unsigned int kref_read(const struct kref *kref)
{
	TRACE;
	return atomic_read(&kref->refcount);
}



/****************************
 ** drivers/phy/phy-core.c **
 ****************************/

void devm_phy_consume(struct device *dev, void *res)
{
	TRACE;
}

struct phy_ops;
struct phy *devm_phy_create(struct device *dev,
                            struct device_node *node,
                            const struct phy_ops *ops)
{
	TRACE;

	phy **ptr = (phy**)devres_alloc(devm_phy_consume, sizeof(*ptr), GFP_KERNEL);
	phy *p    = (phy *)kzalloc(sizeof(phy), GFP_KERNEL);

	p->dev.of_node = node;
	p->ops         = ops;
	p->dev.parent  = dev;

	*ptr = p;
	devres_add(dev, ptr);

	return p;
}


struct phy *devm_phy_get(struct device *dev, const char *string)
{
	int len = 0;
	void const *phy = of_get_property(dev->of_node, string, &len);
	return (struct phy *)phy;
}


int phy_init(struct phy *phy)
{
	TRACE;

	if (phy && phy->ops->init) {

		int ret =  phy->ops->init(phy);
		if (ret)
			Genode::error(__func__, " failed (err: ", ret, ")");

		return ret;
	}

	return 0;
}


int phy_power_on(struct phy *phy)
{
	TRACE;

	if (phy && phy->ops->power_on) {

		int ret = phy->ops->power_on(phy);
		if (ret)
			Genode::error(__func__, " failed (err: ", ret, ")");

		return ret;
	}

	return 0;
}

/***********************
 ** linux/backlight.h **
 ***********************/


struct backlight_device *
devm_backlight_device_register(struct device *dev, const char *name,
                               struct device *parent, void *devdata,
                               const struct backlight_ops *ops,
                               const struct backlight_properties *props)
{
	TRACE;

	backlight_device *backlight = (backlight_device *)kzalloc(sizeof(backlight_device), GFP_KERNEL);
	backlight->ops   = ops;
	backlight->props = *props;
	dev_set_drvdata(&backlight->dev, devdata);

	return backlight;
}


int backlight_enable(struct backlight_device *bd)
{
	int ret = -ENOENT;

	if (bd->ops && bd->ops->update_status)
		ret = bd->ops->update_status(bd);

	return ret;
}


void *bl_get_data(struct backlight_device *bl_dev)
{
	return dev_get_drvdata(&bl_dev->dev);
}


/*********************
 ** drm/drm_panel.h **
 *********************/

int drm_panel_add(struct drm_panel *panel)
{
	device_node *np = &mipi_panel_node;

	if (Genode::strcmp(np->properties[0].name, "panel") != 0) {
		Genode::error("panel property not found");
		return -1;
	}

	np->properties[0].value = (void*)panel;

	return 0;
}


int drm_panel_attach(struct drm_panel *panel, struct drm_connector *connector)
{
	if (panel->connector)
		return -EBUSY;

	panel->connector = connector;
	panel->drm = connector->dev;

	return 0;
}


struct drm_panel *of_drm_find_panel(const struct device_node *np)
{
	int len;
	return (drm_panel *)of_get_property(np, "panel", &len);
}


/***************************
 ** linux/gpio/consumer.h **
 ***************************/

struct gpio_desc *
devm_gpiod_get(struct device *dev, const char *con_id, enum gpiod_flags flags)
{
	TRACE;
	return (struct gpio_desc *)-EINVAL;
}

void gpiod_set_value(struct gpio_desc *desc, int value)
{
	TRACE;
}


/**************************************
 ** Stubs for non-ported driver code **
 **************************************/

int drm_sysfs_connector_add(struct drm_connector *connector)
{
	TRACE;
	connector->kdev = (struct device*)
		kmalloc(sizeof(struct device), GFP_KERNEL);
	DRM_DEBUG("adding \"%s\" to sysfs\n", connector->name);
	drm_sysfs_hotplug_event(connector->dev);
	return 0;
}

void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	kfree(connector->kdev);
	connector->kdev = nullptr;
	DRM_DEBUG("removing \"%s\" from sysfs\n", connector->name);
	drm_sysfs_hotplug_event(connector->dev);
}

void spin_lock_irq(spinlock_t *lock)
{
	TRACE;
}

void spin_unlock_irq(spinlock_t *lock)
{
	TRACE;
}

int fb_get_options(const char *name, char **option)
{
	return 0;
}

void spin_lock(spinlock_t *lock)
{
	TRACE;
}

struct file *shmem_file_setup(const char *name, loff_t const size,
                              unsigned long flags)
{
	return nullptr;
}

void fput(struct file *file)
{
	if (!file)
		return;

	if (file->f_mapping) {
		if (file->f_mapping->my_page) {
			free_pages((unsigned long)file->f_mapping->my_page->addr, /* unknown order */ 0);
			file->f_mapping->my_page = nullptr;
		}
		kfree(file->f_mapping);
	}
	kfree(file);
}

void ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class)
{
	lock->ctx = NULL;
	lock->locked = false;
}

void ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class)
{
	TRACE;
}

int  ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	if (ctx && (lock->ctx == ctx))
		return -EALREADY;

	if (lock->locked) {
		Genode::warning(__func__, " already locked");
		return 1;
	}

	lock->ctx = ctx;
	lock->locked = true;
	return 0;
}

void ww_mutex_unlock(struct ww_mutex *lock)
{
	lock->ctx = NULL;
	lock->locked = false;
}

bool ww_mutex_is_locked(struct ww_mutex *lock)
{
	return lock->locked;
}

void ww_acquire_fini(struct ww_acquire_ctx *ctx)
{
	TRACE;
}

void drm_sysfs_hotplug_event(struct drm_device *dev)
{
	Framebuffer::Driver * driver = (Framebuffer::Driver*)
		lx_c_get_driver(lx_drm_device);

	if (driver) {
		DRM_DEBUG("generating hotplug event\n");
		driver->generate_report();
		driver->trigger_reconfiguration();
	}
}

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

unsigned long find_next_bit(const unsigned long *addr, unsigned long nbits,
                            unsigned long start)
{
	unsigned long tmp;

	if (!nbits || start >= nbits)
		return nbits;

	tmp = addr[start / BITS_PER_LONG] ^ 0UL;

	/* Handle 1st word. */
	tmp &= BITMAP_FIRST_WORD_MASK(start);
	start = round_down(start, BITS_PER_LONG);

	while (!tmp) {
		start += BITS_PER_LONG;
		if (start >= nbits)
			return nbits;
		tmp = addr[start / BITS_PER_LONG] ^ 0UL;
	}

	return min(start + __ffs(tmp), nbits);
}

void bitmap_set(unsigned long *map, unsigned int start, int len)
{
	unsigned long *p = map + BIT_WORD(start);
	const unsigned int size = start + len;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (len - bits_to_set >= 0) {
		*p |= mask_to_set;
		len -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (len) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		*p |= mask_to_set;
	}
}

unsigned long find_next_zero_bit(unsigned long const *addr, unsigned long size,
                                 unsigned long offset)
{
	unsigned long i, j;

	for (i = offset; i < (size / BITS_PER_LONG); i++)
		if (addr[i] != ~0UL)
			break;

	if (i == size)
		return size;

	for (j = 0; j < BITS_PER_LONG; j++)
		if ((~addr[i]) & (1UL << j))
			break;

	return (i * BITS_PER_LONG) + j;
}

unsigned int irq_find_mapping(struct irq_domain *, irq_hw_number_t hwirq)
{
	/* only irqsteer irqs (< 32) are expected */

	if (hwirq > 31) {
		Genode::error(__func__, "(): unexpected hwirq ", hwirq);
		Genode::sleep_forever();
	}

	return hwirq;
}

void drm_printk(const char *level, unsigned int category, const char *format,
                ...)
{
	struct va_format vaf;
	va_list args;

	if (category && !(drm_debug & category))
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk("%s" "[drm:%ps]%s %pV\n",
	       level, __builtin_return_address(0),
	       strcmp(level, KERN_ERR) == 0 ? " *ERROR*" : "", &vaf);
	(void)vaf;

	va_end(args);
}

int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
	Genode::String_console sc(str, size);
	sc.vprintf(format, args);

	return sc.len();
}

char *kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	size_t const bad_guess = strlen(fmt) + 10;
	char * const p = (char *)kmalloc(bad_guess, gfp);
	if (!p)
		return NULL;

	vsnprintf(p, bad_guess, fmt, ap);

	return p;
}

static void _completion_timeout(struct timer_list *list)
{
	struct process_timer *timeout = from_timer(timeout, list, timer);
	timeout->task.unblock();
}

long __wait_completion(struct completion *work, unsigned long timeout)
{
	Lx::timer_update_jiffies();
	unsigned long j = timeout ? jiffies + timeout : 0;

	Lx::Task & cur_task = *Lx::scheduler().current();
	struct process_timer timer { cur_task };

	if (timeout) {
		timer_setup(&timer.timer, _completion_timeout, 0);
		mod_timer(&timer.timer, j);
	}

	while (!work->done) {

		if (j && j <= jiffies) {
			lx_log(1, "timeout jiffies %lu", jiffies);
			return 0;
		}

		Lx::Task *task = Lx::scheduler().current();
		work->task = (void *)task;
		task->block_and_schedule();
	}

	if (timeout)
		del_timer(&timer.timer);

	return (j  || j == jiffies) ? 1 : j - jiffies;
}

} /* extern "C" */
