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
#include <gpu_session/gpu_session.h>
#include <gpu/info_intel.h>
#include <io_mem_session/connection.h>
#include <irq_session/connection.h>
#include <legacy/x86/platform_device/client.h>
#include <legacy/x86/platform_session/connection.h>
#include <root/component.h>
#include <timer_session/connection.h>
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
#include <resources.h>
#include <platform_session.h>
#include <ring_buffer.h>
#include <workarounds.h>


using namespace Genode;


namespace Igd {

	struct Device_info;
	struct Device;
}


struct Igd::Device_info
{
	enum Platform { UNKNOWN, BROADWELL, SKYLAKE, KABYLAKE, WHISKEYLAKE };
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
	struct Could_not_map_buffer  : Genode::Exception { };

	Env       &_env;
	Allocator &_md_alloc;

	enum { WATCHDOG_TIMEOUT = 1*1000*1000, };

	/*********
	 ** PCI **
	 *********/

	Resources               &_resources;
	Platform::Device_client &_device { _resources.gpu_client() };

	struct Pci_backend_alloc : Utils::Backend_alloc
	{
		Platform::Connection &_pci;

		Pci_backend_alloc(Platform::Connection &pci) : _pci(pci) { }

		Ram_dataspace_capability alloc(size_t size) override
		{
			return _pci.with_upgrade([&] () {
				return _pci.alloc_dma_buffer(size, Genode::UNCACHED); });
		}

		void free(Ram_dataspace_capability cap) override
		{
			if (!cap.valid()) {
				Genode::error("could not free, capability invalid");
				return;
			}

			_pci.free_dma_buffer(cap);
		}

		addr_t dma_addr(Ram_dataspace_capability ds_cap) override
		{
			return _pci.dma_addr(ds_cap);
		}

	} _pci_backend_alloc { _resources.platform() };


	Device_info                    _info          { };
	Gpu::Info_intel::Revision      _revision      { };
	Gpu::Info_intel::Slice_mask    _slice_mask    { };
	Gpu::Info_intel::Subslice_mask _subslice_mask { };
	Gpu::Info_intel::Eu_total      _eus           { };
	Gpu::Info_intel::Subslices     _subslices     { };

	void _pci_info(String<64> const &descr) const
	{
		using namespace Genode;

		uint16_t const vendor_id = _device.vendor_id();
		uint16_t const device_id = _device.device_id();

		uint8_t bus = 0, dev = 0, fun = 0;
		_device.bus_address(&bus, &dev, &fun);

		log("Found: '", descr, "' gen=", _info.generation,
		    " rev=", _revision.value, " ",
		    "[", Hex(vendor_id), ":", Hex(device_id), "] (",
		    Hex(bus, Hex::OMIT_PREFIX), ":",
		    Hex(dev, Hex::OMIT_PREFIX), ".",
		    Hex(fun, Hex::OMIT_PREFIX), ")");

		enum { PCI_NUM_RES = 6 };
		for (int i = 0; i < PCI_NUM_RES; i++) {

			using Resource = Platform::Device::Resource;

			Resource const resource = _device.resource(i);

			if (resource.type() == Resource::INVALID) { continue; }

			log("  Resource ", i, " "
			    "(", resource.type() == Resource::IO ? "I/O" : "MEM", "): "
			    "base=", Genode::Hex(resource.base()), " "
			    "size=", Genode::Hex(resource.size()), " ",
			    (resource.prefetchable() ? "prefetchable" : ""));
		}
	}

