/*
 * \brief  VMM for ARM virtualization - config frontend
 * \author Stefan Kalkowski
 * \date   2022-11-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__CONFIG_H_
#define _SRC__SERVER__VMM__CONFIG_H_

#include <board.h>
#include <base/allocator_avl.h>
#include <base/heap.h>
#include <util/bit_allocator.h>
#include <util/list_model.h>
#include <util/string.h>
#include <util/xml_node.h>

namespace Vmm {
	class Config;
	using namespace Genode;
}

class Vmm::Config
{
	public:

		struct Invalid_configuration{};

		using Name      = String<128>;
		using Arguments = String<512>;

		struct Virtio_device : List_model<Virtio_device>::Element
		{
			enum Type { INVALID, CONSOLE, NET, BLOCK, GPU, INPUT };

			enum { MMIO_SIZE = 0x200 };

			Config & _config;

			Virtio_device(Name & name, Type type, Config & config);
			~Virtio_device();

			Name     const name;
			Type     const type;
			void   * const mmio_start;
			size_t   const mmio_size;
			unsigned const irq;
		};

	private:

		struct Irq_allocator
		{
			Bit_allocator<VIRTIO_IRQ_COUNT> _alloc {};

			unsigned alloc() {
				return VIRTIO_IRQ_START + _alloc.alloc(); }
			void free(unsigned irq) {
				_alloc.free(VIRTIO_IRQ_START+irq); }
		};

		Heap        & _heap;
		Allocator_avl _mmio_alloc  { &_heap };
		Irq_allocator _irq_alloc   {   };
		Name          _kernel_name {   };
		Name          _initrd_name {   };
		size_t        _ram_size    { 0 };
		unsigned      _cpu_count   { 0 };
		Name          _cpu_type    {   };
		unsigned      _gic_version { 0 };
		Arguments     _bootargs    {   };

		List_model<Virtio_device> _model;

		struct Virtio_device_update_policy
			: List_model<Config::Virtio_device>::Update_policy
		{
			Config & config;

			Virtio_device_update_policy(Config & config)
			: config(config) {}

			static Virtio_device::Type type(Xml_node node)
			{
				Virtio_device::Type t = Virtio_device::INVALID;
				Config::Name type = node.attribute_value("type", Config::Name());
				if (type == "console") t = Virtio_device::CONSOLE;
				if (type == "net")     t = Virtio_device::NET;
				if (type == "block")   t = Virtio_device::BLOCK;
				if (type == "gpu")     t = Virtio_device::GPU;
				if (type == "input")   t = Virtio_device::INPUT;
				return t;
			}

			void destroy_element(Element & dev) { destroy(config._heap, &dev); }

			Element & create_element(Genode::Xml_node node)
			{
				Config::Name name = node.attribute_value("name", Config::Name());
				Virtio_device::Type t = type(node);
				if (t == Virtio_device::INVALID || !name.valid()) {
					error("Invalid type or missing name in Virtio device node");
					throw Invalid_configuration();
				}
				return *(new (config._heap) Element(name, t, config));
			}

			void update_element(Element &, Genode::Xml_node) {}

			static bool element_matches_xml_node(Element const & dev, Xml_node node)
			{
				Config::Name name = node.attribute_value("name", Config::Name());
				Virtio_device::Type t = type(node);
				return name == dev.name && t == dev.type;
			}

			static bool node_is_element(Xml_node node) {
				return node.has_type("virtio_device"); }
		};

	public:

		Config(Heap & heap) : _heap(heap) {
			_mmio_alloc.add_range(VIRTIO_MMIO_START, VIRTIO_MMIO_SIZE); }

		bool initrd() const { return _initrd_name.valid(); }

		const char * kernel_name() const { return _kernel_name.string(); }
		const char * initrd_name() const { return _initrd_name.string(); }
		const char * cpu_type()    const { return _cpu_type.string();    }
		const char * bootargs()    const { return _bootargs.string();    }

		size_t    ram_size()    const { return _ram_size;    }
		unsigned  cpu_count()   const { return _cpu_count;   }
		unsigned  gic_version() const { return _gic_version; }

		template <typename FN>
		void for_each_virtio_device(FN const & fn) const {
			_model.for_each(fn); }

		void update(Xml_node);
};

#endif /* _SRC__SERVER__VMM__CONFIG_H_ */
