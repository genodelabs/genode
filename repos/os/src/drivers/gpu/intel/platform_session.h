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

#include <region_map/client.h>
#include <rm_session/connection.h>
#include <platform_session/platform_session.h>
#include <types.h>

struct Irq_ack_handler
{
	virtual ~Irq_ack_handler() {}
	virtual void ack_irq() = 0;
};

struct Gpu_reset_handler
{
	virtual ~Gpu_reset_handler() {}
	virtual void reset() = 0;
};

namespace Platform {
	using namespace Genode;

	class Device_component;
	class Session_component;
	class Io_mem_session_component;
	class Irq_session_component;
	class Root;
}

using Range = Platform::Device_interface::Range;

struct Main;

class Platform::Irq_session_component : public Rpc_object<Irq_session>
{
	private:

		Irq_ack_handler         & _ack_handler;
		Signal_context_capability _sigh { };

	public:

		Irq_session_component(Irq_ack_handler & ack_handler)
		: _ack_handler(ack_handler) { }

		void ack_irq() override { _ack_handler.ack_irq(); }

		void sigh(Signal_context_capability sigh) override {
			_sigh = sigh; }

		bool handle_irq()
		{
			if (!_sigh.valid()) return false;
			Signal_transmitter(_sigh).submit();
			return true;
		}

		Info info() override {
			return Info { Info::INVALID, 0, 0 }; }
};


class Platform::Io_mem_session_component : public Rpc_object<Io_mem_session>
{
	private:

		Io_mem_dataspace_capability _ds_cap;

	public:

		Io_mem_session_component(Dataspace_capability cap)
		: _ds_cap(static_cap_cast<Io_mem_dataspace>(cap)) { }

		Io_mem_dataspace_capability dataspace() override
		{
			return _ds_cap;
		}
};


class Platform::Device_component : public Rpc_object<Device_interface,
                                                     Device_component>
{
	private:

		Env                    & _env;
		Io_mem_session_component _gttmmadr_io;
		Range                    _gttmmadr_range;
		Io_mem_session_component _gmadr_io;
		Range                    _gmadr_range;
		Irq_session_component    _irq;

	public:

		Device_component(Env                & env,
		                 Irq_ack_handler    & ack_handler,
		                 Dataspace_capability gttmmadr_ds_cap,
		                 Range                gttmmadr_range,
		                 Dataspace_capability gmadr_ds_cap,
		                 Range                gmadr_range)
		:
		  _env(env),
		  _gttmmadr_io(gttmmadr_ds_cap),
		  _gttmmadr_range(gttmmadr_range),
		  _gmadr_io(gmadr_ds_cap),
		  _gmadr_range(gmadr_range),
		  _irq(ack_handler)
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

		Irq_session_capability irq(unsigned)
		{
			return _irq.cap();
		}

		Io_mem_session_capability io_mem(unsigned idx, Range & range)
		{
			range.start = 0;

			if (idx == 0) {
				range.size = _gttmmadr_range.size;
				return _gttmmadr_io.cap();
			}

			if (idx == 1) {
				range.size = _gmadr_range.size;
				return _gmadr_io.cap();
			}

			return Io_mem_session_capability();
		}

		Io_port_session_capability io_port_range(unsigned /* id */)
		{
			Genode::error(__func__, " is not supported");
			return Io_port_session_capability();
		}

		bool handle_irq() { return _irq.handle_irq(); }
};


class Platform::Session_component : public Rpc_object<Session>
{
	private:

		Env                & _env;
		Connection         & _platform;
		Gpu_reset_handler  & _reset_handler;
		Heap                 _heap { _env.ram(), _env.rm() };
		Device_component     _device_component;
		bool                 _acquired { false };

		/*
		 * track DMA memory allocations so we can free them at session
		 * destruction
		 */
		struct Buffer : Dma_buffer, Registry<Buffer>::Element
		{
			Buffer(Registry<Buffer> & registry,
			       Connection       & platform,
			       size_t             size,
			       Cache              cache)
			:
				Dma_buffer(platform, size, cache),
				Registry<Buffer>::Element(registry, *this) {}
		};
		Registry<Buffer> _dma_registry { };

	public:

		using Device_capability = Capability<Platform::Device_interface>;

		Session_component(Env                & env,
		                  Connection         & platform,
		                  Irq_ack_handler    & ack_handler,
		                  Gpu_reset_handler  & reset_handler,
		                  Dataspace_capability gttmmadr_ds_cap,
		                  Range                gttmmadr_range,
		                  Dataspace_capability gmadr_ds_cap,
		                  Range                gmadr_range)
		:
		  _env(env),
		  _platform(platform),
		  _reset_handler(reset_handler),
		  _device_component(env, ack_handler, gttmmadr_ds_cap, gttmmadr_range,
		                    gmadr_ds_cap, gmadr_range)
		{
			_env.ep().rpc_ep().manage(&_device_component);
		}