	bool _supported(Xml_node &supported)
	{
		bool found = false;

		supported.for_each_sub_node("device", [&] (Xml_node node) {
			if (found)
				return;

			uint16_t   const vendor     = node.attribute_value("vendor", 0U);
			uint16_t   const device     = node.attribute_value("device", 0U);
			uint8_t    const generation = node.attribute_value("generation", 0U);
			String<16> const platform   = node.attribute_value("platform", String<16>("unknown"));
			String<64> const desc       = node.attribute_value("description", String<64>("unknown"));

			if (vendor != 0x8086 /* Intel */ || generation < 8)
				return;

			struct Igd::Device_info const info {
				.id          = device,
				.generation  = generation,
				.platform    = platform_type(platform),
				.features    = 0
			};

			if (info.platform == Igd::Device_info::Platform::UNKNOWN)
				return;

			if (info.id == _device.device_id()) {
				_info = info;
				_revision.value = _resources.config_read<uint8_t>(8);
				_pci_info(desc.string());

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
		return Igd::Device_info::UNKNOWN;
	}

	/**********
	 ** GGTT **
	 **********/

	Genode::Constructible<Igd::Ggtt> _ggtt { };

	/**********
	 ** MMIO **
	 **********/

	Igd::Mmio &_mmio { _resources.mmio() };

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
		Region_map_client _rm;

		Ggtt_mmio_mapping(Resources &resources, Ggtt::Offset offset, size_t size)
		:
			_rm(resources.rm().create(size))
		{
			_rm.attach_at(resources.gmadr_ds(), 0, size, offset * PAGE_SIZE);
			Ggtt::Mapping::cap    = _rm.dataspace();
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
			                                      _resources, offset, size);
		for (size_t i = 0; i < size; i += PAGE_SIZE) {
			addr_t const pa = phys_addr + i;
			_ggtt->insert_pte(pa, offset + (i / PAGE_SIZE));
		}

		return *mem;
	}

	void unmap_dataspace_ggtt(Genode::Allocator &alloc, Genode::Dataspace_capability cap)
	{
		size_t const num = Genode::Dataspace_client(cap).size() / PAGE_SIZE;

		auto lookup_and_free = [&] (Ggtt_mmio_mapping &m) {
			if (!(m.cap == cap)) { return; }

			_ggtt->remove_pte_range(m.offset, num);
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

		/***************************
		 ** Ring buffer interface **
		 ***************************/

		void               ring_reset() { _ring.reset(); }
		Ring_buffer::Index ring_tail() const { return _ring.tail(); }
		Ring_buffer::Index ring_head() const { return _ring.head(); }
		Ring_buffer::Index ring_append(Igd::Cmd_header cmd) { return _ring.append(cmd); }

		bool ring_avail(Ring_buffer::Index num) const { return _ring.avail(num); }
		Ring_buffer::Index ring_max()   const { return _ring.max(); }
		void ring_reset_and_fill_zero() { _ring.reset_and_fill_zero(); }
		void ring_update_head(Ring_buffer::Index head) { _ring.update_head(head); }

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
			offset(device._ggtt->find_free(pages, true)),
			skip(skip_pages),
			ram_ds(device, device._alloc_dataspace(pages * PAGE_SIZE)),
			map(device, *this, alloc),
			ram(rm, map.map.cap)
		{ }
	};

	template <typename CONTEXT>
	struct Engine
	{
		enum {
			CONTEXT_PAGES = CONTEXT::CONTEXT_PAGES,
			RING_PAGES    = CONTEXT::RING_PAGES,
		};

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
			                        _device._subslices);
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

		bool setup_ring_buffer(Gpu::addr_t const buffer_addr)
		{
			_current_seqno++;

			Execlist &el = *rcs.execlist;

			Ring_buffer::Index advance = 0;

			bool dc_flush_wa = _device.match(Device_info::Platform::KABYLAKE,
			                                 Device_info::Stepping::A0,
			                                 Device_info::Stepping::B0);

			size_t const need = 4 /* batchbuffer cmd */ + 6 /* prolog */
			                  + ((_device.generation().value == 9) ? 6 : 0)
			                  + ((_device.generation().value == 8) ? 20 : 22) /* epilog + w/a */
			                  + (dc_flush_wa ? 12 : 0);
			if (!el.ring_avail(need)) { el.ring_reset_and_fill_zero(); }

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
			if (_device.generation().value == 9) {
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
				enum { CMD_NUM = 6, HWS_DATA = 0xc0, };
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
			 * batch-buffer commands
			 */
			if (_device.generation().value == 8)
			{
				enum { CMD_NUM = 4 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Mi_batch_buffer_start mi;

				cmd[0] = Mi_arb_on_off(false).value;
				cmd[1] = mi.value;
				cmd[2] = buffer_addr & 0xffffffff;
				cmd[3] = (buffer_addr >> 32) & 0xffff;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

			/*
			 * gen8_emit_bb_start, gen9
			 *
			 * batch-buffer commands
			 */
			if (_device.generation().value >= 9)
			{
				enum { CMD_NUM = 6 };
				Genode::uint32_t cmd[CMD_NUM] = {};
				Igd::Mi_batch_buffer_start mi;

				cmd[0] = Mi_arb_on_off(true).value;
				cmd[1] = mi.value;
				cmd[2] = buffer_addr & 0xffffffff;
				cmd[3] = (buffer_addr >> 32) & 0xffff;
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
				enum { CMD_NUM = 6, HWS_DATA = 0xc0, };
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
				cmd[2] = HWS_DATA;
				cmd[3] = 0; /* upper addr 0 */
				cmd[4] = _current_seqno & 0xffffffff;
				cmd[5] = _current_seqno >> 32;

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
				enum { CMD_NUM = 2 };
				Genode::uint32_t cmd[2] = {};
				Igd::Mi_user_interrupt ui;
				cmd[0] = ui.value;
				cmd[1] = Mi_arb_on_off(true).value;

				for (size_t i = 0; i < CMD_NUM; i++) {
					advance += el.ring_append(cmd[i]);
				}
			}

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

		if (_mmio.read<Igd::Mmio::EXECLIST_STATUS_RSCUNIT::Execlist_0_valid>() ||
		    _mmio.read<Igd::Mmio::EXECLIST_STATUS_RSCUNIT::Execlist_1_valid>())
			return;


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

	Vgpu *_unschedule_current_vgpu()
	{
		Vgpu *result = nullptr;
		_vgpu_list.dequeue([&] (Vgpu &head) {
			result = &head; });
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

		_submit_execlist(rcs);

		_active_vgpu = gpu;
		_resources.timer().trigger_once(WATCHDOG_TIMEOUT);
	}

	/**********
	 ** INTR **
	 **********/

	void _clear_rcs_iir(Mmio::GT_0_INTERRUPT_IIR::access_t const v)
	{
		_mmio.write_post<Mmio::GT_0_INTERRUPT_IIR>(v);
	}

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

	void _handle_watchdog_timeout()
	{
		if (!_active_vgpu) { return; }

		Genode::error("watchdog triggered: engine stuck,"
		              " vGPU=", _active_vgpu->id());
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

		_device_reset_and_init();

		if (_active_vgpu == _current_vgpu()) {
			_unschedule_current_vgpu();
		}

		if (_current_vgpu()) {
			_schedule_current_vgpu();
		}
	}

	Genode::Signal_handler<Device> _watchdog_timeout_sigh {
		_env.ep(), *this, &Device::_handle_watchdog_timeout };

	void _device_reset_and_init()
	{
		_mmio.reset(_info.generation);
		_mmio.clear_errors();
		_mmio.init();
		_mmio.enable_intr();
	}

	/**
	 * Constructor
	 */
	Device(Genode::Env                 &env,
	       Genode::Allocator           &alloc,
	       Resources                   &resources,
	       Genode::Xml_node            &supported)
	:
		_env(env), _md_alloc(alloc), _resources(resources)
	{
		using namespace Genode;

		if (!_supported(supported)) { throw Unsupported_device(); }

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
		MGGC_0_2_0_PCI::access_t v = _resources.config_read<uint16_t>(PCI_GMCH_CTL);

		{
			log("MGGC_0_2_0_PCI");
			log("  Graphics_mode_select:               ", Hex(MGGC_0_2_0_PCI::Graphics_mode_select::get(v)));
			log("  Gtt_graphics_memory_size:           ", Hex(MGGC_0_2_0_PCI::Gtt_graphics_memory_size::get(v)));
			log("  Versatile_acceleration_mode_enable: ", Hex(MGGC_0_2_0_PCI::Versatile_acceleration_mode_enable::get(v)));
			log("  Igd_vga_disable:                    ", Hex(MGGC_0_2_0_PCI::Igd_vga_disable::get(v)));
			log("  Ggc_lock:                           ", Hex(MGGC_0_2_0_PCI::Ggc_lock::get(v)));
		}

		/* map PCI resources */
		addr_t gttmmadr_base = _resources.map_gttmmadr();

		/* GGTT */
		addr_t const scratch_page = _resources.scratch_page();

		/* reserverd size for framebuffer */
		size_t const aperture_reserved = resources.gmadr_platform_size();

		size_t const ggtt_size = (1u << MGGC_0_2_0_PCI::Gtt_graphics_memory_size::get(v)) << 20;
		addr_t const ggtt_base = gttmmadr_base + (_resources.gttmmadr_size() / 2);
		size_t const gmadr_size = _resources.gmadr_size();
		_ggtt.construct(_mmio, ggtt_base, ggtt_size, gmadr_size, scratch_page, aperture_reserved);
		_ggtt->dump();

		_vgpu_avail = (gmadr_size - aperture_reserved) / Vgpu::APERTURE_SIZE;

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
			Genode::error("unsupported platform ", (int)_info.platform);

		/* apply generation specific workarounds */
		apply_workarounds(_mmio, _info.generation);

		_resources.timer().sigh(_watchdog_timeout_sigh);
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
	void reset() { _device_reset_and_init(); }

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

	addr_t hw_status_page() const { return _hw_status_ctx->gmaddr(); }

	uint64_t seqno() const
	{
		Utils::clflush((uint32_t*)(_hw_status_ctx->vaddr() + 0xc0));
		return *(uint32_t*)(_hw_status_ctx->vaddr() + 0xc0);
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
		bool result = false;
		_vgpu_list.head([&] (Vgpu const &curr) {
			result = (&vgpu == &curr);
		});
		return result;
	}

	/*********************
	 ** Buffer handling **
	 *********************/

	/**
	 * Allocate DMA buffer
	 *
	 * \param guard  resource allocator and guard
	 * \param size   size of the DMA buffer
	 *
	 * \return DMA buffer capability
	 *
	 * \throw Out_of_memory
	 */
	Genode::Ram_dataspace_capability alloc_buffer(Allocator &,
	                                              size_t const size)
	{
		return _pci_backend_alloc.alloc(size);
	}


	/**
	 * Get physical address for DMA buffer
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
	 * Free DMA buffer
	 *
	 * \param guard  resource allocator and guard
	 * \param cap    DMA buffer capability
	 */
	void free_buffer(Allocator &,
	                 Dataspace_capability const cap)
	{
		if (!cap.valid()) { return; }

		_pci_backend_alloc.free(Genode::static_cap_cast<Genode::Ram_dataspace>(cap));
	}

	/**
	 * Map DMA buffer in the GGTT
	 *
	 * \param guard     resource allocator and guard
	 * \param cap       DMA buffer capability
	 * \param aperture  true if mapping should be accessible by CPU
	 *
	 * \return GGTT mapping
	 *
	 * \throw Could_not_map_buffer
	 */
	Ggtt::Mapping const &map_buffer(Genode::Allocator &guard,
	                                Genode::Ram_dataspace_capability cap,
	                                bool aperture)
	{
		if (aperture == false) {
			error("GGTT mapping outside aperture");
			throw Could_not_map_buffer();
		}

		size_t const size = Genode::Dataspace_client(cap).size();
		size_t const num = size / PAGE_SIZE;
		Ggtt::Offset const offset = _ggtt->find_free(num, aperture);
		return map_dataspace_ggtt(guard, cap, offset);
	}

	/**
	 * Unmap DMA buffer from GGTT
	 *
	 * \param guard    resource allocator and guard
	 * \param mapping  GGTT mapping
	 */
	void unmap_buffer(Genode::Allocator &guard, Ggtt::Mapping mapping)
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

	unsigned handle_irq()
	{
		Mmio::MASTER_INT_CTL::access_t master = _mmio.read<Mmio::MASTER_INT_CTL>();

		/* handle render interrupts only */
		if (Mmio::MASTER_INT_CTL::Render_interrupts_pending::get(master) == 0)
			return master;

		_mmio.disable_master_irq();

		Mmio::GT_0_INTERRUPT_IIR::access_t const v = _mmio.read<Mmio::GT_0_INTERRUPT_IIR>();

		bool const ctx_switch    = Mmio::GT_0_INTERRUPT_IIR::Cs_ctx_switch_interrupt::get(v);
		bool const user_complete = Mmio::GT_0_INTERRUPT_IIR::Cs_mi_user_interrupt::get(v);

		if (v) { _clear_rcs_iir(v); }

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
			_active_vgpu = nullptr;

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

		return master;
	}

	void enable_master_irq() { _mmio.enable_master_irq(); }

	Resources &resources() { return _resources; }

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


struct Gpu::Buffer : Genode::Interface
{
	GENODE_RPC_INTERFACE();
};


class Gpu::Session_component : public Genode::Session_object<Gpu::Session>
{
	private:

		Genode::Env              &_env;
		Genode::Region_map       &_rm;
		Constrained_ram_allocator _ram;
		Heap                      _heap { _ram, _rm };
		Capability<Gpu::Session>  _session_cap { cap() };

		Igd::Device       &_device;
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
			bool avail_caps() { return _cap_quota_guard.have_avail(Cap_quota { 5 }); }

			/* size + possible heap allocations */
			bool avail_ram(size_t size = 0) {
				return _ram_quota_guard.have_avail(Ram_quota { size + 2*1024*1024 }); }

			void withdraw(size_t caps, size_t ram)
			{
				try {
					_cap_quota_guard.withdraw(Cap_quota { caps });
					_ram_quota_guard.withdraw(Ram_quota { ram });
				} catch (... /* intentional catch-all */) {
					/*
					 * At this point something in the accounting went wrong
					 * and as quick-fix let the client abort rather than the
					 * multiplexer.
					 */
					throw Service_denied();
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
		 * Buffer managed by session ep
		 */
		struct Buffer : Rpc_object<Gpu::Buffer>
		{
			Ram_dataspace_capability ds_cap;
			Session_capability       owner_cap;

			enum { INVALID_FENCE = 0xff };
			Genode::uint32_t fenced { INVALID_FENCE };

			Igd::Ggtt::Mapping map { };

			addr_t phys_addr { 0 };
			size_t size { 0 };

			bool   caps_used { false };
			size_t ram_used { 0 };

			Buffer(Ram_dataspace_capability ds_cap, Genode::addr_t phys_addr,
			       Session_capability  owner_cap)
			:
				ds_cap { ds_cap }, owner_cap { owner_cap },
				phys_addr { phys_addr }
			{
				Dataspace_client buf(ds_cap);
				size = buf.size();
			}

			bool owner(Capability<Gpu::Session> other)
			{
				return owner_cap == other;
			}
		};

		/*
		 * Buffer session/gpu-context local buffer
		 */
		struct Buffer_local
		{
			using Id_space = Genode::Id_space<Buffer_local>;

			Buffer_capability const buffer_cap;
			size_t                  size;
			Id_space::Element const elem;
			addr_t            ppgtt_va { 0 };
			bool              ppgtt_va_valid { false };

			Buffer_local(Buffer_capability buffer_cap, size_t size,
			             Id_space &space, Buffer_id id)
			: buffer_cap(buffer_cap), size(size),
			  elem(*this, space, Id_space::Id { .value = id.value })
			{ }
		};

		Id_space<Buffer_local> _buffer_space { };

		template <typename FN>
		void _apply_buffer(Buffer_local &buffer_local, FN const &fn)
		{
			Buffer *b = nullptr;
			bool free = _env.ep().rpc_ep().apply(buffer_local.buffer_cap, [&] (Buffer *buffer) {
				if (buffer) {
					b = buffer;
					return fn(*buffer);
				}
				return false;
			});

			if (b && free)
				destroy(&_heap, b);
		}

		bool _buffer_valid(Buffer_capability buffer_cap)
		{
			bool valid = false;
			_env.ep().rpc_ep().apply(buffer_cap, [&] (Buffer *buffer) {
				if (buffer) valid = true;
			});

			return valid;
		}

		template <typename FN>
		void _apply_buffer_local(Gpu::Buffer_id id, FN const &fn)
		{
			Buffer_local::Id_space::Id local_id { .value = id.value };
			_buffer_space.apply<Buffer_local>(local_id, [&] (Buffer_local &buffer) {
				fn(buffer);
			});
		}

		Genode::uint64_t seqno { 0 };

		void _free_local_buffer(Buffer_local &buffer_local)
		{
			if (buffer_local.ppgtt_va_valid) {
				_vgpu.rcs_unmap_ppgtt(buffer_local.ppgtt_va, buffer_local.size);
			}

			destroy(&_heap, &buffer_local);
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
			auto lookup_and_free = [&] (Buffer_local &buffer_local) {

				_apply_buffer(buffer_local, [&](Buffer &buffer) {

					if (buffer.owner(_session_cap) == false) return false;

					if (buffer.map.offset != Igd::Ggtt::Mapping::INVALID_OFFSET) {
						_device.unmap_buffer(_heap, buffer.map);
					}

					if (buffer.fenced != Buffer::INVALID_FENCE) {
						_device.clear_tiling(buffer.fenced);
						_vgpu.active_fences--;
					}

					_env.ep().dissolve(buffer);
					_device.free_buffer(_heap, buffer.ds_cap);
					return true;
				});

				_free_local_buffer(buffer_local);
			};

			while(_buffer_space.apply_any<Buffer_local>(
				[&] (Buffer_local &buffer_local) { lookup_and_free(buffer_local); }));
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

		/***************************
		 ** Gpu session interface **
		 ***************************/

		Genode::Dataspace_capability info_dataspace() const override
		{
			return _vgpu.info_dataspace();
		}

		Gpu::Sequence_number exec_buffer(Buffer_id id,
		                                 Genode::size_t) override
		{
			bool found = false;

			_apply_buffer_local(id, [&] (Buffer_local &buffer_local) {

				if (_buffer_valid(buffer_local.buffer_cap) == false) {
					_free_local_buffer(buffer_local);
					return;
				}

				if (!buffer_local.ppgtt_va_valid) {
					Genode::error("Invalid execbuffer");
					Genode::Signal_transmitter(_vgpu.completion_sigh()).submit();
					throw Gpu::Session::Invalid_state();
				}

				found = _vgpu.setup_ring_buffer(buffer_local.ppgtt_va);
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

		Genode::Dataspace_capability alloc_buffer(Gpu::Buffer_id id,
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

			Ram_dataspace_capability ds_cap = _device.alloc_buffer(_heap, size);
			addr_t phys_addr = _device.dma_addr(ds_cap);
			Buffer *buffer = new (&_heap) Buffer(ds_cap, phys_addr, _session_cap);
			_env.ep().manage(*buffer);

			try {
				new (&_heap) Buffer_local(buffer->cap(), size, _buffer_space, id);
			} catch (Id_space<Buffer_local>::Conflicting_id) {
				_env.ep().dissolve(*buffer);
				destroy(&_heap, buffer);
				_device.free_buffer(_heap, ds_cap);
				throw Gpu::Session_component::Conflicting_id();
			}

			size_t caps_after = _env.pd().avail_caps().value;
			size_t ram_after  = _env.pd().avail_ram().value;

			/* limit to buffer size */
			buffer->ram_used = min(ram_before - ram_after, size);
			buffer->caps_used = (caps_before - caps_after) > 0;

			_resource_guard.withdraw(caps_before - caps_after, buffer->ram_used);

			return ds_cap;
		}

		void free_buffer(Gpu::Buffer_id id) override
		{
			auto lookup_and_free = [&] (Buffer_local &buffer_local) {

				_apply_buffer(buffer_local, [&](Buffer &buffer) {

					if (buffer.owner(_session_cap) == false) return false;

					if (buffer.map.offset != Igd::Ggtt::Mapping::INVALID_OFFSET) {
						Genode::error("cannot free mapped buffer");
						/* XXX throw */
						return false;
					}
					_env.ep().dissolve(buffer);
					_device.free_buffer(_heap, buffer.ds_cap);
					_resource_guard.replenish(buffer.caps_used ? 1 : 0,
					                          buffer.ram_used);
					return true;
				});

				_free_local_buffer(buffer_local);
			};

			_apply_buffer_local(id, lookup_and_free);
		}

		Buffer_capability export_buffer(Buffer_id id) override
		{
			Buffer_capability cap { };
			_apply_buffer_local(id, [&] (Buffer_local &buffer_local) {
				if (_buffer_valid(buffer_local.buffer_cap))
					cap = buffer_local.buffer_cap;
			});
			return cap;
		}

		void import_buffer(Buffer_capability cap, Buffer_id id) override
		{
			if (_buffer_valid(cap) == false)
				throw Gpu::Session::Invalid_state();

			try {
				Buffer_local *buffer_local = new (_heap) Buffer_local(cap, 0, _buffer_space, id);

				_apply_buffer(*buffer_local, [&](Buffer &buffer) {
					buffer_local->size = buffer.size; return false; });

			} catch (Id_space<Buffer_local>::Conflicting_id) {
				throw Gpu::Session::Conflicting_id();
			} catch (Cap_quota_guard::Limit_exceeded) {
				throw Gpu::Session::Out_of_caps();
			} catch (Ram_quota_guard::Limit_exceeded) {
				throw Gpu::Session::Out_of_ram();
			}
		}

		Genode::Dataspace_capability map_buffer(Gpu::Buffer_id id,
		                                        bool aperture,
		                                        Gpu::Mapping_attributes attrs) override
		{
			/* treat GGTT mapped buffers as rw */
			if (!(attrs.readable && attrs.writeable))
				return Genode::Dataspace_capability();

			Genode::Dataspace_capability map_cap;

			auto lookup_and_map = [&] (Buffer &buffer) {
				if (buffer.owner(_session_cap) == false) {
					Genode::error("GGTT mappings can only be done by buffer owner");
					return false;
				}

				if (buffer.map.offset != Igd::Ggtt::Mapping::INVALID_OFFSET) {
					Genode::error("buffer already mapped");
					return false;
				}
				/* GGTT mappings only require the heap */
				if (_resource_guard.avail_caps() == false)
					throw Gpu::Session::Out_of_caps();

				if (_resource_guard.avail_ram() == false)
					throw Gpu::Session::Out_of_ram();

				size_t caps_before = _env.pd().avail_caps().value;
				size_t ram_before  = _env.pd().avail_ram().value;

				Igd::Ggtt::Mapping const &map =
					_device.map_buffer(_heap, buffer.ds_cap, aperture);
				buffer.map.cap    = map.cap;
				buffer.map.offset = map.offset;
				map_cap           = buffer.map.cap;

				size_t caps_after = _env.pd().avail_caps().value;
				size_t ram_after  = _env.pd().avail_ram().value;
				_resource_guard.withdraw(caps_before - caps_after,
				                         ram_before - ram_after);
				return true;
			};

			_apply_buffer_local(id, [&] (Buffer_local &buffer_local) {
				_apply_buffer(buffer_local, lookup_and_map); });

			return map_cap;
		}

		void unmap_buffer(Gpu::Buffer_id id) override
		{
			bool unmapped = false;

			auto lookup_and_unmap = [&] (Buffer &buffer) {

				if (buffer.owner(_session_cap) == false) {
					Genode::error("GGTT unmappings can only be done by buffer owner");
					return false;
				}

				if (!buffer.map.cap.valid()) { return false; }

				if (buffer.fenced != Buffer::INVALID_FENCE) {
					_device.clear_tiling(buffer.fenced);
					_vgpu.active_fences--;
				}

				_device.unmap_buffer(_heap, buffer.map);
				buffer.map.offset = Igd::Ggtt::Mapping::INVALID_OFFSET;
				unmapped = true;

				return false;
			};

			_apply_buffer_local(id, [&](Buffer_local &buffer) {
				_apply_buffer(buffer, lookup_and_unmap); });

			if (!unmapped) { Genode::error("buffer not mapped"); }
		}

		bool map_buffer_ppgtt(Gpu::Buffer_id id, Gpu::addr_t va) override
		{
			auto lookup_and_map = [&] (Buffer_local &buffer_local) {

				if (buffer_local.ppgtt_va_valid) {
					Genode::error("buffer already mapped");
					return;
				}

				addr_t phys_addr = 0;
				_apply_buffer(buffer_local, [&](Buffer &buffer) {
					phys_addr = buffer.phys_addr; return false; });

				if (phys_addr == 0) {
					_free_local_buffer(buffer_local);
					return;
				}

				_vgpu.rcs_map_ppgtt(va, phys_addr, buffer_local.size);
				buffer_local.ppgtt_va = va;
				buffer_local.ppgtt_va_valid = true;
			};

			if (_resource_guard.avail_caps() == false)
				throw Gpu::Session::Out_of_caps();

			if (_resource_guard.avail_ram() == false)
				throw Gpu::Session::Out_of_ram();

			size_t caps_before = _env.pd().avail_caps().value;
			size_t ram_before  = _env.pd().avail_ram().value;

			_apply_buffer_local(id, lookup_and_map);

			size_t caps_after = _env.pd().avail_caps().value;
			size_t ram_after  = _env.pd().avail_ram().value;

			_resource_guard.withdraw(caps_before - caps_after,
			                         ram_before - ram_after);

			return true;
		}

		void unmap_buffer_ppgtt(Gpu::Buffer_id id,
		                        Gpu::addr_t va) override
		{
			auto lookup_and_unmap = [&] (Buffer_local &buffer_local) {

				if (!buffer_local.ppgtt_va_valid) {
					Genode::error("buffer not mapped");
					return;
				}

				if (buffer_local.ppgtt_va != va) {
					Genode::error("buffer not mapped at ", Genode::Hex(va));
					return;
				}

				_vgpu.rcs_unmap_ppgtt(va, buffer_local.size);
				buffer_local.ppgtt_va_valid = false;
			};
			_apply_buffer_local(id, lookup_and_unmap);
		}

		Gpu::addr_t query_buffer_ppgtt(Gpu::Buffer_id id) override
		{
			Gpu::addr_t result = (Gpu::addr_t)-1;

			auto lookup_va = [&] (Buffer_local &buffer_local) {

				if (!buffer_local.ppgtt_va_valid) {
					Genode::error("buffer not mapped");
					return;
				}

				result = buffer_local.ppgtt_va;
			};
			_apply_buffer_local(id, lookup_va);
			return result;
		}

		bool set_tiling(Gpu::Buffer_id id,
		                Genode::uint32_t const mode) override
		{
			if (_vgpu.active_fences > Igd::Device::Vgpu::MAX_FENCES) {
				Genode::error("no free fences left, already active: ", _vgpu.active_fences);
				return false;
			}

			Buffer *b = nullptr;
			auto lookup = [&] (Buffer &buffer) {
				if (!buffer.map.cap.valid() || !buffer.owner(_session_cap)) { return false; }
				b = &buffer;
				return false;
			};

			_apply_buffer_local(id, [&](Buffer_local &buffer_local) {
				_apply_buffer(buffer_local, lookup);
			});

			if (b == nullptr) {
				Genode::error("attempt to set tiling for non-mapped or non-owned buffer");
				return false;
			}

			//XXX: support change of already fenced bo's fencing mode
			if (b->fenced) return true;

			Igd::size_t const size = b->size;
			Genode::uint32_t const fenced = _device.set_tiling(b->map.offset, size, mode);

			b->fenced = fenced;
			if (fenced != Buffer::INVALID_FENCE) { _vgpu.active_fences++; }
			return fenced != Buffer::INVALID_FENCE;
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
					Session_component(_env, _env.ep(), _env.ram(), _env.rm(),
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
			}
			Genode::destroy(md_alloc(), s);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &alloc)
		: Root_component(env.ep(), alloc), _env(env), _device(nullptr) { }

		void manage(Igd::Device &device) { _device = &device; }
};


struct Main
{
	Genode::Env &_env;

	/*********
	 ** Gpu **
	 *********/

	Genode::Sliced_heap            _root_heap  { _env.ram(), _env.rm() };
	Gpu::Root                      _gpu_root   { _env, _root_heap };
	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	Genode::Heap                       _device_md_alloc { _env.ram(), _env.rm() };
	Genode::Constructible<Igd::Device> _device { };
	Igd::Resources                     _gpu_resources { _env, _device_md_alloc,
	                                                   *this, &Main::ack_irq };

	Genode::Irq_session_client     _irq { _gpu_resources.gpu_client().irq(0) };
	Genode::Signal_handler<Main>   _irq_dispatcher {
		_env.ep(), *this, &Main::handle_irq };

	Constructible<Platform::Root> _platform_root { };

	Genode::Signal_handler<Main> _config_sigh {
		_env.ep(), *this, &Main::_handle_config_update };

	Main(Genode::Env &env)
	:
		_env(env)
	{
		_config_rom.sigh(_config_sigh);

		/* IRQ */
		_irq.sigh(_irq_dispatcher);
		_irq.ack_irq();

		/* GPU */
		_handle_config_update();

		/* platform service */
		_platform_root.construct(_env, _device_md_alloc, _gpu_resources);
	}

	void _handle_config_update()
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		if (_device.constructed()) {
			Genode::log("gpu device already initialized - ignore");
			return;
		}

		try {
			_device.construct(_env, _device_md_alloc, _gpu_resources, _config_rom.xml());
			_gpu_root.manage(*_device);
			_env.parent().announce(_env.ep().manage(_gpu_root));
		} catch (Igd::Device::Unsupported_device) {
			Genode::warning("No supported Intel GPU detected - no GPU service");
		} catch (...) {
			Genode::error("Unknown error occurred - no GPU service");
		}
	}

	void handle_irq()
	{
		unsigned master = 0;
		if (_device.constructed())
			master = _device->handle_irq();
		/* GPU not present forward all IRQs to platform client */
		else {
			_platform_root->handle_irq();
			return;
		}

		/*
		 * GPU present check for display engine related IRQs before calling platform
		 * client
		 */
		using Master = Igd::Mmio::MASTER_INT_CTL;
		if (Master::De_interrupts_pending::get(master) &&
		    (_platform_root->handle_irq()))
			return;

		ack_irq();
	}

	void ack_irq()
	{
		if (_device.constructed()) {
			_device->enable_master_irq();
		}

		_irq.ack_irq();
	}
};


void Component::construct(Genode::Env &env)
{
	static Constructible<Main> main;
	try {
		main.construct(env);
	} catch (Igd::Resources::Initialization_failed) {
		Genode::warning("Intel GPU resources not found.");
		env.parent().exit(0);
	}
}


Genode::size_t Component::stack_size() { return 32UL*1024*sizeof(long); }
