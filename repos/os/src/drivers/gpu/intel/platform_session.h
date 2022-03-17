/*
 * \brief  Platform service implementation
 * \author Sebastian Sumpf
 * \date   2021-07-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <legacy/x86/platform_session/platform_session.h>

namespace Platform {
class Device_component;
class Session_component;
class Io_mem_session_component;
class Io_mem_session_component_gmadr;
class Irq_session_component;
class Root;
}


class Platform::Irq_session_component : public Rpc_object<Irq_session>
{
	private:

		Igd::Resources           &_resources;
		Signal_context_capability _sigh { };

	public:

		Irq_session_component(Igd::Resources &resources)
		:
		  _resources(resources)
		{ }

		void ack_irq() override
		{
			_resources.ack_irq();
		}

		void sigh(Signal_context_capability sigh) override
		{
			_sigh = sigh;
		}

		bool handle_irq()
		{
			if (!_sigh.valid()) return false;
			Signal_transmitter(_sigh).submit();

			return true;
		}

		Info info() override
		{
			return Info { Info::INVALID, 0, 0 };
		}
};


class Platform::Io_mem_session_component : public Rpc_object<Io_mem_session>
{
	private:

		Igd::Resources &_resources;

	public:

		Io_mem_session_component(Igd::Resources &resources)
		:
		  _resources(resources) { }

		Io_mem_dataspace_capability dataspace() override
		{
			return _resources.gttmmadr_platform_ds();
		}
};


class Platform::Io_mem_session_component_gmadr : public Rpc_object<Io_mem_session>
{
	private:

		Igd::Resources &_resources;

	public:

		Io_mem_session_component_gmadr(Igd::Resources &resources)
		:
		  _resources(resources) { }

		Io_mem_dataspace_capability dataspace() override
		{
			return _resources.gmadr_platform_ds();
		}
};


class Platform::Device_component : public Rpc_object<Device>
{
	private:

		Env                 &_env;
		Igd::Resources      &_resources;
		Device_client       &_device { _resources.gpu_client() };

		Io_mem_session_component       _gttmmadr_io { _resources };
		Io_mem_session_component_gmadr _gmadr_io    { _resources };
		Irq_session_component          _irq         { _resources };

	public:

		Device_component(Env &env, Igd::Resources &resources)
		:
		  _env(env), _resources(resources)
		{
			_env.ep().rpc_ep().manage(&_gttmmadr_io);
			_env.ep().rpc_ep().manage(&_gmadr_io);
			_env.ep().rpc_ep().manage(&_irq);
		}

		~Device_component()
		{
			_env.ep().rpc_ep().dissolve(&_gttmmadr_io);
			_env.ep().rpc_ep().dissolve(&_gmadr_io);
			_env.ep().rpc_ep().dissolve(&_irq);
		}

		Irq_session_capability irq(uint8_t) override
		{
			return _irq.cap();
		}

		Io_mem_session_capability io_mem(uint8_t v_id, Cache /* caching */,
		                                 addr_t /* offset */,
		                                 size_t /* size */) override
		{
			if (v_id == 0)
				return _gttmmadr_io.cap();

			if (v_id == 1)
				return _gmadr_io.cap();

			return Io_mem_session_capability();
		}

		void bus_address(unsigned char *bus, unsigned char *dev,
		                 unsigned char *fn) override
		{
			_device.bus_address(bus, dev, fn);
		}

		unsigned short vendor_id() override
		{ 
			return _device.vendor_id();
		}

		unsigned short device_id() override
		{
			return _device.device_id();
		}

		unsigned class_code() override
		{
			return _device.class_code();
		}

		Resource resource(int resource_id) override
		{
			/* bar 0 is io mem/gtt */
			if (resource_id == 0)
				return Resource(_resources.gttmmadr_base(), _resources.gttmmadr_size());

			/* bar 2 is GMADR (i.e., aperture) */
			if(resource_id == 2)
				return Resource(_resources.gmadr_base(), _resources.gmadr_platform_size());

			return Resource();
		}

		unsigned config_read(unsigned char address, Access_size size) override
		{
			return _device.config_read(address, size);
		}

		void config_write(unsigned char /* address */, unsigned /* value */,
		                  Access_size/*  size */) override
		{
		}

		Io_port_session_capability io_port(uint8_t /* id */) override
		{
			Genode::error(__func__, " is not supported");
			return Io_port_session_capability();
		}

		bool handle_irq() { return _irq.handle_irq(); }
};


