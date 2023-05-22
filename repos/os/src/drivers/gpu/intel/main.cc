/*
 * \brief  Broadwell multiplexer
 * \author Josef Soentgen
 * \data   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/ram_allocator.h>
#include <base/registry.h>
#include <base/rpc_server.h>
#include <base/session_object.h>
#include <dataspace/client.h>
#include <gpu/info_intel.h>
#include <platform_session/dma_buffer.h>
#include <platform_session/device.h>
#include <root/component.h>
#include <timer_session/connection.h>
#include <util/dictionary.h>
#include <util/fifo.h>
#include <util/mmio.h>
#include <util/retry.h>
#include <util/xml_node.h>

/* local includes */
#include <mmio.h>
#include <ppgtt.h>
#include <ppgtt_allocator.h>
#include <ggtt.h>
#include <context.h>
#include <context_descriptor.h>
#include <platform_session.h>
#include <reset.h>
#include <ring_buffer.h>

using namespace Genode;

static constexpr bool DEBUG = false;

namespace Igd {

	struct Device_info;
	struct Device;
}


struct Igd::Device_info
{
	enum Platform { UNKNOWN, BROADWELL, SKYLAKE, KABYLAKE, WHISKEYLAKE, TIGERLAKE };
	enum Stepping { A0, B0, C0, D0, D1, E0, F0, G0 };

	uint16_t    id;
	uint8_t     generation;
	Platform    platform;
	uint64_t    features;
};



struct Igd::Device
{
	struct Unsupported_device    : Genode::Exception { };
	struct Out_of_caps           : Genode::Exception { };
	struct Out_of_ram            : Genode::Exception { };
	struct Could_not_map_vram    : Genode::Exception { };

	/* 200 ms */
	enum { WATCHDOG_TIMEOUT = 200*1000 };

	Env                    & _env;
	Allocator              & _md_alloc;
	Platform::Connection   & _platform;
	Rm_connection          & _rm;
	Igd::Mmio              & _mmio;
	Igd::Reset               _reset { _mmio };
	Platform::Device::Mmio & _gmadr;
	uint8_t                  _gmch_ctl;
	Timer::Connection        _timer { _env };

	/*********
	 ** PCI **
	 *********/

	struct Pci_backend_alloc : Utils::Backend_alloc, Ram_allocator
	{
		Env                  &_env;
		Platform::Connection &_pci;

		Pci_backend_alloc(Env &env, Platform::Connection &pci)
		: _env(env), _pci(pci) { }

		Ram_dataspace_capability alloc(size_t size) override
		{
			enum {
				UPGRADE_RAM      = 8 * PAGE_SIZE,
				UPGRADE_CAPS     = 2,
				UPGRADE_ATTEMPTS = ~0U
			};

			return retry<Genode::Out_of_ram>(
				[&] () {
					return retry<Genode::Out_of_caps>(
						[&] () { return _pci.Client::alloc_dma_buffer(size, UNCACHED); },
						[&] ()
						{
							if (_env.pd().avail_caps().value < UPGRADE_CAPS) {
								warning("alloc dma vram: out if caps");
								throw Gpu::Session::Out_of_caps();
							}

							_pci.upgrade_caps(UPGRADE_CAPS);
						},
						UPGRADE_ATTEMPTS);
				},
				[&] ()
				{
					if (_env.pd().avail_ram().value < size) {
						warning("alloc dma vram: out of ram");
						throw Gpu::Session::Out_of_ram();
					}
					_pci.upgrade_ram(size);
				},
				UPGRADE_ATTEMPTS);
		}

		void free(Ram_dataspace_capability cap) override
		{
			if (!cap.valid()) {
				error("could not free, capability invalid");
				return;
			}

			_pci.free_dma_buffer(cap);
		}

		addr_t dma_addr(Ram_dataspace_capability ds_cap) override
		{
			return _pci.dma_addr(ds_cap);
		}

		/**
		 * RAM allocator interface
		 */

		size_t dataspace_size(Ram_dataspace_capability) const override { return 0; }

		Alloc_result try_alloc(size_t size, Cache) override
		{
			return alloc(size);
		}

	} _pci_backend_alloc { _env, _platform };

	Device_info                      _info          { };
	Gpu::Info_intel::Revision        _revision      { };
	Gpu::Info_intel::Slice_mask      _slice_mask    { };
	Gpu::Info_intel::Subslice_mask   _subslice_mask { };
	Gpu::Info_intel::Eu_total        _eus           { };
	Gpu::Info_intel::Subslices       _subslices     { };
	Gpu::Info_intel::Topology        _topology      { };
	Gpu::Info_intel::Clock_frequency _clock_frequency { };


	bool _supported(Xml_node & supported,
	                uint16_t   dev_id,
	                uint8_t    rev_id)
	{
		bool found = false;

		supported.for_each_sub_node("device", [&] (Xml_node node) {
			if (found)
				return;

			uint16_t   const vendor     = node.attribute_value("vendor", 0U);
			uint16_t   const device     = node.attribute_value("device", 0U);
			uint8_t    const generation = node.attribute_value("generation", 0U);
			String<16> const platform   = node.attribute_value("platform", String<16>("unknown"));
			//String<64> const desc       = node.attribute_value("description", String<64>("unknown"));

			if (vendor != 0x8086 /* Intel */ || generation < 8)
				return;

			struct Igd::Device_info const info {
				.id                    = device,
				.generation            = generation,
				.platform              = platform_type(platform),
				.features              = 0,
			};

			if (info.platform == Igd::Device_info::Platform::UNKNOWN)
				return;

			/* set generation for device IO as early as possible */
			_mmio.generation(generation);

			if (info.id == dev_id) {
				_info = info;
				_revision.value        = rev_id;
				_clock_frequency.value = _mmio.clock_frequency();

				found = true;
				return;
			}
		});

		return found;
	}

	Igd::Device_info::Platform platform_type(String<16> const &platform) const
	{
		if (platform == "broadwell")
			return Igd::Device_info::Platform::BROADWELL;
		if (platform == "skylake")
			return Igd::Device_info::Platform::SKYLAKE;
		if (platform == "kabylake")
			return Igd::Device_info::Platform::KABYLAKE;
		if (platform == "whiskeylake")
			return Igd::Device_info::Platform::WHISKEYLAKE;
		if (platform == "tigerlake")
			return Igd::Device_info::Platform::TIGERLAKE;
		return Igd::Device_info::UNKNOWN;
	}

	/**********
	 ** GGTT **
	 **********/

	size_t _ggtt_size()
	{
		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1068
		 */
		struct MGGC_0_2_0_PCI : Genode::Register<16>
		{
			struct Graphics_mode_select               : Bitfield<8, 8> { };
			struct Gtt_graphics_memory_size           : Bitfield<6, 2> { };
			struct Versatile_acceleration_mode_enable : Bitfield<3, 1> { };
			struct Igd_vga_disable                    : Bitfield<2, 1> { };
			struct Ggc_lock                           : Bitfield<0, 1> { };
		};
		enum { PCI_GMCH_CTL = 0x50, };
		MGGC_0_2_0_PCI::access_t v = _gmch_ctl;

		{
			log("MGGC_0_2_0_PCI");
			log("  Graphics_mode_select:               ", Hex(MGGC_0_2_0_PCI::Graphics_mode_select::get(v)));
			log("  Gtt_graphics_memory_size:           ", Hex(MGGC_0_2_0_PCI::Gtt_graphics_memory_size::get(v)));
			log("  Versatile_acceleration_mode_enable: ", Hex(MGGC_0_2_0_PCI::Versatile_acceleration_mode_enable::get(v)));
			log("  Igd_vga_disable:                    ", Hex(MGGC_0_2_0_PCI::Igd_vga_disable::get(v)));
			log("  Ggc_lock:                           ", Hex(MGGC_0_2_0_PCI::Ggc_lock::get(v)));
		}

		return (1u << MGGC_0_2_0_PCI::Gtt_graphics_memory_size::get(v)) << 20;
	}

	addr_t _ggtt_base() const {
		return _mmio.base() + (_mmio.size() / 2); }

	Igd::Ggtt _ggtt { _platform, _mmio, _ggtt_base(),
	                  _ggtt_size(), _gmadr.size(), APERTURE_RESERVED };

	/************
	 ** MEMORY **
	 ************/

	struct Unaligned_size : Genode::Exception { };

	Ram_dataspace_capability _alloc_dataspace(size_t const size)
	{
		if (size & 0xfff) { throw Unaligned_size(); }

		Genode::Ram_dataspace_capability ds = _pci_backend_alloc.alloc(size);
		if (!ds.valid()) { throw Out_of_ram(); }
		return ds;
	}

	void free_dataspace(Ram_dataspace_capability const cap)
	{
		if (!cap.valid()) { return; }

		_pci_backend_alloc.free(cap);
	}

	struct Ggtt_mmio_mapping : Ggtt::Mapping
	{
		Region_map_client _rmc;

		Ggtt_mmio_mapping(Rm_connection      & rm,
		                  Dataspace_capability cap,
		                  Ggtt::Offset         offset,
		                  size_t               size)
		:
			_rmc(rm.create(size))
		{
			_rmc.attach_at(cap, 0, size, offset * PAGE_SIZE);
			Ggtt::Mapping::cap    = _rmc.dataspace();
			Ggtt::Mapping::offset = offset;
		}

		virtual ~Ggtt_mmio_mapping() { }
	};

