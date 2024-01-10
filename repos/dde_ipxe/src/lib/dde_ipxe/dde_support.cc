/*
 * \brief  DDE iPXE wrappers to C++ backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2010-10-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*
 * The build system picks up stdarg.h from iPXE instead of the one
 * provided by GCC. This header contains the FILE_LICENCE macro which
 * is defined by iPXE's compiler.h.
 */
#define FILE_LICENCE(x)

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/registry.h>
#include <base/log.h>
#include <base/slab.h>
#include <dataspace/client.h>
#include <io_mem_session/connection.h>
#include <io_port_session/connection.h>
#include <irq_session/connection.h>
#include <platform_session/device.h>
#include <platform_session/dma_buffer.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <timer_session/connection.h>
#include <util/misc_math.h>
#include <util/retry.h>
#include <util/reconstructible.h>

/* format-string includes */
#include <format/snprintf.h>

/* local includes */
#include <dde_support.h>

/* DDE support includes */
#include <dde_ipxe/support.h>

using namespace Genode;

static Genode::Env        *_global_env;
static Genode::Allocator  *_global_alloc;


void dde_support_init(Genode::Env &env, Genode::Allocator &alloc)
{
	_global_env   = &env;
	_global_alloc = &alloc;
}


/**************************
 ** Initialization check **
 **************************/

extern "C" int dde_support_initialized(void)
{
	return !!_global_env;
}


/************
 ** printf **
 ************/

extern "C" void dde_vprintf(const char *format, va_list list)
{
	using namespace Genode;

	char buf[128] { };
	Format::String_console(buf, sizeof(buf)).vprintf(format, list);
	log(Cstring(buf));
}


extern "C" void dde_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dde_vprintf(fmt, args);
	va_end(args);
}


/***********
 ** Timer **
 ***********/

static Timer::Connection & timer()
{
	static Timer::Connection _timer { *_global_env };
	return _timer;
}


extern "C" void dde_udelay(unsigned long usecs)
{
	/*
	 * This function is called only once during rdtsc calibration (usecs will be
	 * 10000, see dde.c 'udelay'.
	 */
	timer().usleep(usecs);
}


/***************************
 ** locking/synchronizing **
 ***************************/

/**
 * DDE iPXE mutual exclusion lock
 */
static Genode::Mutex & ipxe_mutex()
{
	static Genode::Mutex _ipxe_mutex;
	return _ipxe_mutex;
}

extern "C" void dde_lock_enter(void) { ipxe_mutex().acquire(); }
extern "C" void dde_lock_leave(void) { ipxe_mutex().release(); }


extern "C" void dde_mdelay(unsigned long msecs)
{
	/*
	 * This function is only called while initializing the device
	 * and only be the same thread.
	 */
	timer().msleep(msecs);
}


/******************
 ** PCI handling **
 ******************/

struct Range { addr_t start; size_t size; };

using Io_mem  = Platform::Device::Mmio<0>;
using Io_port = Platform::Device::Io_port_range;

struct Pci_driver
{
	enum { BACKING_STORE_SIZE = 1024 * 1024 };

	Env                  & _env;
	Heap                   _heap    { _env.ram(), _env.rm() };
	Platform::Connection   _pci     { _env };
	Platform::Device       _dev     { _pci };
	Platform::Device::Irq  _irq     { _dev };
	Platform::Dma_buffer   _dma     { _pci, BACKING_STORE_SIZE, CACHED };
	Constructible<Io_mem>  _mmio    {};
	Constructible<Io_port> _io_port {};

	Io_signal_handler<Pci_driver> _io_handler { _env.ep(), *this,
	                                            &Pci_driver::_irq_handle };

	typedef void (*irq_handler)(void*);
	irq_handler   _irq_handler { nullptr };
	void        * _irq_data    { nullptr };

	String<16>       _name;
	dde_pci_device_t _pci_info;

	void _device_handle() {} /* just ignore changes here */

	void _irq_handle()
	{
		if (_irq_handler) _irq_handler(_irq_data);
		_irq.ack();
	}

