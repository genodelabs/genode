/*
 * \brief  Platform service implementation
 * \author Sebastian Sumpf
 * \date   2021-07-16
 */

/*
 * Copyright (C) 2021-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_SESSION_H_
#define _PLATFORM_SESSION_H_

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

struct Hw_ready_state
{
	virtual ~Hw_ready_state() {}
	virtual bool mmio_ready() = 0;
};

namespace Platform {
	using namespace Genode;

	class Device_component;
	class Session_component;
	class Io_mem_session_component;
	class Irq_session_component;
	class Resources;
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

		Device_component(Env                  & env,
		                 Irq_ack_handler      & ack_handler,
		                 Dataspace_capability   gttmmadr_ds_cap,
		                 Range                  gttmmadr_range,
		                 Dataspace_capability   gmadr_ds_cap,
		                 Range                  gmadr_range)
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
		Hw_ready_state     & _hw_ready;
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
		                  Hw_ready_state     & hw_ready,
		                  Dataspace_capability gttmmadr_ds_cap,
		                  Range                gttmmadr_range,
		                  Dataspace_capability gmadr_ds_cap,
		                  Range                gmadr_range)
		:
		  _env(env),
		  _platform(platform),
		  _hw_ready(hw_ready),
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
			if (_acquired || !_hw_ready.mmio_ready())
				return { };

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


class Platform::Resources : Noncopyable, public Hw_ready_state
{
	private:

		Env                                       & _env;
		Signal_context_capability const             _irq_cap;

		Platform::Connection                        _platform  { _env           };
		Reconstructible<Platform::Device>           _device    { _platform      };
		Reconstructible<Platform::Device::Irq>      _irq       { *_device       };
		Reconstructible<Igd::Mmio>                  _mmio      { *_device, _env };
		Reconstructible<Platform::Device::Mmio<0> > _gmadr     { *_device, Platform::Device::Mmio<0>::Index(1) };
		Reconstructible<Attached_dataspace>         _gmadr_mem { _env.rm(), _gmadr->cap() };

		uint64_t const      _aperture_reserved;

		Region_map_client   _rm_gttmm;
		Region_map_client   _rm_gmadr;
		Range               _range_gttmm;
		Range               _range_gmadr;

		void _reinit()
		{
			with_mmio_gmadr([&](auto &mmio, auto &gmadr) {

				using namespace Igd;

				/********************************
				 ** Prepare managed dataspaces **
				 ********************************/

				/* GTT starts at half of the mmio memory */
				size_t const gttm_half_size = mmio.size() / 2;
				off_t  const gtt_offset     = gttm_half_size;

				if (gttm_half_size < gtt_reserved()) {
					Genode::error("GTTM size too small");
					return;
				}

				/* attach actual iomem + reserved */
				_rm_gttmm.detach(0ul);
				_rm_gttmm.attach_at(mmio.cap(), 0ul, gtt_offset);

				/* attach beginning of GTT */
				_rm_gttmm.detach(gtt_offset);
				_rm_gttmm.attach_at(mmio.cap(), gtt_offset,
				                    gtt_reserved(), gtt_offset);

				_rm_gmadr.detach(0ul);
				_rm_gmadr.attach_at(gmadr.cap(), 0ul, aperture_reserved());
			}, []() {
				error("reinit failed");
			});
		}

		Number_of_bytes _sanitized_aperture_size(Number_of_bytes memory) const
		{
			/*
			 * Ranges of global GTT (ggtt) are handed in page granularity (4k)
			 * to the platform client (intel display driver).
			 * 512 page table entries a 4k fit into one page, which adds up to
			 * 2M virtual address space.
			 */
			auto constexpr shift_2mb = 21ull;
			auto constexpr MIN_MEMORY_FOR_MULTIPLEXER = 4ull << shift_2mb;

			/* align requests to 2M and enforce 2M as minimum */
			memory = memory & _align_mask(shift_2mb);
			if (memory < (1ull << shift_2mb))
				memory = 1ull << shift_2mb;

			if (_gmadr->size() >= MIN_MEMORY_FOR_MULTIPLEXER) {
				if (memory > _gmadr->size() - MIN_MEMORY_FOR_MULTIPLEXER)
					memory = _gmadr->size() - MIN_MEMORY_FOR_MULTIPLEXER;
			} else {
				/* paranoia case, should never trigger */
				memory = _gmadr->size() / 2;
				error("aperture smaller than ", MIN_MEMORY_FOR_MULTIPLEXER,
				      " will not work properly");
			}

			log("Maximum aperture size ", Number_of_bytes(_gmadr->size()));
			log(" - available framebuffer memory for display driver: ", memory);

			return memory;
		}

	public:

		Resources(Env &env, Rm_connection &rm, Signal_context_capability irq,
		          Number_of_bytes const &aperture)
		:
			_env(env),
			_irq_cap(irq),
			_aperture_reserved(_sanitized_aperture_size(aperture)),
			_rm_gttmm(rm.create(_mmio->size())),
			_rm_gmadr(rm.create(aperture_reserved())),
			_range_gttmm(1ul << 30, _mmio->size()),
			_range_gmadr(1ul << 29, aperture_reserved())
		{
			_irq->sigh(_irq_cap);

			/* GTT starts at half of the mmio memory */
			size_t const gttm_half_size = _mmio->size() / 2;
			off_t  const gtt_offset     = gttm_half_size;

			if (gttm_half_size < gtt_reserved()) {
				Genode::error("GTTM size too small");
				return;
			}

			_reinit();

			/* attach the rest of the GTT as dummy RAM */
			auto const dummmy_gtt_ds = _env.ram().alloc(Igd::PAGE_SIZE);
			auto       remainder     = gttm_half_size - gtt_reserved();

			for (off_t offset = gtt_offset + gtt_reserved();
			     remainder > 0;
			     offset += Igd::PAGE_SIZE, remainder -= Igd::PAGE_SIZE) {

				rm.retry_with_upgrade({Igd::PAGE_SIZE}, Cap_quota{8}, [&]() {
					_rm_gttmm.attach_at(dummmy_gtt_ds, offset, Igd::PAGE_SIZE); });
			}
		}

		void with_mmio_gmadr(auto const &fn, auto const &fn_error)
		{
			if (!_mmio.constructed() || !_gmadr.constructed()) {
				fn_error();
				return;
			}

			fn(*_mmio, *_gmadr);
		}

		void with_gmadr(auto const offset, auto const &fn, auto const &fn_error)
		{
			if (!_gmadr.constructed() || !_gmadr_mem.constructed()) {
				fn_error();
				return;
			}

			fn({_gmadr_mem->local_addr<char>() + offset, _gmadr->size() - offset });
		}

		void with_irq(auto const &fn, auto const &fn_error)
		{
			if (_irq.constructed())
				fn(*_irq);
			else
				fn_error();
		}

		void with_mmio(auto const &fn, auto const &fn_error)
		{
			if (_mmio.constructed())
				fn(*_mmio);
			else
				fn_error();
		}

		void with_gttm_gmadr(auto const &fn)
		{
			fn(_platform, _rm_gttmm, _range_gttmm, _rm_gmadr, _range_gmadr);
		}

		void with_platform(auto const &fn) { fn(_platform); }

		void acquire_device()
		{
			_device.construct(_platform);

			_irq.construct(*_device);
			_irq->sigh(_irq_cap);

			_mmio.construct(*_device, _env);
			_gmadr.construct(*_device, Platform::Device::Mmio<0>::Index(1));
			_gmadr_mem.construct(_env.rm(), _gmadr->cap());

			_reinit();
		}

		void release_device()
		{
			_gmadr_mem.destruct();
			_gmadr.destruct();
			_mmio.destruct();
			_irq.destruct();
			_device.destruct();
		}

		/*
		 * Reserved aperture for platform service
		 */
		uint64_t aperture_reserved() const { return _aperture_reserved; }

		uint64_t gtt_reserved() const
		{
			/* reserved GTT for platform service, GTT entry is 8 byte */
			return (aperture_reserved() / Igd::PAGE_SIZE) * 8;
		}

		bool mmio_ready() override { return _device.constructed(); }
};