class Platform::Session_component : public Rpc_object<Session>
{
	private:

		Env              &_env;
		Device_component  _device_component;
		Connection       &_platform;
		Device_capability _bridge;
		Igd::Resources   &_resources;

		struct Dma_cap
		{
			Ram_dataspace_capability cap;

			Dma_cap(Ram_dataspace_capability cap)
			  : cap(cap) { }

			virtual ~Dma_cap() { }
		};

		/*
		 * track DMA memory allocations so we can free them at session
		 * destruction
		 */
		Registry<Registered<Dma_cap>> _dma_registry { };

	public:

		Session_component(Env &env, Igd::Resources &resources)
		:
		  _env(env),
		  _device_component(env, resources),
		  _platform(resources.platform()),
		  _bridge(resources.host_bridge_cap()),
		  _resources(resources)
		{
			_env.ep().rpc_ep().manage(&_device_component);
		}

		~Session_component()
		{
			_env.ep().rpc_ep().dissolve(&_device_component);

			/* clear ggtt */
			_resources.gtt_platform_reset();

			/* free DMA allocations */
			_dma_registry.for_each([&](Dma_cap &dma) {
				_platform.free_dma_buffer(dma.cap);
				destroy(&_resources.heap(), &dma);
			});
		}

		Device_capability first_device(unsigned device_class, unsigned) override
		{
			if (device_class == _resources.isa_bridge_class())
				return _resources.isa_bridge_cap();

			return _bridge;
		}

		Device_capability next_device(Device_capability prev_device,
		                              unsigned, unsigned) override
		{
			if (!prev_device.valid())
				return _bridge;

			if (prev_device == _bridge)
				return _device_component.cap();

			if (prev_device == _device_component.cap())
				return _resources.isa_bridge_cap();

			return Device_capability();
		}

		void release_device(Device_capability) override
		{
			return;
		}

		Device_capability device(Device_name const & /* string */) override
		{
			Genode::error(__func__, " is not supported");
			return Device_capability();
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache) override
		{
			Ram_dataspace_capability cap = _platform.alloc_dma_buffer(size, cache);
			new (&_resources.heap()) Registered<Dma_cap>(_dma_registry, cap);
			return cap;
		}

		void free_dma_buffer(Ram_dataspace_capability cap) override
		{
			if (!cap.valid()) return;

			_dma_registry.for_each([&](Dma_cap &dma) {
				if ((dma.cap == cap) == false) return;
				_platform.free_dma_buffer(cap);
				destroy(&_resources.heap(), &dma);
			});
		}

		addr_t dma_addr(Ram_dataspace_capability cap) override
		{
			return _platform.dma_addr(cap);
		}

		bool handle_irq() { return _device_component.handle_irq(); }
};


class Platform::Root : public Root_component<Session_component, Genode::Single_client>
{
	private:

		Env                             &_env;
		Constructible<Session_component> _session { };
		Igd::Resources                  &_resources;

	public:

		Root(Env &env, Allocator &md_alloc, Igd::Resources &resources)
		:
		  Root_component<Session_component, Genode::Single_client>(&env.ep().rpc_ep(), &md_alloc),
		  _env(env), _resources(resources)
		{
			env.parent().announce(env.ep().manage(*this));
		}

		Session_component *_create_session(char const * /* args */) override
		{
			_session.construct(_env, _resources);

			return &*_session;
		}

		void _upgrade_session(Session_component *, const char *args) override
		{
			if (!_session.constructed()) return;
			_resources.platform().upgrade({ ram_quota_from_args(args),
			                                cap_quota_from_args(args) });
		}

		void _destroy_session(Session_component *) override
		{
			if (_session.constructed())
				_session.destruct();
		}

		bool handle_irq()
		{
			if (_session.constructed())
				return _session->handle_irq();

			return false;
		}
};