	Pci_driver(Genode::Env &env) : _env(env)
	{
		_pci.update();
		_pci.with_xml([&] (Xml_node node) {
			node.with_optional_sub_node("device", [&] (Xml_node node)
			{
				node.with_optional_sub_node("pci-config", [&] (Xml_node node)
				{
					_name = node.attribute_value("name", String<16>());
					_pci_info.vendor     = node.attribute_value("vendor_id", 0U);
					_pci_info.device     = node.attribute_value("device_id", 0U);
					_pci_info.class_code = node.attribute_value("class", 0U);
					_pci_info.revision   = node.attribute_value("revision", 0U);
					_pci_info.name       = _name.string();
				});

				node.with_optional_sub_node("io_mem", [&] (Xml_node node)
				{
					_mmio.construct(_dev);
					_pci_info.io_mem_addr = (addr_t)_mmio->local_addr<void>();
				});

				node.with_optional_sub_node("io_port", [&] (Xml_node node)
				{
					_io_port.construct(_dev);
					_pci_info.io_port_start = 0x10;
				});
			});
		});

		_irq.sigh(_io_handler);
	}

	template <typename T>
	void config_read(unsigned int devfn, T *val)
	{
		switch (devfn) {
		case 0x4:  /* CMD  */
			*val = 0x7;
			return;
		default:
			*val = 0;
		};
	}

	template <typename T>
	void config_write(unsigned int devfn, T val) { }

	dde_pci_device_t device() { return _pci_info; }

	Range dma() {
		return { (addr_t)_dma.local_addr<void>(), BACKING_STORE_SIZE }; }

	Genode::addr_t virt_to_dma(Genode::addr_t virt) {
		return virt - (addr_t)_dma.local_addr<void>() + _dma.dma_addr(); }

	void irq(irq_handler handler, void * data)
	{
		_irq_handler = handler;
		_irq_data    = data;
	}

	template <typename FN>
	void with_io_port(FN const & fn) {
		if (_io_port.constructed()) fn(*_io_port); }
};


static Pci_driver& pci_drv()
{
	static Pci_driver _pci_drv { *_global_env };
	return _pci_drv;
}


extern "C" dde_pci_device_t dde_pci_device() { return pci_drv().device(); }


extern "C" void dde_pci_readb(int pos, dde_uint8_t *val) {
	pci_drv().config_read(pos, val); }


extern "C" void dde_pci_readw(int pos, dde_uint16_t *val) {
	pci_drv().config_read(pos, val); }


extern "C" void dde_pci_readl(int pos, dde_uint32_t *val) {
	pci_drv().config_read(pos, val); }


extern "C" void dde_pci_writeb(int pos, dde_uint8_t val) {
	pci_drv().config_write(pos, val); }


extern "C" void dde_pci_writew(int pos, dde_uint16_t val) {
	pci_drv().config_write(pos, val); }


extern "C" void dde_pci_writel(int pos, dde_uint32_t val) {
	pci_drv().config_write(pos, val); }


/************************
 ** Interrupt handling **
 ************************/

extern "C" void dde_interrupt_attach(void(*handler)(void *), void *priv) {
	pci_drv().irq(handler, priv); }


/***************************************************
 ** Support for aligned and DMA memory allocation **
 ***************************************************/

struct Backing_store
{
	Genode::Allocator_avl _avl;

	Backing_store (Genode::Allocator &alloc) : _avl(&alloc)
	{
		Range r = pci_drv().dma();
		_avl.add_range(r.start, r.size);
	}
};


static Genode::Allocator_avl& allocator()
{
	static Backing_store _instance { *_global_alloc };
	return _instance._avl;

}


extern "C" void *dde_dma_alloc(dde_size_t size, dde_size_t align,
                                    dde_size_t offset)
{
	return allocator().alloc_aligned(size, Genode::log2(align)).convert<void *>(

		[&] (void *ptr) { return ptr; },

		[&] (Genode::Range_allocator::Alloc_error) -> void * {
			Genode::error("memory allocation failed in alloc_memblock ("
			              "size=",   size, " "
			              "align=",  Genode::Hex(align), " "
			              "offset=", Genode::Hex(offset), ")");
			return nullptr; });
}


