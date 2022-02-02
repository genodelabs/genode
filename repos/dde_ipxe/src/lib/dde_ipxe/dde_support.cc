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
#include <base/log.h>
#include <base/slab.h>
#include <dataspace/client.h>
#include <io_mem_session/connection.h>
#include <io_port_session/connection.h>
#include <irq_session/connection.h>
#include <legacy/x86/platform_device/client.h>
#include <legacy/x86/platform_session/connection.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <timer_session/connection.h>
#include <util/misc_math.h>
#include <util/retry.h>
#include <util/reconstructible.h>

/* local includes */
#include <dde_support.h>

/* DDE support includes */
#include <dde_ipxe/support.h>


static Genode::Entrypoint *_global_ep;
static Genode::Env        *_global_env;
static Genode::Allocator  *_global_alloc;


void dde_support_init(Genode::Env &env, Genode::Allocator &alloc)
{
	_global_env   = &env;
	_global_ep    = &env.ep();
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
	String_console(buf, sizeof(buf)).vprintf(format, list);
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

struct Pci_driver
{
	enum {
		PCI_BASE_CLASS_NETWORK = 0x02,
		CLASS_MASK             = 0xff0000,
		CLASS_NETWORK          = PCI_BASE_CLASS_NETWORK << 16
	};

	Genode::Region_map &_rm;

	Platform::Connection        _pci;
	Platform::Device_capability _cap;
	Platform::Device_capability _last_cap;

	struct Region
	{
		Genode::addr_t base;
		Genode::addr_t mapped_base;
	} _region;

	template <typename T>
	Platform::Device::Access_size _access_size(T t)
	{
		switch (sizeof(T)) {
		case 1:  return Platform::Device::ACCESS_8BIT;
		case 2:  return Platform::Device::ACCESS_16BIT;
		default: return Platform::Device::ACCESS_32BIT;
		}
	}

	void _bus_address(int *bus, int *dev, int *fun)
	{
		Platform::Device_client client(_cap);
		unsigned char b, d, f;
		client.bus_address(&b, &d, &f);

		*bus = b;
		*dev = d;
		*fun = f;
	}


	Pci_driver(Genode::Env &env, Genode::Region_map &rm)
	: _rm(rm), _pci(env) { }

	template <typename T>
	void config_read(unsigned int devfn, T *val)
	{
		Platform::Device_client client(_cap);
		*val = client.config_read(devfn, _access_size(*val));
	}

	template <typename T>
	void config_write(unsigned int devfn, T val)
	{
		Platform::Device_client client(_cap);

		_pci.with_upgrade([&] () {
			client.config_write(devfn, val, _access_size(val)); });
	}

	int first_device(int *bus, int *dev, int *fun)
	{
		_cap = _pci.with_upgrade([&] () {
			return _pci.first_device(CLASS_NETWORK, CLASS_MASK); });

		if (!_cap.valid())
			return -1;

		_bus_address(bus, dev, fun);
		return 0;
	}

	int next_device(int *bus, int *dev, int *fun)
	{
		int result = -1;

		_last_cap = _cap;
		_cap = _pci.with_upgrade([&] () {
			return _pci.next_device(_cap, CLASS_NETWORK, CLASS_MASK); });

		if (_cap.valid()) {
			_bus_address(bus, dev, fun);
			result = 0;
		}

		if (_last_cap.valid())
			_pci.release_device(_last_cap);

		return result;
	}

	Genode::addr_t alloc_dma_memory(Genode::size_t size)
	{
		try {
			using namespace Genode;

			size_t donate = size;

			Ram_dataspace_capability ram_cap =
				retry<Out_of_ram>(
					[&] () {
						return retry<Out_of_caps>(
							[&] () { return _pci.alloc_dma_buffer(size, UNCACHED); },
							[&] () { _pci.upgrade_caps(2); });
					},
					[&] () {
						_pci.upgrade_ram(donate);
						donate = donate * 2 > size ? 4096 : donate * 2;
					});

			_region.mapped_base = _rm.attach(ram_cap);
			_region.base = _pci.dma_addr(ram_cap);

			return _region.mapped_base;
		} catch (...) {
			Genode::error("failed to allocate dma memory");
			return 0;
		}
	}

	Genode::addr_t virt_to_phys(Genode::addr_t virt) {
		return virt - _region.mapped_base + _region.base; }
};


static Pci_driver& pci_drv()
{
	static Pci_driver _pci_drv { *_global_env , _global_env->rm() };
	return _pci_drv;
}


extern "C" int dde_pci_first_device(int *bus, int *dev, int *fun) {
	return pci_drv().first_device(bus, dev, fun); }


extern "C" int dde_pci_next_device(int *bus, int *dev, int *fun) {
	return pci_drv().next_device(bus, dev, fun); }


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

struct Irq_handler
{
	Genode::Irq_session_client           irq;
	Genode::Signal_handler<Irq_handler>  dispatcher;

	typedef void (*irq_handler)(void*);

	irq_handler  handler;
	void        *priv;

	void handle()
	{
		handler(priv);
		irq.ack_irq();
	}

	Irq_handler(Genode::Entrypoint &ep, Genode::Irq_session_capability cap,
	            irq_handler handler, void *priv)
	:
		irq(cap), dispatcher(ep, *this, &Irq_handler::handle),
		handler(handler), priv(priv)
	{
		irq.sigh(dispatcher);

		/* intial ack so that we will receive IRQ signals */
		irq.ack_irq();
	}
};


extern "C" int dde_interrupt_attach(void(*handler)(void *), void *priv)
{
	static Genode::Constructible<Irq_handler> _irq_handler;

	if (_irq_handler.constructed()) {
		Genode::error("Irq_handler already registered");
		return -1;
	}

	try {
		Platform::Device_client device(pci_drv()._cap);
		_irq_handler.construct(*_global_ep, device.irq(0), handler, priv);
	} catch (...) { return -1; }

	return 0;
}


/***************************************************
 ** Support for aligned and DMA memory allocation **
 ***************************************************/

enum { BACKING_STORE_SIZE = 1024 * 1024 };

struct Backing_store
{
	Genode::Allocator_avl _avl;

	Backing_store (Genode::Allocator &alloc) : _avl(&alloc)
	{
		Genode::addr_t base = pci_drv().alloc_dma_memory(BACKING_STORE_SIZE);
		/* add to allocator */
		_avl.add_range(base, BACKING_STORE_SIZE);
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
	return pci_drv().virt_to_phys((Genode::addr_t)virt); }


/**************
 ** I/O port **
 **************/

static Genode::Constructible<Genode::Io_port_session_client> & io_port()
{
	static Genode::Constructible<Genode::Io_port_session_client> _io_port;
	return _io_port;
}


extern "C" void dde_request_io(dde_uint8_t virt_bar_ioport)
{
	using namespace Genode;

	if (io_port().constructed()) { io_port().destruct(); }

	Platform::Device_client device(pci_drv()._cap);
	Io_port_session_capability cap = device.io_port(virt_bar_ioport);

	io_port().construct(cap);
}


extern "C" dde_uint8_t dde_inb(dde_addr_t port) {
	return io_port()->inb(port); }


extern "C" dde_uint16_t dde_inw(dde_addr_t port) {
	return io_port()->inw(port); }


extern "C" dde_uint32_t dde_inl(dde_addr_t port) {
	return io_port()->inl(port); }


extern "C" void dde_outb(dde_addr_t port, dde_uint8_t data) {
	io_port()->outb(port, data); }


extern "C" void dde_outw(dde_addr_t port, dde_uint16_t data) {
	io_port()->outw(port, data); }


extern "C" void dde_outl(dde_addr_t port, dde_uint32_t data) {
	io_port()->outl(port, data); }


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

struct Io_memory
{
	Genode::Io_mem_session_client       _mem;
	Genode::Io_mem_dataspace_capability _mem_ds;

	Genode::addr_t                      _vaddr;

	Io_memory(Genode::Region_map &rm,
	          Genode::addr_t base, Genode::Io_mem_session_capability cap)
	:
		_mem(cap),
		_mem_ds(_mem.dataspace())
	{
		if (!_mem_ds.valid())
			throw Genode::Exception();

		_vaddr = rm.attach(_mem_ds);
		_vaddr |= base & 0xfff;
	}

	Genode::addr_t vaddr() const { return _vaddr; }
};


static Genode::Constructible<Io_memory> & io_mem()
{
	static Genode::Constructible<Io_memory> _io_mem;
	return _io_mem;
}


extern "C" int dde_request_iomem(dde_addr_t start, dde_addr_t *vaddr)
{
	if (io_mem().constructed()) {
		Genode::error("Io_memory already requested");
		return -1;
	}

	Platform::Device_client device(pci_drv()._cap);
	Genode::Io_mem_session_capability cap;

	Genode::uint8_t virt_iomem_bar = 0;
	for (unsigned i = 0; i < Platform::Device::NUM_RESOURCES; i++) {
		Platform::Device::Resource res = device.resource(i);
		if (res.type() == Platform::Device::Resource::MEMORY) {
			if (res.base() == start) {
				cap = device.io_mem(virt_iomem_bar);
				break;
			}
			virt_iomem_bar ++;
		}
	}

	if (!cap.valid()) { return -1; }

	try {
		io_mem().construct(_global_env->rm(), start, cap);
	} catch (...) { return -1; }

	*vaddr = io_mem()->vaddr();
	return 0;
}


extern "C" int dde_release_iomem(dde_addr_t start, dde_size_t size)
{
	try {
		io_mem().destruct();
		return 0;
	} catch (...) { return -1; }
}
