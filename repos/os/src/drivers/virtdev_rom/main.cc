/*
 * \brief  Virt Qemu device config generator for ARM platform driver
 * \author Piotr Tworek
 * \date   2020-07-01
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/attached_mmio.h>
#include <rom_session/rom_session.h>
#include <root/component.h>
#include <util/xml_generator.h>

namespace Virtdev_rom {
	using namespace Genode;
	class Session_component;
	class Root;
	struct Main;
};


class Virtdev_rom::Session_component : public Rpc_object<Rom_session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &) = delete;
		Session_component &operator = (Session_component const &) = delete;

		Rom_dataspace_capability const _rom_cap;

	public:

		Session_component(Rom_dataspace_capability cap) : _rom_cap(cap)
		{ }

		Rom_dataspace_capability dataspace() override { return _rom_cap; }

		void sigh(Signal_context_capability) override { }
};


class Virtdev_rom::Root : public Root_component<Session_component>
{
	private:

		/*
		 * Noncopyable
		 */
		Root(Root const &) = delete;
		Root &operator = (Root const &) = delete;

		Ram_dataspace_capability  _ds;

		Session_component *_create_session(const char *) override
		{
			auto cap = static_cap_cast<Dataspace>(_ds);
			return new (md_alloc()) Session_component(
				static_cap_cast<Rom_dataspace>(cap));
		}

	public:

		Root(Env &env, Allocator &md_alloc, Ram_dataspace_capability &cap)
		: Root_component<Session_component>(env.ep(), md_alloc), _ds(cap)
		{ }
};


struct Virtdev_rom::Main
{
	enum {
		/* Taken from include/hw/arm/virt.h in Qemu source tree. */
		NUM_VIRTIO_TRANSPORTS = 32,
		/* Taken from hw/arm/virt.c in Qemu source tree. */
		BASE_ADDRESS      = 0x0A000000,
		DEVICE_SIZE       = 0x200,
		IRQ_BASE          = 48,
		VIRTIO_MMIO_MAGIC = 0x74726976,
	};

	enum { MAX_ROM_SIZE = 4096, DEVICE_NAME_LEN = 64 };

	Env                      &_env;
	Ram_dataspace_capability  _ds   { _env.ram().alloc(MAX_ROM_SIZE) };
	Sliced_heap               _heap { _env.ram(), _env.rm() };
	Virtdev_rom::Root         _root { _env, _heap, _ds };

	struct Device : public Attached_mmio
	{
		struct Magic : Register<0x000, 32> { };
		struct Id    : Register<0x008, 32> {
			enum Value {
				INVALID = 0,
				NIC     = 1,
				BLOCK   = 2,
				CONSOLE = 3,
				RNG     = 4,
				GPU     = 16,
				INPUT   = 18,
				MAX_VAL = INPUT,
			};
		};

		Device(Env &env, addr_t base, size_t size)
		: Attached_mmio(env, base, size, false) { }
	};

	static char const *_name_for_id(unsigned id)
	{
		switch (id) {
			case Device::Id::NIC     : return "nic";
			case Device::Id::BLOCK   : return "block";
			case Device::Id::CONSOLE : return "console";
			case Device::Id::RNG     : return "rng";
			case Device::Id::GPU     : return "gpu";
			case Device::Id::INPUT   : return "input";
			default: {
				warning("Unhandled VirtIO device ID: ", Hex(id));
				return "virtio";
			}
		}
	}

	void _probe_devices()
	{
		Attached_dataspace ds(_env.rm(), _ds);
		Attached_rom_dataspace config { _env, "config" };

		Xml_generator xml(ds.local_addr<char>(), ds.size(), "config", [&] ()
		{
			uint8_t device_type_idx[Device::Id::MAX_VAL] = { 0 };

			for (size_t idx = 0; idx < NUM_VIRTIO_TRANSPORTS; ++idx) {
				addr_t addr = BASE_ADDRESS + idx * DEVICE_SIZE;
				Device device { _env, BASE_ADDRESS + idx * DEVICE_SIZE, DEVICE_SIZE };

				if (device.read<Device::Magic>() != VIRTIO_MMIO_MAGIC) {
					warning("Found non VirrtIO MMIO device @ ", addr);
					continue;
				}

				auto id = device.read<Device::Id>();
				if (id == Device::Id::INVALID)
					continue;

				xml.node("device", [&] ()
				{
					static char name[DEVICE_NAME_LEN];
					snprintf(name, sizeof(name), "%s%u", _name_for_id(id), device_type_idx[id - 1]++);
					xml.attribute("name", name);
					xml.node("io_mem", [&] () {
						xml.attribute("address", addr);
						xml.attribute("size", DEVICE_SIZE);
					});
					xml.node("irq", [&] () {
						xml.attribute("number", IRQ_BASE + idx);
					});
					xml.node("property", [&] () {
						xml.attribute("name", "type");
						xml.attribute("value", _name_for_id(id));
					});
				});
			}

			config.xml().with_raw_content([&] (char const *txt, size_t sz) {
				xml.append(txt, sz);
			});
		});
	}

	Main(Env &env) : _env(env)
	{
		_probe_devices();
		env.parent().announce(_env.ep().manage(_root));
	}

	~Main() { _env.ram().free(_ds); }
};


void Component::construct(Genode::Env &env)
{
	static Virtdev_rom::Main main(env);
}
