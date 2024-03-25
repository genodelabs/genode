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


/**********************
 ** Device::Io_port **
 **********************/

bool Device::Io_port::match(uint16_t addr)
{
	return (this->addr <= addr) &&
	       ((this->addr + this->size) > addr);
}


/*****************
 ** Device::Irq **
 *****************/

void Device::Irq::_handle()
{
	switch (state) {
	case IDLE:           state = PENDING;        break;
	case PENDING:        state = PENDING;        break;
	case MASKED:         state = MASKED_PENDING; break;
	case MASKED_PENDING: state = MASKED_PENDING; break;
	}

	env().scheduler.unblock_irq_handler();
	env().scheduler.schedule();
}


void Device::Irq::ack()
{
	if (session.constructed())
		session->ack();

	switch (state) {
	case IDLE:           state = IDLE;   break;
	case PENDING:        state = IDLE;   break;
	case MASKED:         state = MASKED; break;
	case MASKED_PENDING: state = MASKED; break;
	}
}


void Device::Irq::mask()
{
	switch (state) {
	case IDLE:           state = MASKED;         break;
	case MASKED:         state = MASKED;         break;
	case PENDING:        state = MASKED_PENDING; break;
	case MASKED_PENDING: state = MASKED_PENDING; break;
	}
}


void Device::Irq::unmask(Platform::Device &dev)
{
	if (!session.constructed()) {
		session.construct(dev, idx);
		session->sigh_omit_initial_signal(handler);
		session->ack();
	}

	switch (state) {
	case IDLE:           state = IDLE;    break;
	case MASKED:         state = IDLE;    break;
	case PENDING:        state = PENDING; break;
	case MASKED_PENDING: state = PENDING; break;
	}

	env().scheduler.unblock_irq_handler();
}


Device::Irq::Irq(Entrypoint & ep, unsigned idx, unsigned number)
:
	idx{idx},
	number(number),
	handler(ep, *this, &Irq::_handle)
{ }


/************
 ** Device **
 ************/

const char * Device::compatible()
{
	return _type.name.string();
}


Device::Name Device::name()
{
	return _name;
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
	for_each_io_mem([&] (Io_mem & io) {
		if (io.match(phys_addr, size))
			ret = true;
	});
	return ret;
}


void * Device::io_mem_local_addr(addr_t phys_addr, size_t size)
{
	void * ret = nullptr;
	for_each_io_mem([&] (Io_mem & io) {
		if (!io.match(phys_addr, size))
			return;

		enable();

		if (!io.io_mem.constructed())
			io.io_mem.construct(*_pdev, io.idx);

		ret = (void*)((addr_t)io.io_mem->local_addr<void>() + phys_addr - io.addr);
	});
	return ret;
}


int Device::pending_irq()
{
	if (!_pdev.constructed())
		return -1;

	int result = -1;

	for_each_irq([&] (Irq & irq) {
		if (result == -1 && irq.state == Irq::PENDING)
			result = irq.number;
	});

	return result;
}


bool Device::irq_unmask(unsigned number)
{
	bool ret = false;

	for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;

		ret = true;
		enable();
		irq.unmask(*_pdev);
	});

	return ret;
}


void Device::irq_mask(unsigned number)
{
	if (!_pdev.constructed())
		return;

	for_each_irq([&] (Irq & irq) {
		if (irq.number == number) irq.mask(); });
}


void Device::irq_ack(unsigned number)
{
	if (!_pdev.constructed())
		return;

	for_each_irq([&] (Irq & irq) {
		if (irq.number != number)
			return;
		irq.ack();
	});
}


bool Device::io_port(uint16_t addr)
{
	bool ret = false;
	for_each_io_port([&] (Io_port & io) {
		if (io.match(addr))
			ret = true;
	});
	return ret;
}


uint8_t Device::io_port_inb(uint16_t addr)
{
	uint8_t ret = 0;
	for_each_io_port([&] (Device::Io_port & io) {
		if (!io.match(addr))
			return;

		if (!io.io_port.constructed())
			io.io_port.construct(*_pdev, io.idx);

		ret = io.io_port->inb(addr);
	});

	return ret;
}


uint16_t Device::io_port_inw(uint16_t addr)
{
	uint16_t ret = 0;
	for_each_io_port([&] (Device::Io_port & io) {
		if (!io.match(addr))
			return;

		if (!io.io_port.constructed())
			io.io_port.construct(*_pdev, io.idx);

		ret = io.io_port->inw(addr);
	});

	return ret;
}


