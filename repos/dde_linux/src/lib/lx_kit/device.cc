/*
 * \brief  Lx_kit device
 * \author Stefan Kalkowski
 * \date   2021-05-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>

using namespace Lx_kit;


/********************
 ** Device::Io_mem **
 ********************/

bool Device::Io_mem::match(addr_t addr, size_t size)
{
	return (this->addr <= addr) &&
	       ((this->addr + this->size) >= (addr + size));
}


/*************************
 ** Device::Irq_handler **
 *************************/

void Device::Irq_handler::_handle()
{
	env().last_irq = _number;
	env().scheduler.unblock_irq_handler();
	env().scheduler.schedule();
}


Device::Irq_handler::Irq_handler(Platform::Device & dev,
                                 Platform::Device::Irq::Index idx,
                                 unsigned number)
:
	_irq(dev, idx),
	_handler(Lx_kit::env().env.ep(), *this, &Irq_handler::_handle),
	_number(number)
{
	_irq.sigh_omit_initial_signal(_handler);
	_irq.ack();
}


/************
 ** Device **
 ************/

const char * Device::compatible()
{
	return _type.name.string();
}


const char * Device::name()
{
	return _name.string();
}


clk * Device::clock(const char * name)
{
	clk * ret = nullptr;
	_for_each_clock([&] (Clock & c) {
		if (c.name == name) {
			enable();
			ret = &c.lx_clock;
		}
	});
	return ret;
}


clk * Device::clock(unsigned idx)
{
	clk * ret = nullptr;
	_for_each_clock([&] (Clock & c) {
		if (c.idx == idx) {
			enable();
			ret = &c.lx_clock;
		}
	});
	return ret;
}


bool Device::io_mem(addr_t phys_addr, size_t size)
{
	bool ret = false;
	_for_each_io_mem([&] (Io_mem & io) {
		if (io.match(phys_addr, size))
			ret = true;
	});
	return ret;
}


void * Device::io_mem_local_addr(addr_t phys_addr, size_t size)
{
	void * ret = nullptr;
	_for_each_io_mem([&] (Io_mem & io) {
		if (!io.match(phys_addr, size))
			return;

		enable();

		if (!io.io_mem.constructed())
			io.io_mem.construct(*_pdev, io.idx);

		ret = (void*)((addr_t)io.io_mem->local_addr<void>()
		              + (phys_addr - io.addr));
	});
	return ret;
}


bool Device::irq_unmask(unsigned number)
{
	bool ret = false;

	_for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;

		enable();

		if (irq.handler.constructed())
			return;

		irq.handler.construct(*_pdev, irq.idx, number);
		ret = true;
	});

	return ret;
}


void Device::irq_mask(unsigned number)
{
	if (!_pdev.constructed())
		return;

	_for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;
		irq.handler.destruct();
	});

}


void Device::irq_ack(unsigned number)
{
	if (!_pdev.constructed())
		return;

	_for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;
		irq.handler->ack();
	});
}


void Device::enable()
{
	if (_pdev.constructed())
		return;

	_pdev.construct(_platform, _type);

	_platform.update();
	_platform.with_xml([&] (Xml_node & xml) {
		xml.for_each_sub_node("device", [&] (Xml_node node) {
			if (_name != node.attribute_value("name", Device::Name()))
				return;

			node.for_each_sub_node("clock", [&] (Xml_node node) {
				clk * c = clock(node.attribute_value("name", Device::Name()).string());
				if (!c)
					return;
				c->rate = node.attribute_value("rate", 0UL);
			});
		});
	});
}


Device::Device(Platform::Connection & plat,
               Xml_node             & xml,
               Heap                 & heap)
:
	_platform(plat),
	_name(xml.attribute_value("name", Device::Name())),
	_type{xml.attribute_value("type", Device::Name())}
{
	unsigned i = 0;
	xml.for_each_sub_node("io_mem", [&] (Xml_node node) {
		addr_t addr = node.attribute_value("phys_addr", 0UL);
		size_t size = node.attribute_value("size",      0UL);
		_io_mems.insert(new (heap) Io_mem(i++, addr, size));
	});

	i = 0;
	xml.for_each_sub_node("irq", [&] (Xml_node node) {
		_irqs.insert(new (heap) Irq(i++, node.attribute_value("number", 0U)));
	});

	i = 0;
	xml.for_each_sub_node("clock", [&] (Xml_node node) {
		Device::Name name = node.attribute_value("name", Device::Name());
		_clocks.insert(new (heap) Device::Clock(i++, name));
	});
}


/*****************
 ** Device_list **
 *****************/

Device_list::Device_list(Heap & heap, Platform::Connection & platform)
:
	_platform(platform)
{
	_platform.with_xml([&] (Xml_node & xml) {
		xml.for_each_sub_node("device", [&] (Xml_node node) {
			insert(new (heap) Device(_platform, node, heap));
		});
	});
}