class Platform::Root : public Root_component<Session_component, Genode::Single_client>
{
	private:

		Env               & _env;
		Resources         & _resources;
		Irq_ack_handler   & _ack_handler;
		Gpu_reset_handler & _reset_handler;

		Constructible<Session_component> _session { };

	public:

		Root(Env                & env,
		     Allocator          & md_alloc,
		     Resources          & resources,
		     Irq_ack_handler    & ack_handler,
		     Gpu_reset_handler  & reset_handler)
		:
			Root_component<Session_component,
			               Genode::Single_client>(&env.ep().rpc_ep(), &md_alloc),
			_env(env),
			_resources(resources),
			_ack_handler(ack_handler),
			_reset_handler(reset_handler)
		{
			env.parent().announce(env.ep().manage(*this));
		}

		Session_component *_create_session(char const * /* args */) override
		{
			_resources.with_gttm_gmadr([&](auto &platform,
			                               auto &rm_gttmm, auto &range_gttmm,
			                               auto &rm_gmadr, auto &range_gmadr) {
				_session.construct(_env, platform, _ack_handler,
				                   _reset_handler, _resources,
				                   rm_gttmm.dataspace(), range_gttmm,
				                   rm_gmadr.dataspace(), range_gmadr);
			});

			if (!_session.constructed())
				throw Service_denied();

			return &*_session;
		}

		void _upgrade_session(Session_component *, const char *args) override
		{
			if (!_session.constructed()) return;

			_resources.with_platform([&](auto &platform) {
				platform.upgrade({ ram_quota_from_args(args),
				                   cap_quota_from_args(args) });
			});
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

#endif /* _PLATFORM_SESSION_H_ */