	Genode::Registry<Genode::Registered<Ggtt_mmio_mapping>> _ggtt_mmio_mapping_registry { };

	Ggtt_mmio_mapping const &map_dataspace_ggtt(Genode::Allocator &alloc,
	                                            Genode::Ram_dataspace_capability cap,
	                                            Ggtt::Offset offset)
	{
		Genode::Dataspace_client client(cap);
		addr_t const phys_addr = _pci_backend_alloc.dma_addr(cap);
		size_t const size      = client.size();

		/*
		 * Create the mapping first and insert the entries afterwards
		 * so we do not have to rollback when the allocation failes.
		 */
		Genode::Registered<Ggtt_mmio_mapping> *mem = new (&alloc)
			Genode::Registered<Ggtt_mmio_mapping>(_ggtt_mmio_mapping_registry,
			                                      _rm, _gmadr.cap(), offset, size);
		for (size_t i = 0; i < size; i += PAGE_SIZE) {
			addr_t const pa = phys_addr + i;
			_ggtt.insert_pte(pa, offset + (i / PAGE_SIZE));
		}

		return *mem;
	}

	void unmap_dataspace_ggtt(Genode::Allocator &alloc, Genode::Dataspace_capability cap)
	{
		size_t const num = Genode::Dataspace_client(cap).size() / PAGE_SIZE;

		auto lookup_and_free = [&] (Ggtt_mmio_mapping &m) {
			if (!(m.cap == cap)) { return; }

			_ggtt.remove_pte_range(m.offset, num);
			Genode::destroy(&alloc, &m);
		};

		_ggtt_mmio_mapping_registry.for_each(lookup_and_free);
	}

	struct Invalid_ppgtt : Genode::Exception { };

	static addr_t _ppgtt_phys_addr(Igd::Ppgtt_allocator &alloc,
	                               Igd::Ppgtt const * const ppgtt)
	{
		void * const p = alloc.phys_addr(const_cast<Igd::Ppgtt*>(ppgtt));
		if (p == nullptr) { throw Invalid_ppgtt(); }
		return reinterpret_cast<addr_t>(p);
	}

	/************
	 ** ENGINE **
	 ************/

	struct Execlist : Genode::Noncopyable
	{
		Igd::Context_descriptor _elem0;
		Igd::Context_descriptor _elem1;

		Igd::Ring_buffer        _ring;
		bool                    _scheduled;

		Execlist(uint32_t const id, addr_t const lrca,
		         addr_t const ring, size_t const ring_size)
		:
			_elem0(id, lrca), _elem1(),
			_ring(ring, ring_size), _scheduled(false)
		{ }

		Igd::Context_descriptor elem0() const { return _elem0; }
		Igd::Context_descriptor elem1() const { return _elem1; }

		void schedule(int port) { _scheduled = port; }
		int scheduled() const { return _scheduled; }

		/*************************
		 ** Ring vram interface **
		 *************************/

		void               ring_reset() { _ring.reset(); }
		Ring_buffer::Index ring_tail() const { return _ring.tail(); }
		Ring_buffer::Index ring_head() const { return _ring.head(); }
		Ring_buffer::Index ring_append(Igd::Cmd_header cmd) { return _ring.append(cmd); }

		bool ring_avail(Ring_buffer::Index num) const { return _ring.avail(num); }
		Ring_buffer::Index ring_max()   const { return _ring.max(); }
		void ring_reset_and_fill_zero() { _ring.reset_and_fill_zero(); }
		void ring_update_head(Ring_buffer::Index head) { _ring.update_head(head); }
		void ring_reset_to_head(Ring_buffer::Index head) { _ring.reset_to_head(head); }

		void ring_flush(Ring_buffer::Index from, Ring_buffer::Index to)
		{
			_ring.flush(from, to);
		}

		void ring_dump(size_t limit, unsigned hw_tail, unsigned hw_head) const { _ring.dump(limit, hw_tail, hw_head); }

		/*********************
		 ** Debug interface **
		 *********************/

		void dump() { _elem0.dump(); }
	};

	struct Ggtt_map_memory
	{
		struct Dataspace_guard {
			Device                         &device;
			Ram_dataspace_capability const  ds;

			Dataspace_guard(Device &device, Ram_dataspace_capability ds)
			: device(device), ds(ds)
			{ }

			~Dataspace_guard()
			{
				if (ds.valid())
					device.free_dataspace(ds);
			}
		};

		struct Mapping_guard {
			Device              &device;
			Allocator           &alloc;
			Ggtt::Mapping const &map;

			Mapping_guard(Device &device, Ggtt_map_memory &gmm, Allocator &alloc)
			:
				device(device),
				alloc(alloc),
				map(device.map_dataspace_ggtt(alloc, gmm.ram_ds.ds, gmm.offset))
			{ }

			~Mapping_guard() { device.unmap_dataspace_ggtt(alloc, map.cap); }
		};

		Device             const &device;
		Allocator          const &alloc;
		Ggtt::Offset       const  offset;
		Ggtt::Offset       const  skip;
		Dataspace_guard    const  ram_ds;
		Mapping_guard      const  map;
		Attached_dataspace const  ram;

		addr_t vaddr() const
		{
			return reinterpret_cast<addr_t>(ram.local_addr<addr_t>())
			       + (skip * PAGE_SIZE);
		}

		addr_t gmaddr() const { /* graphics memory address */
			return (offset + skip) * PAGE_SIZE; }

		Ggtt_map_memory(Region_map &rm, Allocator &alloc, Device &device,
		                Ggtt::Offset const pages, Ggtt::Offset const skip_pages)
		:
			device(device),
			alloc(alloc),
			offset(device._ggtt.find_free(pages, true)),
			skip(skip_pages),
			ram_ds(device, device._alloc_dataspace(pages * PAGE_SIZE)),
			map(device, *this, alloc),
			ram(rm, map.map.cap)
		{ }
	};

	template <typename CONTEXT>
	struct Engine
	{
		static constexpr size_t CONTEXT_PAGES = CONTEXT::CONTEXT_PAGES;
		static constexpr size_t RING_PAGES    = CONTEXT::RING_PAGES;

		Ggtt_map_memory ctx;  /* context memory */
		Ggtt_map_memory ring; /* ring memory */

		Ppgtt_allocator  ppgtt_allocator;
		Ppgtt_scratch    ppgtt_scratch;
		Ppgtt           *ppgtt { nullptr };

		Genode::Constructible<CONTEXT>  context  { };
		Genode::Constructible<Execlist> execlist { };

		Engine(Igd::Device         &device,
		       uint32_t             id,
		       Allocator           &alloc)
		:
			ctx (device._env.rm(), alloc, device, CONTEXT::CONTEXT_PAGES, 1 /* omit GuC page */),
			ring(device._env.rm(), alloc, device, CONTEXT::RING_PAGES, 0),
			ppgtt_allocator(alloc, device._env.rm(), device._pci_backend_alloc),
			ppgtt_scratch(device._pci_backend_alloc)
		{
			/* PPGTT */
			device._populate_scratch(ppgtt_scratch);

			ppgtt = new (ppgtt_allocator) Igd::Ppgtt(&ppgtt_scratch.pdp);

			try {
				size_t const ring_size = RING_PAGES * PAGE_SIZE;

				/* get PML4 address */
				addr_t const ppgtt_phys_addr = _ppgtt_phys_addr(ppgtt_allocator,
				                                                ppgtt);
				addr_t const pml4 = ppgtt_phys_addr | 1;

				/* setup context */
				context.construct(ctx.vaddr(), ring.gmaddr(), ring_size, pml4, device.generation());

				/* setup execlist */
				execlist.construct(id, ctx.gmaddr(), ring.vaddr(), ring_size);
				execlist->ring_reset();
			} catch (...) {
				_destruct();
				throw;
			}
		}

		~Engine() { _destruct(); };

		void _destruct()
		{
			if (ppgtt)
				Genode::destroy(ppgtt_allocator, ppgtt);

			execlist.destruct();
			context.destruct();
		}

		size_t ring_size() const { return RING_PAGES * PAGE_SIZE; }

		addr_t hw_status_page() const { return ctx.gmaddr(); }


		private:

			/*
			 * Noncopyable
			 */
			Engine(Engine const &);
			Engine &operator = (Engine const &);
	};

	void _fill_page(Genode::Ram_dataspace_capability ds, addr_t v)
	{
		Attached_dataspace ram(_env.rm(), ds);
		uint64_t * const p = ram.local_addr<uint64_t>();

		for (size_t i = 0; i < Ppgtt_scratch::MAX_ENTRIES; i++) {
			p[i] = v;
		}
	}

	void _populate_scratch(Ppgtt_scratch &scratch)
	{
		_fill_page(scratch.pt.ds,  scratch.page.addr);
		_fill_page(scratch.pd.ds,  scratch.pt.addr);
		_fill_page(scratch.pdp.ds, scratch.pd.addr);
	}


	/**********
	 ** Vgpu **
	 **********/

	uint32_t _vgpu_avail { 0 };

	/* global hardware-status page */
	Constructible<Ggtt_map_memory>      _hw_status_ctx  { };
	Constructible<Hardware_status_page> _hw_status_page { };

	struct Vgpu : Genode::Fifo<Vgpu>::Element
	{
		enum {
			APERTURE_SIZE = 4096ul,
			MAX_FENCES    = 16,