uint32_t Device::io_port_inl(uint16_t addr)
{
	uint32_t ret = 0;
	for_each_io_port([&] (Device::Io_port & io) {
		if (!io.match(addr))
			return;

		if (!io.io_port.constructed())
			io.io_port.construct(*_pdev, io.idx);

		ret = io.io_port->inl(addr);
	});

	return ret;
}


void Device::io_port_outb(uint16_t addr, uint8_t val)
{
	for_each_io_port([&] (Device::Io_port & io) {
		if (!io.match(addr))
			return;

		if (!io.io_port.constructed())
			io.io_port.construct(*_pdev, io.idx);

		io.io_port->outb(addr, val);
	});
}


void Device::io_port_outw(uint16_t addr, uint16_t val)
{
	for_each_io_port([&] (Device::Io_port & io) {
		if (!io.match(addr))
			return;

		if (!io.io_port.constructed())
			io.io_port.construct(*_pdev, io.idx);

		io.io_port->outw(addr, val);
	});
}


void Device::io_port_outl(uint16_t addr, uint32_t val)
{
	for_each_io_port([&] (Device::Io_port & io) {
		if (!io.match(addr))
			return;

		if (!io.io_port.constructed())
			io.io_port.construct(*_pdev, io.idx);

		io.io_port->outl(addr, val);
	});
}


void Device::enable()
{
	if (_pdev.constructed())
		return;

	_pdev.construct(_platform, _name);

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


Device::Device(Entrypoint           & ep,
               Platform::Connection & plat,
               Xml_node             & xml,
               Heap                 & heap)
:
	_platform(plat),
	_name(xml.attribute_value("name", Device::Name())),
	_type{xml.attribute_value("type", Device::Name())}
{
	unsigned i = 0;
	xml.for_each_sub_node("io_mem", [&] (Xml_node node) {
		addr_t   addr = node.attribute_value("phys_addr", 0UL);
		size_t   size = node.attribute_value("size",      0UL);
		unsigned bar  = node.attribute_value("pci_bar",   0U);
		_io_mems.insert(new (heap) Io_mem(i++, addr, size, bar));
	});

	i = 0;
	xml.for_each_sub_node("io_port_range", [&] (Xml_node node) {
		uint16_t addr = node.attribute_value<uint16_t>("phys_addr", 0U);
		uint16_t size = node.attribute_value<uint16_t>("size",      0U);
		unsigned bar  = node.attribute_value("pci_bar",             0U);
		_io_ports.insert(new (heap) Io_port(i++, addr, size, bar));
	});

	i = 0;
	xml.for_each_sub_node("irq", [&] (Xml_node node) {
		_irqs.insert(new (heap) Irq(ep, i++, node.attribute_value("number", 0U)));
	});

	i = 0;
	xml.for_each_sub_node("clock", [&] (Xml_node node) {
		Device::Name name = node.attribute_value("name", Device::Name());
		_clocks.insert(new (heap) Device::Clock(i++, name));
	});

	xml.for_each_sub_node("pci-config",  [&] (Xml_node node) {
		using namespace Pci;
		_pci_config.construct(Pci_config{
			node.attribute_value<vendor_t>("vendor_id", 0xffff),
			node.attribute_value<device_t>("device_id", 0xffff),
			node.attribute_value<class_t>("class", 0xff),
			node.attribute_value<rev_t>("revision", 0xff),
			node.attribute_value<vendor_t>("sub_vendor_id", 0xffff),
			node.attribute_value<device_t>("sub_device_id", 0xffff)});
	});
}


/*****************
 ** Device_list **
 *****************/

Device_list::Device_list(Entrypoint           & ep,
                         Heap                 & heap,
                         Platform::Connection & platform)
:
	_platform(platform)
{
	bool initialized = false;
	Constructible<Io_signal_handler<Device_list>> handler {};

	while (!initialized) {
		_platform.update();
		_platform.with_xml([&] (Xml_node & xml) {
			if (!xml.num_sub_nodes()) {
				if (!handler.constructed()) {
					handler.construct(ep, *this, &Device_list::_handle_signal);
					_platform.sigh(*handler);
				}
				ep.wait_and_dispatch_one_io_signal();
			} else {
				_platform.sigh(Signal_context_capability());
				handler.destruct();
				xml.for_each_sub_node("device", [&] (Xml_node node) {
					insert(new (heap) Device(ep, _platform, node, heap));
				});
				initialized = true;
			}
		});
	}
}