		~Session_component()
		{
			_env.ep().rpc_ep().dissolve(&_device_component);

			/* clear ggtt */
			_reset_handler.reset();

			/* free DMA allocations */
			_dma_registry.for_each([&](Buffer & dma) {
				destroy(_heap, &dma);
			});
		}

		Device_capability acquire_single_device() override
		{
			if (_acquired)
				return Device_capability();

			_acquired = true;
			return _device_component.cap();
		}

		void release_device(Device_capability) override
		{
			_acquired = false;
		}

		Device_capability acquire_device(Device_name const & /* string */) override
		{
			return acquire_single_device();
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache) override
		{
			Buffer & db = *(new (_heap)
				Buffer(_dma_registry, _platform, size, cache));
			return static_cap_cast<Ram_dataspace>(db.cap());
		}

		void free_dma_buffer(Ram_dataspace_capability cap) override
		{
			if (!cap.valid()) return;

			_dma_registry.for_each([&](Buffer & db) {
				if ((db.cap() == cap) == false) return;
				destroy(_heap, &db);
			});
		}

		addr_t dma_addr(Ram_dataspace_capability cap) override
		{
			addr_t ret = 0UL;
			_dma_registry.for_each([&](Buffer & db) {
				if ((db.cap() == cap) == false) return;
				ret = db.dma_addr();
			});
			return ret;
		}

		Rom_session_capability devices_rom() override {
			return _platform.devices_rom(); }

		bool handle_irq() { return _device_component.handle_irq(); }
};


class Platform::Root : public Root_component<Session_component, Genode::Single_client>
{
	private:

		Env               & _env;
		Connection        & _platform;
		Irq_ack_handler   & _ack_handler;
		Gpu_reset_handler & _reset_handler;
		Region_map_client   _gttmmadr_rm;
		Range               _gttmmadr_range;
		Region_map_client   _gmadr_rm;
		Range               _gmadr_range;

		Constructible<Session_component> _session { };

	public:

		Root(Env                & env,
		     Allocator          & md_alloc,
		     Connection         & platform,
		     Irq_ack_handler    & ack_handler,
		     Gpu_reset_handler  & reset_handler,
		     Igd::Mmio          & mmio,
		     Device::Mmio       & gmadr,
		     Rm_connection      & rm)
		:
			Root_component<Session_component,
			               Genode::Single_client>(&env.ep().rpc_ep(), &md_alloc),
			_env(env),
			_platform(platform),
			_ack_handler(ack_handler),
			_reset_handler(reset_handler),
			_gttmmadr_rm(rm.create(mmio.size())),
			_gttmmadr_range{1<<30, mmio.size()},
			_gmadr_rm(rm.create(Igd::APERTURE_RESERVED)),
			_gmadr_range{1<<29, gmadr.size()}
		{
			using namespace Igd;

			/********************************
			 ** Prepare managed dataspaces **
			 ********************************/

			/* GTT starts at half of the mmio memory */
			size_t const gttm_half_size = mmio.size() / 2;
			off_t  const gtt_offset     = gttm_half_size;

			if (gttm_half_size < GTT_RESERVED) {
				Genode::error("GTTM size too small");
				return;
			}

			/* attach actual iomem + reserved */
			_gttmmadr_rm.attach_at(mmio.cap(), 0, gtt_offset);

			/* attach beginning of GTT */
			_gttmmadr_rm.attach_at(mmio.cap(), gtt_offset,
			                       GTT_RESERVED, gtt_offset);

			/* attach the rest of the GTT as dummy RAM */
			Genode::Ram_dataspace_capability dummmy_gtt_ds {
				_env.ram().alloc(PAGE_SIZE) };
			size_t remainder = gttm_half_size - GTT_RESERVED;
			for (off_t offset = gtt_offset + GTT_RESERVED;
			     remainder > 0;
			     offset += PAGE_SIZE, remainder -= PAGE_SIZE) {
				rm.retry_with_upgrade(Genode::Ram_quota{PAGE_SIZE},
				                       Genode::Cap_quota{8}, [&]() {
					_gttmmadr_rm.attach_at(dummmy_gtt_ds, offset, PAGE_SIZE); });
			}

			_gmadr_rm.attach_at(gmadr.cap(), 0, APERTURE_RESERVED);

			env.parent().announce(env.ep().manage(*this));
		}

		Session_component *_create_session(char const * /* args */) override
		{
			_session.construct(_env, _platform, _ack_handler, _reset_handler,
			                   _gttmmadr_rm.dataspace(), _gttmmadr_range,
			                   _gmadr_rm.dataspace(), _gmadr_range);

			return &*_session;
		}

		void _upgrade_session(Session_component *, const char *args) override
		{
			if (!_session.constructed()) return;
			_platform.upgrade({ ram_quota_from_args(args),
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