			INFO_SIZE = 4096,
		};

		Device                          &_device;
		Signal_context_capability        _completion_sigh { };
		uint32_t                  const  _id;
		Engine<Rcs_context>              rcs;
		uint32_t                         active_fences    { 0 };
		uint64_t                         _current_seqno   { 0 };

		Genode::Attached_ram_dataspace  _info_dataspace;
		Gpu::Info_intel                &_info {
			*_info_dataspace.local_addr<Gpu::Info_intel>() };

		uint32_t _id_alloc()
		{
			static uint32_t id = 1;

			uint32_t const v = id++;
			return v << 8;
		}

		Vgpu(Device &device, Allocator &alloc,
		     Ram_allocator &ram, Region_map &rm)
		:
			_device(device),
			_id(_id_alloc()),
			rcs(_device, _id + Rcs_context::HW_ID, alloc),
			_info_dataspace(ram, rm, INFO_SIZE)
		{
			_device.vgpu_created();

			_info = Gpu::Info_intel(_device.id(), _device.features(),
			                        APERTURE_SIZE,
			                        _id, Gpu::Sequence_number { .value = 0 },
			                        _device._revision,
			                        _device._slice_mask,
			                        _device._subslice_mask,
			                        _device._eus,
			                        _device._subslices,
			                        _device._clock_frequency,
			                        _device._topology);
		}

		~Vgpu()
		{
			_device.vgpu_released();
		}

		Genode::Dataspace_capability info_dataspace() const
		{
			return _info_dataspace.cap();
		}

		uint32_t id() const { return _id; }

		void completion_sigh(Genode::Signal_context_capability sigh) {
			_completion_sigh = sigh; }

		Genode::Signal_context_capability completion_sigh() {
			return _completion_sigh; }

		uint64_t current_seqno() const { return _current_seqno; }

		uint64_t completed_seqno() const { return _info.last_completed.value; }

		void user_complete()
		{
			_info.last_completed =
				Gpu::Sequence_number { .value = _device.seqno() };
		}

		void mark_completed() {
			_info.last_completed = Gpu::Sequence_number { .value = current_seqno() };
		}