extern "C" void dde_dma_free(void *p, dde_size_t size) {
	allocator().free(p, size); }


extern "C" dde_addr_t dde_dma_get_physaddr(void *virt) {
	return pci_drv().virt_to_dma((Genode::addr_t)virt); }


/**************
 ** I/O port **
 **************/

extern "C" dde_uint8_t dde_inb(dde_addr_t port)
{
	dde_uint8_t v;
	pci_drv().with_io_port([&] (Io_port & iop) { v = iop.inb(port); });
	return v;
}

extern "C" dde_uint16_t dde_inw(dde_addr_t port)
{
	dde_uint16_t v;
	pci_drv().with_io_port([&] (Io_port & iop) { v = iop.inw(port); });
	return v;
}

extern "C" dde_uint32_t dde_inl(dde_addr_t port)
{
	dde_uint32_t v;
	pci_drv().with_io_port([&] (Io_port & iop) { v = iop.inl(port); });
	return v;
}


extern "C" void dde_outb(dde_addr_t port, dde_uint8_t data) {
	pci_drv().with_io_port([&] (Io_port & iop) { iop.outb(port, data); }); }

extern "C" void dde_outw(dde_addr_t port, dde_uint16_t data) {
	pci_drv().with_io_port([&] (Io_port & iop) { iop.outw(port, data); }); }

extern "C" void dde_outl(dde_addr_t port, dde_uint32_t data) {
	pci_drv().with_io_port([&] (Io_port & iop) { iop.outl(port, data); }); }


/**********************
 ** Slab memory pool **
 **********************/

struct Slab_backend_alloc : public Genode::Allocator,
                            public Genode::Rm_connection,
                            public Genode::Region_map_client
{
	enum {
		VM_SIZE    = 2 * 1024 * 1024,
		BLOCK_SIZE =       64 * 1024,
		ELEMENTS   = VM_SIZE / BLOCK_SIZE,
	};

	Genode::addr_t                    _base;
	Genode::Ram_dataspace_capability  _ds_cap[ELEMENTS];
	int                               _index;
	Genode::Allocator_avl             _range;
	Genode::Ram_allocator            &_ram;

	struct Extend_ok { };
	using Extend_result = Genode::Attempt<Extend_ok, Alloc_error>;

	Extend_result _extend_one_block()
	{
		using namespace Genode;

		if (_index == ELEMENTS) {
			error("slab backend exhausted!");
			return Alloc_error::DENIED;
		}

		return _ram.try_alloc(BLOCK_SIZE).convert<Extend_result>(

			[&] (Ram_dataspace_capability ds) -> Extend_result {

				_ds_cap[_index] = ds;

				Alloc_error error = Alloc_error::DENIED;

				try {
					Region_map_client::attach_at(_ds_cap[_index],
					                             _index * BLOCK_SIZE,
					                             BLOCK_SIZE, 0);
					/* return base + offset in VM area */
					addr_t block_base = _base + (_index * BLOCK_SIZE);
					++_index;

					_range.add_range(block_base, BLOCK_SIZE);

					return Extend_ok();
				}
				catch (Out_of_ram)  { error = Alloc_error::OUT_OF_RAM; }
				catch (Out_of_caps) { error = Alloc_error::OUT_OF_CAPS; }
				catch (...)         { error = Alloc_error::DENIED; }

				Genode::error("Slab_backend_alloc: local attach_at failed");

				_ram.free(ds);
				_ds_cap[_index] = { };

				return error;
			},

			[&] (Alloc_error e) -> Extend_result {
				error("Slab_backend_alloc: backend allocator exhausted");
				return e; });
	}

	Slab_backend_alloc(Genode::Env &env, Genode::Region_map &rm,
	                   Genode::Ram_allocator &ram,
	                   Genode::Allocator &md_alloc)
	:
		Rm_connection(env),
		Region_map_client(Rm_connection::create(VM_SIZE)),
		_index(0), _range(&md_alloc), _ram(ram)
	{
		/* reserver attach us, anywere */
		_base = rm.attach(dataspace());
	}

	Genode::addr_t start() const { return _base; }
	Genode::addr_t end()   const { return _base + VM_SIZE - 1; }

	/*************************
	 ** Allocator interface **
	 *************************/

	Alloc_result try_alloc(Genode::size_t size) override
	{
		Alloc_result result = _range.try_alloc(size);
		if (result.ok())
			return result;

		return _extend_one_block().convert<Alloc_result>(
			[&] (Extend_ok)         { return _range.try_alloc(size); },
			[&] (Alloc_error error) { return error; });
	}

	void           free(void *addr, Genode::size_t size) { _range.free(addr, size); }
	Genode::size_t overhead(Genode::size_t size) const   { return 0; }
	bool           need_size_for_free() const            { return false; }
};


class Slab_alloc : public Genode::Slab
{
	private:

		Genode::size_t const _object_size;

		static Genode::size_t _calculate_block_size(Genode::size_t object_size)
		{
			Genode::size_t const block_size = 8*object_size;
			return Genode::align_addr(block_size, 12);
		}

	public:

		Slab_alloc(Genode::size_t object_size, Slab_backend_alloc &allocator)
		:
			Slab(object_size, _calculate_block_size(object_size), 0, &allocator),
			_object_size(object_size)
		{ }

		Genode::addr_t alloc()
		{
			return Slab::try_alloc(_object_size).convert<Genode::addr_t>(
				[&] (void *ptr) { return (Genode::addr_t)ptr; },
				[&] (Alloc_error) -> Genode::addr_t { return 0; });
		}

		void free(void *ptr) { Slab::free(ptr, _object_size); }
};


struct Slab
{
	enum {
		SLAB_START_LOG2 = 5,  /* 32 B */
		SLAB_STOP_LOG2  = 13, /* 8 KiB */
		NUM_SLABS = (SLAB_STOP_LOG2 - SLAB_START_LOG2) + 1,
	};

	Genode::Constructible<Slab_alloc> _allocator[NUM_SLABS];

	Genode::addr_t _start;
	Genode::addr_t _end;

	Slab(Slab_backend_alloc &alloc)
	: _start(alloc.start()), _end(alloc.end())
	{
		for (unsigned i = 0; i < NUM_SLABS; i++) {
			_allocator[i].construct(1U << (SLAB_START_LOG2 + i), alloc);
		}
	}

	void *alloc(Genode::size_t size)
	{
		using namespace Genode;
		size += sizeof(Genode::addr_t);

		int msb = Genode::log2(size);

		if (size > (1U << msb))
			msb++;

		if (size < (1U << SLAB_START_LOG2))
			msb = SLAB_STOP_LOG2;

		if (msb > SLAB_STOP_LOG2)
			return 0;

		Genode::addr_t addr =  _allocator[msb - SLAB_START_LOG2]->alloc();
		if (!addr)
			return 0;

		*(Genode::addr_t*)addr = msb - SLAB_START_LOG2;
		addr += sizeof(Genode::addr_t);

		return (void*)addr;
	}

	void free(void *p)
	{
		Genode::addr_t *addr = ((Genode::addr_t *)p)-1;
		unsigned index = *(unsigned*)(addr);
		_allocator[index]->free((void*)(addr));
	}
};


static ::Slab& slab()
{
	static ::Slab_backend_alloc sb(*_global_env, _global_env->rm(),
	                               _global_env->ram(), *_global_alloc);
	static ::Slab s(sb);
	return s;
}


extern "C" void *dde_slab_alloc(dde_size_t size) {
	return slab().alloc(size); }


extern "C" void dde_slab_free(void *p) {
	slab().free(p); }


/****************
 ** I/O memory **
 ****************/

extern "C" int dde_request_iomem(dde_addr_t start, dde_addr_t *vaddr)
{
	/*
	 * We just return the virtual address as physical one,
	 * because io_mem address announced was already a virtual one
	 */
	*vaddr = start;
	return 0;
}


extern "C" int dde_release_iomem(dde_addr_t start, dde_size_t size) { return 0; }