		bool setup_ring_vram(Gpu::addr_t const vram_addr)
		{
			_current_seqno++;

			unsigned generation = _device.generation().value;

			Execlist &el = *rcs.execlist;

			Ring_buffer::Index advance = 0;

			bool dc_flush_wa = _device.match(Device_info::Platform::KABYLAKE,
			                                 Device_info::Stepping::A0,
			                                 Device_info::Stepping::B0);

			size_t const need = 4 /* batchvram cmd */ + 6 /* prolog */
			                  + ((generation == 9) ? 6 : 0)
			                  + ((generation == 8) ? 20 : 22) /* epilog + w/a */
			                  + (dc_flush_wa ? 12 : 0);

			if (!el.ring_avail(need))
				el.ring_reset_and_fill_zero();

			/* save old tail */
			Ring_buffer::Index const tail = el.ring_tail();

			/*
			 * IHD-OS-BDW-Vol 7-11.15 p. 18 ff.
			 *
			 * Pipeline synchronization
			 */

			/*
			 * on GEN9: emit empty pipe control before VF_CACHE_INVALIDATE
			 * - Linux 5.13 gen8_emit_flush_rcs()
			 */
			if (generation == 9) {
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Pipe_control pc(CMD_NUM);
				cmd[0] = pc.value;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			if (dc_flush_wa) {
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Pipe_control pc(CMD_NUM);
				cmd[0] = pc.value;
				cmd[1] = Igd::Pipe_control::DC_FLUSH_ENABLE;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/* prolog */
			if (1)
			{
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Pipe_control pc(CMD_NUM);
				cmd[0] = pc.value;
				Genode::uint32_t tmp = 0;
				tmp |= Igd::Pipe_control::CS_STALL;
				tmp |= Igd::Pipe_control::TLB_INVALIDATE;
				tmp |= Igd::Pipe_control::INSTRUCTION_CACHE_INVALIDATE;
				tmp |= Igd::Pipe_control::TEXTURE_CACHE_INVALIDATE;
				tmp |= Igd::Pipe_control::VF_CACHE_INVALIDATE;
				tmp |= Igd::Pipe_control::CONST_CACHE_INVALIDATE;
				tmp |= Igd::Pipe_control::STATE_CACHE_INVALIDATE;
				tmp |= Igd::Pipe_control::QW_WRITE;
				tmp |= Igd::Pipe_control::STORE_DATA_INDEX;

				cmd[1] = tmp;
				/*
				 * PP-Hardware status page (base GLOBAL_GTT_IVB is disabled)
				 * offset (dword 52) because STORE_DATA_INDEX is enabled.
				 */
				cmd[2] = 0x34 * 4;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			if (dc_flush_wa) {
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Pipe_control pc(CMD_NUM);
				cmd[0] = pc.value;
				cmd[1] = Igd::Pipe_control::CS_STALL;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/*
			 * gen8_emit_bb_start_noarb, gen8 and render engine
			 *
			 * batch-vram commands
			 */
			if (generation == 8)
			{
				enum { CMD_NUM = 4 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Mi_batch_buffer_start mi;

				cmd[0] = Mi_arb_on_off(false).value;
				cmd[1] = mi.value;
				cmd[2] = vram_addr & 0xffffffff;
				cmd[3] = (vram_addr >> 32) & 0xffff;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/*
			 * gen8_emit_bb_start, gen9
			 *
			 * batch-vram commands
			 */
			if (generation >= 9)
			{
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Mi_batch_buffer_start mi;

				cmd[0] = Mi_arb_on_off(true).value;
				cmd[1] = mi.value;
				cmd[2] = vram_addr & 0xffffffff;
				cmd[3] = (vram_addr >> 32) & 0xffff;
				cmd[4] = Mi_arb_on_off(false).value;
				cmd[5] = Mi_noop().value;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/* epilog 1/3 - gen8_emit_fini_breadcrumb_rcs, gen8_emit_pipe_control */
			if (1)
			{
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Pipe_control pc(CMD_NUM);
				cmd[0] = pc.value;
				Genode::uint32_t tmp = 0;
				tmp |= Igd::Pipe_control::RENDER_TARGET_CACHE_FLUSH;
				tmp |= Igd::Pipe_control::DEPTH_CACHE_FLUSH;
				tmp |= Igd::Pipe_control::DC_FLUSH_ENABLE;
				cmd[1] = tmp;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/* epilog 2/3 - gen8_emit_fini_breadcrumb_rcs, gen8_emit_ggtt_write_rcs */
			if (1)
			{
				using HWS_DATA = Hardware_status_page::Sequence_number;
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Pipe_control pc(CMD_NUM);
				cmd[0] = pc.value;
				Genode::uint32_t tmp = 0;
				tmp |= Igd::Pipe_control::CS_STALL;
				tmp |= Igd::Pipe_control::FLUSH_ENABLE;
				tmp |= Igd::Pipe_control::GLOBAL_GTT_IVB;
				tmp |= Igd::Pipe_control::QW_WRITE;
				tmp |= Igd::Pipe_control::STORE_DATA_INDEX;

				cmd[1] = tmp;
				cmd[2] = HWS_DATA::OFFSET;
				cmd[3] = 0; /* upper addr 0 */
				cmd[4] = _current_seqno & 0xffffffff;
				cmd[5] = _current_seqno >> 32;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}


			/*
			 * emit semaphore we can later block on in order to stop ring
			 *
			 * 'emit_preempt_busywait' and 'gen12_emit_preempt_busywait'
			 */
			if (1)
			{
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = { };

				Igd::Mi_semaphore_wait sw;
				/* number of dwords after [1] */
				sw.dword_length(generation < 12 ? 2 : 3);

				cmd[0] = Mi_arb_check().value;
				cmd[1] = sw.value;
				cmd[2] = 0;                                  /* data word zero */
				cmd[3] = _device.hw_status_page_semaphore(); /* semaphore address low  */
				cmd[4] = 0;                                  /* semaphore address high */
				cmd[5] = generation < 12 ? Mi_noop().value : 0;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			if (1)
			{
				enum { CMD_NUM = 2 };
				Genode::uint32_t cmd[2] = {};
				Igd::Mi_user_interrupt ui;
				cmd[0] = Mi_arb_on_off(true).value;
				cmd[1] = ui.value;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/*
			 * epilog 3/3 - gen8_emit_fini_breadcrumb_rcs, gen8_emit_fini_breadcrumb_tail
			 *
			 * IHD-OS-BDW-Vol 2d-11.15 p. 199 ff.
			 *
			 * HWS page layout dword 48 - 1023 for driver usage
			 */
			if (1)
			{
				/* gen8_emit_fini_breadcrumb_tail -> gen8_emit_wa_tail */
				enum { CMD_NUM = 2 };
				Genode::uint32_t cmd[2] = {};
				cmd[0] = Mi_arb_check().value;
				cmd[1] = Mi_noop().value;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			addr_t const offset = ((tail + advance) * sizeof(uint32_t));

			if (offset % 8) {
				Genode::error("ring offset misaligned - abort");
				return false;
			}

			el.ring_flush(tail, tail + advance);

			/* tail_offset must be specified in qword */
			rcs.context->tail_offset((offset % (rcs.ring_size())) / 8);

			return true;
		}

		void rcs_map_ppgtt(addr_t vo, addr_t pa, size_t size)
		{
			Genode::Page_flags pf;
			pf.writeable = Genode::Writeable::RW;

			rcs.ppgtt->insert_translation(vo, pa, size, pf,
			                              &rcs.ppgtt_allocator,
			                              &rcs.ppgtt_scratch.pdp);
		}

		void rcs_unmap_ppgtt(addr_t vo, size_t size)
		{
			rcs.ppgtt->remove_translation(vo, size,
			                              &rcs.ppgtt_allocator,
			                              &rcs.ppgtt_scratch.pdp);
		}
	};

	/****************
	 ** SCHEDULING **
	 ****************/

	Genode::Fifo<Vgpu>  _vgpu_list { };
	Vgpu               *_active_vgpu { nullptr };

	void _submit_execlist(Engine<Rcs_context> &engine)
	{
		Execlist &el = *engine.execlist;

		int const port = _mmio.read<Igd::Mmio::EXECLIST_STATUS_RSCUNIT::Execlist_write_pointer>();


		/*
		 * Exec list might still be executing, check multiple times, in the normal
		 * case first iteration should succeed. If still running loop should end
		 * after 2 or three retries. Everything else needs to be inspected.
		 */
		bool empty = false;
		for (unsigned i = 0; i < 100; i++) {

			if (_mmio.read<Igd::Mmio::EXECLIST_STATUS_RSCUNIT::Execlist_0_valid>() == 0 &&
			    _mmio.read<Igd::Mmio::EXECLIST_STATUS_RSCUNIT::Execlist_1_valid>() == 0) {
				empty = true;
				break;
			}
		}

		/*
		 * In case exec list is still running write list anyway, it will preempt
		 * something in the worst case.  Nonetheless if you see the warning below,
		 * something is not right and should be investigated.
		 */
		if (!empty)
			warning("exec list is not empty");

		el.schedule(port);

		uint32_t desc[4];
		desc[3] = el.elem1().high();
		desc[2] = el.elem1().low();
		desc[1] = el.elem0().high();
		desc[0] = el.elem0().low();

		_mmio.write<Igd::Mmio::EXECLIST_SUBMITPORT_RSCUNIT>(desc[3]);
		_mmio.write<Igd::Mmio::EXECLIST_SUBMITPORT_RSCUNIT>(desc[2]);
		_mmio.write<Igd::Mmio::EXECLIST_SUBMITPORT_RSCUNIT>(desc[1]);
		_mmio.write<Igd::Mmio::EXECLIST_SUBMITPORT_RSCUNIT>(desc[0]);
	}

	void _submit_execlist_gen12(Engine<Rcs_context> &engine)
	{
		if (_mmio.read<Igd::Mmio::GEN12_EXECLIST_STATUS_RSCUNIT::Execution_queue_invalid>() == 0)
			return;

		Execlist &el = *engine.execlist;

		_mmio.write<Igd::Mmio::GEN12_EXECLIST_SQ_CONTENTS_RSCUNIT>(el.elem0().low(), 0);
		_mmio.write<Igd::Mmio::GEN12_EXECLIST_SQ_CONTENTS_RSCUNIT>(el.elem0().high(), 1);

		for (unsigned i = 2; i < 16; i++)
			_mmio.write<Igd::Mmio::GEN12_EXECLIST_SQ_CONTENTS_RSCUNIT>(0, i);

		/* load SQ to EQ */
		_mmio.write<Igd::Mmio::GEN12_EXECLIST_CONTROL_RSCUNIT::Load>(1);
	}

	Vgpu *_unschedule_current_vgpu()
	{
		if (_active_vgpu == nullptr) return nullptr;

		Vgpu *result = nullptr;
		_vgpu_list.dequeue([&] (Vgpu &head) {
			result = &head; });

		_active_vgpu = nullptr;

		return result;
	}

	Vgpu *_current_vgpu()
	{
		Vgpu *result = nullptr;
		_vgpu_list.head([&] (Vgpu &head) {
			result = &head; });
		return result;
	}

	void _schedule_current_vgpu()
	{
		Vgpu *gpu = _current_vgpu();
		if (!gpu) {
			Genode::warning("no valid vGPU for scheduling found.");
			return;
		}

		Engine<Rcs_context> &rcs = gpu->rcs;

		_mmio.flush_gfx_tlb();

		if (_info.generation < 11)
			_submit_execlist(rcs);
		else
			_submit_execlist_gen12(rcs);

		_active_vgpu = gpu;
		_timer.trigger_once(WATCHDOG_TIMEOUT);
	}

	/**********
	 ** INTR **
	 **********/

	/**
	 * \return true, if Vgpu is done and has not further work
	 */
	bool _notify_complete(Vgpu *gpu)
	{
		if (!gpu) { return true; }

		uint64_t const curr_seqno = gpu->current_seqno();
		uint64_t const comp_seqno = gpu->completed_seqno();

		Execlist &el = *gpu->rcs.execlist;
		el.ring_update_head(gpu->rcs.context->head_offset());

		if (curr_seqno != comp_seqno)
			return false;

		Genode::Signal_transmitter(gpu->completion_sigh()).submit();

		return true;
	}


	/************
	 ** FENCES **
	 ************/

	/* TODO introduce Fences object, Bit_allocator */
	enum { INVALID_FENCE = 0xff, };

	uint32_t _get_free_fence()
	{
		return _mmio.find_free_fence();
	}

	uint32_t _update_fence(uint32_t const id,
	                       addr_t   const lower,
	                       addr_t   const upper,
	                       uint32_t const pitch,
	                       bool     const tile_x)
	{
		return _mmio.update_fence(id, lower, upper, pitch, tile_x);
	}

	void _clear_fence(uint32_t const id)
	{
		_mmio.clear_fence(id);
	}

	/**********************
	 ** watchdog timeout **
	 **********************/

	void _handle_vgpu_after_reset(Vgpu &vgpu)
	{
		 /* signal completion of last job to vgpu */
		vgpu.mark_completed();
		_notify_complete(&vgpu);

		/* offset of head in ring context */
		size_t head_offset = vgpu.rcs.context->head_offset();
		/* set head = tail in ring and ring context */
		Execlist &el = *vgpu.rcs.execlist;
		el.ring_reset_to_head(head_offset);
		/* set tail in context in qwords */
		vgpu.rcs.context->tail_offset((head_offset % (vgpu.rcs.ring_size())) / 8);
	}

	void _handle_watchdog_timeout()
	{
		if (!_active_vgpu) { return; }

		Genode::error("watchdog triggered: engine stuck,"
		              " vGPU=", _active_vgpu->id(), " IRQ: ",
		              Hex(_mmio.read_irq_vector()));

		if (DEBUG) {
			_mmio.dump();
			_mmio.error_dump();
			_mmio.fault_dump();
			_mmio.execlist_status_dump();

			_active_vgpu->rcs.context->dump();
			_hw_status_page->dump();
			Execlist &el = *_active_vgpu->rcs.execlist;
			el.ring_update_head(_active_vgpu->rcs.context->head_offset());
			el.ring_dump(4096, _active_vgpu->rcs.context->tail_offset() * 2,
			                   _active_vgpu->rcs.context->head_offset());
		}


		Vgpu *vgpu = reset();

		if (!vgpu)
			error("reset vgpu is null");

		/* the stuck vgpu */
		_handle_vgpu_after_reset(*vgpu);
	}

	Genode::Signal_handler<Device> _watchdog_timeout_sigh {
		_env.ep(), *this, &Device::_handle_watchdog_timeout };

	void _device_reset_and_init()
	{
		_mmio.reset();
		_mmio.clear_errors();
		_mmio.init();
		_mmio.enable_intr();
	}

	/**
	 * Constructor
	 */
	Device(Genode::Env            & env,
	       Genode::Allocator      & alloc,
	       Platform::Connection   & platform,
	       Rm_connection          & rm,
	       Igd::Mmio              & mmio,
	       Platform::Device::Mmio & gmadr,
	       Genode::Xml_node       & supported,
	       uint16_t                 device_id,
	       uint8_t                  revision,
	       uint8_t                  gmch_ctl)
	:
		_env(env), _md_alloc(alloc), _platform(platform), _rm(rm),
		_mmio(mmio), _gmadr(gmadr), _gmch_ctl(gmch_ctl)
	{
		using namespace Genode;

		if (!_supported(supported, device_id, revision))
			throw Unsupported_device();


		_ggtt.dump();

		_vgpu_avail = (_gmadr.size() - APERTURE_RESERVED) / Vgpu::APERTURE_SIZE;

		_device_reset_and_init();

		_clock_gating();

		/* setup global hardware status page */
		_hw_status_ctx.construct(env.rm(), alloc, *this, 1, 0);
		_hw_status_page.construct(_hw_status_ctx->vaddr());

		Mmio::HWS_PGA_RCSUNIT::access_t const addr = _hw_status_ctx->gmaddr();
		_mmio.write_post<Igd::Mmio::HWS_PGA_RCSUNIT>(addr);

		/* read out slice, subslice, EUs information depending on platform */
		if (_info.platform == Device_info::Platform::BROADWELL) {
			enum { SUBSLICE_MAX = 3 };
			_subslice_mask.value = (1u << SUBSLICE_MAX) - 1;
			_subslice_mask.value &= ~_mmio.read<Igd::Mmio::FUSE2::Gt_subslice_disable_fuse_gen8>();

			for (unsigned i=0; i < SUBSLICE_MAX; i++)
				if (_subslice_mask.value & (1u << i))
					_subslices.value ++;

			_init_eu_total(3, SUBSLICE_MAX, 8);

		} else
		if (_info.generation == 9) {
			enum { SUBSLICE_MAX = 4 };
			_subslice_mask.value = (1u << SUBSLICE_MAX) - 1;
			_subslice_mask.value &= ~_mmio.read<Igd::Mmio::FUSE2::Gt_subslice_disable_fuse_gen9>();

			for (unsigned i=0; i < SUBSLICE_MAX; i++)
				if (_subslice_mask.value & (1u << i))
					_subslices.value ++;

			_init_eu_total(3, SUBSLICE_MAX, 8);
		} else
		if (_info.generation == 12) {
			_init_topology_gen12();
		} else
			Genode::error("unsupported platform ", (int)_info.platform);

		/* apply generation specific workarounds */
		apply_workarounds(_mmio, _info.generation);

		_timer.sigh(_watchdog_timeout_sigh);
	}

	void _init_topology_gen12()
	{
		/* NOTE: This needs to be different for DG2 and Xe_HP */
		_topology.max_slices           = 1;
		_topology.max_subslices        = 6;
		_topology.max_eus_per_subslice = 16;
		_topology.ss_stride = 1; /* roundup(6/8) */
		_topology.eu_stride = 2; /* 16/8 */

		/* NOTE: 1 for >=12.5 */
		_topology.slice_mask = _mmio.read<Igd::Mmio::MIRROR_GT_SLICE_EN::Enabled>();
		if (_topology.slice_mask > 1)
			error("topology: slices > 1");

		uint32_t dss_en = _mmio.read<Igd::Mmio::MIRROR_GT_DSS_ENABLE>();
		memcpy(_topology.subslice_mask, &dss_en, sizeof(dss_en));

		/* Gen12 uses dual-subslices */
		uint8_t eu_en_fuse = ~_mmio.read<Igd::Mmio::MIRROR_EU_DISABLE0::Disabled>();
		uint16_t eu_en { 0 };
		for (unsigned i = 0; i < _topology.max_eus_per_subslice / 2; i++) {
			if (eu_en_fuse & (1u << i)) {
				_eus.value += 2;
				eu_en |= (3u << (i * 2));
			}
		}

		for (unsigned i = 0; i < _topology.max_subslices; i++) {
			if (_topology.has_subslice(0, i)) {
				_subslices.value++;
				unsigned offset = _topology.eu_idx(0, i);
				for (unsigned j = 0; j < _topology.eu_stride; j++) {
					_topology.eu_mask[offset + j] = (eu_en >> (8 * j)) & 0xff;
				}
			}
		}

		_topology.valid = true;
	}

	void _clock_gating()
	{
		if (_info.platform == Device_info::Platform::KABYLAKE) {
			_mmio.kbl_clock_gating();
		} else
			Genode::warning("no clock gating");
	}

	void _init_eu_total(uint8_t const max_slices,
	                    uint8_t const max_subslices,
	                    uint8_t const max_eus_per_subslice)
	{
		if (max_eus_per_subslice != 8) {
			Genode::error("wrong eu_total calculation");
		}

		_slice_mask.value = _mmio.read<Igd::Mmio::FUSE2::Gt_slice_enable_fuse>();

		unsigned eu_total = 0;

		/* determine total amount of available EUs */
		for (unsigned disable_byte = 0; disable_byte < 12; disable_byte++) {
			unsigned const fuse_bits = disable_byte * 8;
			unsigned const slice     = fuse_bits / (max_subslices * max_eus_per_subslice);
			unsigned const subslice  = fuse_bits / max_eus_per_subslice;

			if (fuse_bits >= max_slices * max_subslices * max_eus_per_subslice)
				break;

			if (!(_subslice_mask.value & (1u << subslice)))
				continue;

			if (!(_slice_mask.value & (1u << slice)))
				continue;

			auto const disabled = _mmio.read<Igd::Mmio::EU_DISABLE>(disable_byte);

			for (unsigned b = 0; b < 8; b++) {
				if (disabled & (1u << b))
					continue;
				eu_total ++;
			}

		}

		_eus = Gpu::Info_intel::Eu_total { eu_total };
	}

	/*********************
	 ** Device handling **
	 *********************/

	/**
	 * Reset the physical device
	 */
	Vgpu *reset()
	{
		/* Stop render engine
		 *
		 * WaKBLVECSSemaphoreWaitPoll:kbl (on ALL_ENGINES):
		 * KabyLake suffers from system hangs when batchbuffer is progressing during
		 * reset
		 */
		hw_status_page_pause_ring(true);

		Vgpu *vgpu { nullptr };

		/* unschedule current vgpu */
		if (_active_vgpu) {
			vgpu = _active_vgpu;
			vgpu_unschedule(*_active_vgpu);
		}

		/* reset */
		_reset.execute();

		/* set address of global hardware status page */
		if (_hw_status_ctx.constructed()) {
			Mmio::HWS_PGA_RCSUNIT::access_t const addr = _hw_status_ctx->gmaddr();
			_mmio.write_post<Igd::Mmio::HWS_PGA_RCSUNIT>(addr);
		}

		_mmio.clear_errors();

		/* clear pending irqs */
		_mmio.clear_render_irq();

		/*
		 * Restore "Hardware Status Mask Register", this register controls which
		 * IRQs are even written to the PCI bus (should be same as unmasked in IMR)
		 */
		_mmio.restore_hwstam();

		hw_status_page_pause_ring(false);

		if (_current_vgpu()) {
			_schedule_current_vgpu();
		}

		return vgpu;
	}

	/**
	 * Get chip id of the physical device
	 */
	uint16_t id() const { return _info.id; }

	/**
	 * Get features of the physical device
	 */
	uint32_t features() const { return _info.features; }

	/**
	 * Get generation of the physical device
	 */
	Igd::Generation generation() const {
		return Igd::Generation { _info.generation }; }

	bool match(Device_info::Platform const platform,
	           Device_info::Stepping const stepping_start,
	           Device_info::Stepping const stepping_end)
	{
		if (_info.platform != platform)
			return false;

		/* XXX only limited support, for now just kabylake */
		if (platform != Device_info::Platform::KABYLAKE) {
			Genode::warning("unsupported platform match request");
			return false;
		}

		Device_info::Stepping stepping = Device_info::Stepping::A0;

		switch (_revision.value) {
		case 0: stepping = Device_info::Stepping::A0; break;
		case 1: stepping = Device_info::Stepping::B0; break;
		case 2: stepping = Device_info::Stepping::C0; break;
		case 3: stepping = Device_info::Stepping::D0; break;
		case 4: stepping = Device_info::Stepping::F0; break;
		case 5: stepping = Device_info::Stepping::C0; break;
		case 6: stepping = Device_info::Stepping::D1; break;
		case 7: stepping = Device_info::Stepping::G0; break;
		default:
			Genode::error("unsupported KABYLAKE revision detected");
			break;
		}

		return stepping_start <= stepping && stepping <= stepping_end;
	}


	/**************************
	 ** Hardware status page **
	 **************************/

	addr_t hw_status_page_gmaddr() const { return _hw_status_ctx->gmaddr(); }

	addr_t hw_status_page_semaphore() const
	{
		return hw_status_page_gmaddr() + Hardware_status_page::Semaphore::OFFSET;
	}

	/*
	 * Pause the physical ring by setting semaphore value programmed by
	 * 'setup_ring_vram' to 1, causing GPU to spin.
	 */
	void hw_status_page_pause_ring(bool pause)
	{
		_hw_status_page->semaphore(pause ? 1 : 0);
	}

	uint64_t seqno()
	{
		return _hw_status_page->sequence_number();
	}


	/*******************
	 ** Vgpu handling **
	 *******************/

	/**
	 * Add vGPU to scheduling list if not enqueued already
	 *
	 * \param vgpu  reference to vGPU
	 */
	void vgpu_activate(Vgpu &vgpu)
	{
		if (vgpu.enqueued())
			return;

		Vgpu const *pending = _current_vgpu();

		_vgpu_list.enqueue(vgpu);

		if (pending) { return; }

		/* none pending, kick-off execution */
		_schedule_current_vgpu();
	}

	/**
	 * Check if there is a vGPU slot left
	 *
	 * \return true if slot is free, otherwise false is returned
	 */
	bool vgpu_avail() const { return _vgpu_avail; }

	/**
	 * Increase/decrease available vGPU count.
	 */
	void vgpu_created()  { _vgpu_avail --; }
	void vgpu_released() { _vgpu_avail ++; }

	/**
	 * Check if vGPU is currently scheduled
	 *
	 * \return true if vGPU is scheduled, otherwise false is returned
	 */
	bool vgpu_active(Vgpu const &vgpu) const
	{
		return _active_vgpu == &vgpu;
	}

	/**
	 * Remove VGPU from scheduling list, if it is enqeued
	 */
	void vgpu_unschedule(Vgpu &vgpu)
	{
		if (vgpu_active(vgpu)) _active_vgpu = nullptr;

		if (vgpu.enqueued())
			_vgpu_list.remove(vgpu);
	}

	/*******************
	 ** Vram handling **
	 *******************/

	/**
	 * Allocate DMA vram
	 *
	 * \param guard  resource allocator and guard
	 * \param size   size of the DMA vram
	 *
	 * \return DMA vram capability
	 *
	 * \throw Out_of_memory
	 */
	Genode::Ram_dataspace_capability alloc_vram(Allocator &,
	                                              size_t const size)
	{
		return _pci_backend_alloc.alloc(size);
	}


	/**
	 * Get physical address for DMA vram
	 *
	 * \param ds_cap  ram dataspace capability
	 *
	 * \return physical DMA address
	 */
	Genode::addr_t dma_addr(Genode::Ram_dataspace_capability ds_cap)
	{
		return _pci_backend_alloc.dma_addr(ds_cap);
	}

	/**
	 * Free DMA vram
	 *
	 * \param guard  resource allocator and guard
	 * \param cap    DMA vram capability
	 */
	void free_vram(Allocator &,
	                 Dataspace_capability const cap)
	{
		if (!cap.valid()) { return; }

		_pci_backend_alloc.free(Genode::static_cap_cast<Genode::Ram_dataspace>(cap));
	}

	/**
	 * Map DMA vram in the GGTT
	 *
	 * \param guard     resource allocator and guard
	 * \param cap       DMA vram capability
	 * \param aperture  true if mapping should be accessible by CPU
	 *
	 * \return GGTT mapping
	 *
	 * \throw Could_not_map_vram
	 */
	Ggtt::Mapping const &map_vram(Genode::Allocator &guard,
	                                Genode::Ram_dataspace_capability cap,
	                                bool aperture)
	{
		if (aperture == false) {
			error("GGTT mapping outside aperture");
			throw Could_not_map_vram();
		}

		size_t const size = Genode::Dataspace_client(cap).size();
		size_t const num = size / PAGE_SIZE;
		Ggtt::Offset const offset = _ggtt.find_free(num, aperture);
		return map_dataspace_ggtt(guard, cap, offset);
	}

	/**
	 * Unmap DMA vram from GGTT
	 *
	 * \param guard    resource allocator and guard
	 * \param mapping  GGTT mapping
	 */
	void unmap_vram(Genode::Allocator &guard, Ggtt::Mapping mapping)
	{
		unmap_dataspace_ggtt(guard, mapping.cap);
	}

	/**
	 * Set tiling mode for GGTT region
	 *
	 * \param start  offset of the GGTT start entry
	 * \param size   size of the region
	 * \param mode   tiling mode for the region
	 *
	 * \return id of the used fence register
	 */
	uint32_t set_tiling(Ggtt::Offset const start, size_t const size,
	                    uint32_t const mode)
	{
		uint32_t const id = _get_free_fence();
		if (id == INVALID_FENCE) {
			Genode::warning("could not find free FENCE");
			return id;
		}
		addr_t   const lower = start * PAGE_SIZE;
		addr_t   const upper = lower + size;
		uint32_t const pitch = ((mode & 0xffff0000) >> 16) / 128 - 1;
		bool     const tilex = (mode & 0x1);

		return _update_fence(id, lower, upper, pitch, tilex);
	}

	/**
	 * Clear tiling for given fence
	 *
	 * \param id  id of fence register
	 */
	void clear_tiling(uint32_t const id)
	{
		_clear_fence(id);
	}

	bool handle_irq()
	{
		bool display_irq = _mmio.display_irq();

		/* handle render interrupts only */
		if (_mmio.render_irq() == false)
			return display_irq;

		_mmio.disable_master_irq();

		unsigned const v = _mmio.read_irq_vector();

		bool ctx_switch = _mmio.context_switch(v);
		bool user_complete = _mmio.user_complete(v);

		if (v) {
			_mmio.clear_render_irq(v);
	}

		Vgpu *notify_gpu = nullptr;
		if (user_complete) {
			notify_gpu = _current_vgpu();
			if (notify_gpu)
				notify_gpu->user_complete();
		}

		bool const fault_valid = _mmio.fault_regs_valid();
		if (fault_valid) { Genode::error("FAULT_REG valid"); }

		if (ctx_switch)
			_mmio.update_context_status_pointer();

		if (user_complete) {
			_unschedule_current_vgpu();

			if (notify_gpu) {
				if (!_notify_complete(notify_gpu)) {
					_vgpu_list.enqueue(*notify_gpu);
				}
			}

			/* keep the ball rolling...  */
			if (_current_vgpu()) {
				_schedule_current_vgpu();
			}
		}

		return display_irq;
	}

	void enable_master_irq() { _mmio.enable_master_irq(); }

	private:

		/*
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);
};


namespace Gpu {

	class Session_component;
	class Root;

	using Root_component = Genode::Root_component<Session_component, Genode::Multiple_clients>;
}


struct Gpu::Vram : Genode::Interface
{
	GENODE_RPC_INTERFACE();
};


class Gpu::Session_component : public Genode::Session_object<Gpu::Session>
{
	private:

		/*
		 * Vram managed by session ep
		 */
		struct Vram : Rpc_object<Gpu::Vram>
		{
			/*
			 * Do not copy session cap, since this increases the session-cap reference
			 * count for each Vram object.
			 */
			struct Owner {
				Capability<Gpu::Session> cap;
			};

			Ram_dataspace_capability ds_cap;
			Owner const             &owning_session;

			enum { INVALID_FENCE = 0xff };
			Genode::uint32_t fenced { INVALID_FENCE };

			Igd::Ggtt::Mapping map { };

			addr_t phys_addr { 0 };
			size_t size { 0 };

			bool   caps_used { false };
			size_t ram_used { 0 };

			Vram(Ram_dataspace_capability ds_cap, Genode::addr_t phys_addr,
			     Owner const &owner)
			:
				ds_cap { ds_cap }, owning_session { owner },
				phys_addr { phys_addr }
			{
				Dataspace_client buf(ds_cap);
				size = buf.size();
			}

			bool owner(Capability<Gpu::Session> const other)
			{
				return owning_session.cap == other;
			}
		};

		Genode::Env              &_env;
		Genode::Region_map       &_rm;
		Constrained_ram_allocator _ram;
		Igd::Device              &_device;
		Heap                      _heap { _device._pci_backend_alloc, _rm };
		/* used to mark ownership of an allocated VRAM object */
		Vram::Owner               _owner { cap() };

		Igd::Device::Vgpu  _vgpu;

		struct Resource_guard
		{
			Cap_quota_guard &_cap_quota_guard;
			Ram_quota_guard &_ram_quota_guard;

			Resource_guard(Cap_quota_guard &cap, Ram_quota_guard &ram)
			:
				_cap_quota_guard { cap },
				_ram_quota_guard { ram }
			{ }

			/* worst case */
			bool avail_caps() { return _cap_quota_guard.have_avail(Cap_quota { 15 }); }

			/* size + possible heap allocations  + possible page table allocation +
			 * + 16KB for 'ep.manage' + unkown overhead */
			bool avail_ram(size_t size = 0) {
				return _ram_quota_guard.have_avail(Ram_quota { size + 2*1024*1024+4096 +
				                                               1024*1024 + 16*1024 + 1024*1024}); }

			void withdraw(size_t caps_old, size_t caps_new,
			              size_t ram_old, size_t ram_new)
			{
				size_t caps = caps_old > caps_new ? caps_old - caps_new : 0;
				size_t ram  = ram_old > ram_new ? ram_old - ram_new : 0;

				try {
					_cap_quota_guard.withdraw(Cap_quota { caps });
					_ram_quota_guard.withdraw(Ram_quota { ram });
				} catch (Genode::Out_of_caps) {
					/*
					 * At this point something in the accounting went wrong
					 * and as quick-fix let the client abort rather than the
					 * multiplexer.
					 */
					Genode::error("Quota guard out of caps! from ", __builtin_return_address(0));
					throw Gpu::Session::Out_of_caps();
				} catch (Genode::Out_of_ram) {
					Genode::error("Quota guard out of ram! from ", __builtin_return_address(0));
					Genode::error("guard ram: ", _ram_quota_guard.avail().value, " requested: ", ram);
					throw Gpu::Session::Out_of_ram();
				} catch (...) {
					Genode::error("Unknown exception in 'Resourcd_guard::withdraw'");
					throw;
				}
			}

			void replenish(size_t caps, size_t ram)
			{
				_cap_quota_guard.replenish(Cap_quota { caps });
				_ram_quota_guard.replenish(Ram_quota { ram });
			}
		};

		Resource_guard _resource_guard { _cap_quota_guard(), _ram_quota_guard() };

		/*
		 * Vram session/gpu-context local vram
		 */
		struct Vram_local
		{
			/* keep track of mappings of different offsets */
			struct Mapping : Dictionary<Mapping, off_t>::Element
			{
				addr_t ppgtt_va { 0 };
				size_t ppgtt_va_size { 0 };

				Mapping(Dictionary<Mapping, off_t> &dict, off_t offset,
				        addr_t ppgtt_va, size_t ppgtt_va_size)
				:
					Dictionary<Mapping, off_t>::Element(dict, offset),
					ppgtt_va(ppgtt_va), ppgtt_va_size(ppgtt_va_size)
				{  }
			};

			using Id_space = Genode::Id_space<Vram_local>;

			Vram_capability      const vram_cap;
			size_t                     size;
			Id_space::Element    const elem;
			Dictionary<Mapping, off_t> mappings { };

			addr_t            ppgtt_va { 0 };
			bool              ppgtt_va_valid { false };

			Vram_local(Vram_capability vram_cap, size_t size,
			             Id_space &space, Vram_id id)
			: vram_cap(vram_cap), size(size),
			  elem(*this, space, Id_space::Id { .value = id.value })
			{ }
		};

		Id_space<Vram_local> _vram_space { };

		template <typename FN>
		void _apply_vram(Vram_local &vram_local, FN const &fn)
		{
			Vram *v = nullptr;
			bool free = _env.ep().rpc_ep().apply(vram_local.vram_cap, [&] (Vram *vram) {
				if (vram) {
					v = vram;
					return fn(*vram);
				}
				return false;
			});

			if (v && free)
				destroy(&_heap, v);
		}

		bool _vram_valid(Vram_capability vram_cap)
		{
			bool valid = false;
			_env.ep().rpc_ep().apply(vram_cap, [&] (Vram *vram) {
				if (vram) valid = true;
			});

			return valid;
		}

		template <typename FN>
		void _apply_vram_local(Gpu::Vram_id id, FN const &fn)
		{
			Vram_local::Id_space::Id local_id { .value = id.value };
			try {
				_vram_space.apply<Vram_local>(local_id, [&] (Vram_local &vram) {
					fn(vram);
				});
			} catch (Vram_local::Id_space::Unknown_id) {
				error("Unknown id: ", id.value);
			}
		}

		Genode::uint64_t seqno { 0 };

		void _free_local_vram(Vram_local &vram_local)
		{
			vram_local.mappings.for_each([&] (Vram_local::Mapping const &mapping) {
				_vgpu.rcs_unmap_ppgtt(mapping.ppgtt_va, mapping.ppgtt_va_size);
			});

			while (vram_local.mappings.with_any_element([&] (Vram_local::Mapping &mapping) {
				destroy(_heap, &mapping);
			})) { }

			destroy(&_heap, &vram_local);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param rm         region map used for attaching GPU resources
		 * \param md_alloc   meta-data allocator
		 * \param ram_quota  initial ram quota
		 * \param device     reference to the physical device
		 */
		Session_component(Env &env,
		                  Entrypoint    &ep,
		                  Ram_allocator &ram,
		                  Region_map    &rm,
		                  Resources      resources,
		                  Label   const &label,
		                  Diag           diag,
		                  Igd::Device   &device)
		:
			Session_object(ep, resources, label, diag),
			_env(env),
			_rm(rm),
			_ram(ram, _ram_quota_guard(), _cap_quota_guard()),
			_device(device),
			_vgpu(_device, _heap, ram, rm)
		{ }

		~Session_component()
		{
			auto lookup_and_free = [&] (Vram_local &vram_local) {

				_apply_vram(vram_local, [&](Vram &vram) {

					/*
					 * Note: cap() will return invalid cap at this point, so use
					 * _owner.cap
					 */
					if (vram.owner(_owner.cap) == false) return false;

					if (vram.map.offset != Igd::Ggtt::Mapping::INVALID_OFFSET) {
						_device.unmap_vram(_heap, vram.map);
					}

					if (vram.fenced != Vram::INVALID_FENCE) {
						_device.clear_tiling(vram.fenced);
						_vgpu.active_fences--;
					}

					_env.ep().dissolve(vram);
					_device.free_vram(_heap, vram.ds_cap);
					return true;
				});

				_free_local_vram(vram_local);
			};

			while(_vram_space.apply_any<Vram_local>(
				[&] (Vram_local &vram_local) { lookup_and_free(vram_local); }));
		}

		/*********************************
		 ** Session_component interface **
		 *********************************/

		void upgrade_resources(Session::Resources resources)
		{
			upgrade(resources.ram_quota);
			upgrade(resources.cap_quota);
		}

		void dump_resources()
		{
			Genode::error(__func__, ": session (cap: ", _cap_quota_guard(),
			              " ram: ", _ram_quota_guard(), ") env: (cap: ",
			              "avail=", _env.pd().avail_caps(), " used=", _env.pd().used_caps(),
			              " ram: avail=", _env.pd().avail_ram(), " used=", _env.pd().used_ram());
		}

		bool vgpu_active() const
		{
			return _device.vgpu_active(_vgpu);
		}

		void vgpu_unschedule()
		{
			_device.vgpu_unschedule(_vgpu);
		}

		/***************************
		 ** Gpu session interface **
		 ***************************/

		Genode::Dataspace_capability info_dataspace() const override
		{
			return _vgpu.info_dataspace();
		}

		Gpu::Sequence_number execute(Vram_id id, off_t offset) override
		{
			bool found = false;

			_apply_vram_local(id, [&] (Vram_local &vram_local) {

				if (_vram_valid(vram_local.vram_cap) == false) {
					_free_local_vram(vram_local);
					return;
				}

				addr_t ppgtt_va { 0 };

				bool ppgtt_va_valid = vram_local.mappings.with_element(offset,
					[&] (Vram_local::Mapping const &mapping) {
						ppgtt_va = mapping.ppgtt_va; return true; },
					[]() { return false; });

				if (!ppgtt_va_valid) {
					Genode::error("Invalid execvram");
					Genode::Signal_transmitter(_vgpu.completion_sigh()).submit();
					throw Gpu::Session::Invalid_state();
				}

				found = _vgpu.setup_ring_vram(ppgtt_va);
			});

			if (!found)
				throw Gpu::Session::Invalid_state();

			_device.vgpu_activate(_vgpu);
			return { .value = _vgpu.current_seqno() };
		}

		bool complete(Gpu::Sequence_number seqno) override
		{
			return _vgpu.completed_seqno() >= seqno.value;
		}

		void completion_sigh(Genode::Signal_context_capability sigh) override
		{
			_vgpu.completion_sigh(sigh);
		}

		Genode::Dataspace_capability alloc_vram(Gpu::Vram_id id,
		                                        Genode::size_t size) override
		{
			/* roundup to next page size */
			size = align_addr(size, 12);

			if (_resource_guard.avail_caps() == false)
				throw Gpu::Session::Out_of_caps();

			if (_resource_guard.avail_ram(size) == false)
				throw Gpu::Session::Out_of_ram();

			size_t caps_before = _env.pd().avail_caps().value;
			size_t ram_before  = _env.pd().avail_ram().value;

			Ram_dataspace_capability ds_cap = _device.alloc_vram(_heap, size);
			addr_t phys_addr                = _device.dma_addr(ds_cap);
			Vram *vram                      = new (&_heap) Vram(ds_cap, phys_addr, _owner);
			_env.ep().manage(*vram);

			try {
				new (&_heap) Vram_local(vram->cap(), size, _vram_space, id);
			} catch (Id_space<Vram_local>::Conflicting_id) {
				_env.ep().dissolve(*vram);
				destroy(&_heap, vram);
				_device.free_vram(_heap, ds_cap);
				return Dataspace_capability();
			}

			size_t caps_after = _env.pd().avail_caps().value;
			size_t ram_after  = _env.pd().avail_ram().value;

			/* limit to vram size for replenish */
			vram->ram_used  = min(ram_before > ram_after ? ram_before - ram_after : 0, size);
			vram->caps_used = caps_before > caps_after ? true  : false;

			_resource_guard.withdraw(caps_before, caps_after, ram_before, ram_after);

			return ds_cap;
		}

		void free_vram(Gpu::Vram_id id) override
		{
			auto lookup_and_free = [&] (Vram_local &vram_local) {

				_apply_vram(vram_local, [&](Vram &vram) {

					if (vram.owner(cap()) == false) return false;

					if (vram.map.offset != Igd::Ggtt::Mapping::INVALID_OFFSET) {
						Genode::error("cannot free mapped vram");
						/* XXX throw */
						return false;
					}
					_env.ep().dissolve(vram);
					_device.free_vram(_heap, vram.ds_cap);
					_resource_guard.replenish(vram.caps_used ? 1 : 0,
					                          vram.ram_used);
					return true;
				});

				_free_local_vram(vram_local);
			};

			_apply_vram_local(id, lookup_and_free);
		}

		Vram_capability export_vram(Vram_id id) override
		{
			Vram_capability cap { };
			_apply_vram_local(id, [&] (Vram_local &vram_local) {
				if (_vram_valid(vram_local.vram_cap))
					cap = vram_local.vram_cap;
			});
			return cap;
		}

		void import_vram(Vram_capability cap, Vram_id id) override
		{
			if (_vram_valid(cap) == false)
				throw Gpu::Session::Invalid_state();

			try {
				Vram_local *vram_local = new (_heap) Vram_local(cap, 0, _vram_space, id);

				_apply_vram(*vram_local, [&](Vram &vram) {
					vram_local->size = vram.size; return false; });

			} catch (Id_space<Vram_local>::Conflicting_id) {
				throw Gpu::Session::Conflicting_id();
			} catch (Cap_quota_guard::Limit_exceeded) {
				throw Gpu::Session::Out_of_caps();
			} catch (Ram_quota_guard::Limit_exceeded) {
				throw Gpu::Session::Out_of_ram();
			}
		}

		Genode::Dataspace_capability map_cpu(Gpu::Vram_id,
		                                     Gpu::Mapping_attributes) override
		{
			error("map_cpu: called not implemented");
			throw Mapping_vram_failed();
		}

		void unmap_cpu(Vram_id) override
		{
			error("unmap_cpu: called not implemented");
		}

		bool map_gpu(Vram_id id, size_t size, off_t offset, Gpu::Virtual_address va) override
		{
			auto lookup_and_map = [&] (Vram_local &vram_local) {

				if (vram_local.mappings.exists(offset)) {
					Genode::error("vram already mapped at offset: ", Hex(offset));
					return;
				}

				addr_t phys_addr = 0;
				_apply_vram(vram_local, [&](Vram &vram) {
					phys_addr = vram.phys_addr; return false; });

				if (phys_addr == 0) {
					_free_local_vram(vram_local);
					return;
				}

				try {
				_vgpu.rcs_map_ppgtt(va.value, phys_addr + offset, size);
				} catch (Level_4_translation_table::Double_insertion) {
					error("PPGTT: Double insertion: va: ", Hex(va.value), " offset: ", Hex(offset),
					      "size: ", Hex(size));
					throw Mapping_vram_failed();
				} catch(...) {
					error("PPGTT: invalid address/range/alignment: va: ", Hex(va.value),
					      " offset: ", Hex(offset),
					      "size: ", Hex(size));
					throw Mapping_vram_failed();
				}

				new (_heap) Vram_local::Mapping(vram_local.mappings, offset, va.value, size);
			};

			if (_resource_guard.avail_caps() == false)
				throw Gpu::Session::Out_of_caps();

			if (_resource_guard.avail_ram() == false)
				throw Gpu::Session::Out_of_ram();

			size_t caps_before = _env.pd().avail_caps().value;
			size_t ram_before  = _env.pd().avail_ram().value;

			_apply_vram_local(id, lookup_and_map);

			size_t caps_after = _env.pd().avail_caps().value;
			size_t ram_after  = _env.pd().avail_ram().value;

			_resource_guard.withdraw(caps_before, caps_after,
			                         ram_before, ram_after);

			return true;
		}

		void unmap_gpu(Vram_id id, off_t offset, Gpu::Virtual_address va) override
		{
			auto lookup_and_unmap = [&] (Vram_local &vram_local) {

				vram_local.mappings.with_element(offset,
					[&] (Vram_local::Mapping &mapping) {

						if (mapping.ppgtt_va != va.value) {
							Genode::error("VRAM: not mapped at ", Hex(va.value), " offset: ", Hex(offset));
							return;
						}
						_vgpu.rcs_unmap_ppgtt(va.value, mapping.ppgtt_va_size);
						destroy(_heap, &mapping);
					},
					[&] () { error("VRAM: nothing mapped at offset ", Hex(offset)); }
				);
			};
			_apply_vram_local(id, lookup_and_unmap);
		}

		bool set_tiling_gpu(Vram_id id, off_t offset,
		                    unsigned mode) override
		{
			if (_vgpu.active_fences > Igd::Device::Vgpu::MAX_FENCES) {
				Genode::error("no free fences left, already active: ", _vgpu.active_fences);
				return false;
			}

			Vram *v = nullptr;
			auto lookup = [&] (Vram &vram) {
				if (!vram.map.cap.valid() || !vram.owner(cap())) { return false; }
				v = &vram;
				return false;
			};

			_apply_vram_local(id, [&](Vram_local &vram_local) {
				_apply_vram(vram_local, lookup);
			});

			if (v == nullptr) {
				Genode::error("attempt to set tiling for non-mapped or non-owned vram");
				return false;
			}

			//XXX: support change of already fenced bo's fencing mode
			if (v->fenced) return true;

			Igd::size_t const size = v->size;
			Genode::uint32_t const fenced = _device.set_tiling(v->map.offset + offset, size, mode);

			v->fenced = fenced;
			if (fenced != Vram::INVALID_FENCE) { _vgpu.active_fences++; }
			return fenced != Vram::INVALID_FENCE;
		}
};


class Gpu::Root : public Gpu::Root_component
{
	private:

		/*
		 * Noncopyable
		 */
		Root(Root const &);
		Root &operator = (Root const &);

		Genode::Env &_env;
		Igd::Device *_device;

		Genode::size_t _ram_quota(char const *args)
		{
			return Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		}

	protected:

		Session_component *_create_session(char const *args) override
		{
			if (!_device || !_device->vgpu_avail()) {
				throw Genode::Service_denied();
			}

			/* at the moment we just need about ~160KiB for initial RCS bring-up */
			Genode::size_t const required_quota = Gpu::Session::REQUIRED_QUOTA / 2;
			Genode::size_t const ram_quota      = _ram_quota(args);

			if (ram_quota < required_quota) {
				Genode::Session_label const label = Genode::label_from_args(args);
				Genode::warning("insufficient dontated ram_quota (", ram_quota,
				                " bytes), require ", required_quota, " bytes ",
				                " by '", label, "'");
				throw Session::Out_of_ram();
			}

			Genode::Session::Resources resources = session_resources_from_args(args);
			try {

				using namespace Genode;

				return new (md_alloc())
					Session_component(_env, _env.ep(), _device->_pci_backend_alloc, _env.rm(),
					                  resources,
					                  session_label_from_args(args),
					                  session_diag_from_args(args),
					                  *_device);
			} catch (...) { throw; }
		}

		void _upgrade_session(Session_component *s, const char *args) override
		{
			Session::Resources const res = session_resources_from_args(args);
			s->upgrade_resources(res);
		}

		void _destroy_session(Session_component *s) override
		{
			if (s->vgpu_active()) {
				Genode::warning("vGPU active, reset device and hope for the best");
				_device->reset();
			} else {
				/* remove from scheduled list */
				s->vgpu_unschedule();
			}

			Genode::destroy(md_alloc(), s);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &alloc)
		: Root_component(env.ep(), alloc), _env(env), _device(nullptr) { }

		void manage(Igd::Device &device) { _device = &device; }
};


struct Initialization_failed : Genode::Exception { };


struct Main : Irq_ack_handler, Gpu_reset_handler
{
	Env                   & _env;
	Sliced_heap             _root_heap      { _env.ram(), _env.rm()         };
	Gpu::Root               _gpu_root       { _env, _root_heap              };
	Attached_rom_dataspace  _config_rom     { _env, "config"                };
	Heap                    _md_alloc       { _env.ram(), _env.rm()         };
	Platform::Connection    _platform       { _env                          };
	Platform::Device        _device         { _platform                     };
	Platform::Device::Irq   _irq            { _device                       };
	Igd::Mmio               _mmio           { _device, _env                 };
	Platform::Device::Mmio  _gmadr          { _device, { 1 }                };
	Rm_connection           _rm             { _env                          };
	Io_signal_handler<Main> _irq_dispatcher { _env.ep(), *this,
	                                          &Main::handle_irq             };
	Signal_handler<Main>    _config_sigh    { _env.ep(), *this,
	                                          &Main::_handle_config_update  };

	Platform::Root _platform_root { _env, _md_alloc, _platform, *this, *this,
	                                _mmio, _gmadr, _rm };

	Genode::Constructible<Igd::Device> _igd_device { };

	Main(Genode::Env &env)
	:
		_env(env)
	{
		_config_rom.sigh(_config_sigh);

		/* IRQ */
		_irq.sigh(_irq_dispatcher);

		/* GPU */
		_handle_config_update();
	}

	void _create_device()
	{
		uint16_t device_id;
		uint8_t  revision;
		uint8_t  gmch_ctl;

		_platform.update();
		_platform.with_xml([&] (Xml_node node) {
			node.with_optional_sub_node("device", [&] (Xml_node node) {
				node.with_optional_sub_node("pci-config", [&] (Xml_node node) {
					device_id = node.attribute_value("device_id", 0U);
					revision  = node.attribute_value("revision",  0U);
					gmch_ctl  = node.attribute_value("intel_gmch_control",  0U);
				});
			});
		});

		try {
			_igd_device.construct(_env, _md_alloc, _platform, _rm, _mmio,
			                      _gmadr, _config_rom.xml(), device_id,
			                      revision, gmch_ctl);
			_gpu_root.manage(*_igd_device);
			_env.parent().announce(_env.ep().manage(_gpu_root));
		} catch (Igd::Device::Unsupported_device) {
			Genode::warning("No supported Intel GPU detected - no GPU service");
		} catch (...) {
			Genode::error("Unknown error occurred - no GPU service");
		}
	}

	void _handle_config_update()
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		if (_igd_device.constructed()) {
			Genode::log("gpu device already initialized - ignore");
			return;
		}

		_create_device();
	}

	void handle_irq()
	{
		bool display_irq = false;
		if (_igd_device.constructed())
			display_irq = _igd_device->handle_irq();
		/* GPU not present forward all IRQs to platform client */
		else {
			_platform_root.handle_irq();
			return;
		}

		/*
		 * GPU present check for display engine related IRQs before calling platform
		 * client
		 */
		if (display_irq && _platform_root.handle_irq()) {
			return;
		}

		ack_irq();
	}

	void ack_irq() override
	{
		if (_igd_device.constructed()) {
			_igd_device->enable_master_irq();
		}

		_irq.ack();
	}

	void reset() override
	{
		addr_t const base = _mmio.base() + (_mmio.size() / 2);
		Igd::Ggtt(_platform, _mmio, base, Igd::GTT_RESERVED, 0, 0);
	}
};


void Component::construct(Genode::Env &env)
{
	static Constructible<Main> main;
	try {
		main.construct(env);
	} catch (Initialization_failed) {
		Genode::warning("Intel GPU resources not found.");
		env.parent().exit(0);
	}
}


Genode::size_t Component::stack_size() { return 32UL*1024*sizeof(long); }
