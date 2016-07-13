/*
 * \brief  DDE iPXE wrappers to C++ backend
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2010-10-21
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
#include <base/printf.h>
#include <base/log.h>
#include <base/slab.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <io_mem_session/connection.h>
#include <io_port_session/connection.h>
#include <irq_session/connection.h>
#include <os/server.h>
#include <platform_device/client.h>
#include <platform_session/connection.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <timer_session/connection.h>
#include <util/misc_math.h>
#include <util/retry.h>

/* local includes */
#include <dde_support.h>


/****************
 ** Migriation **
 ****************/

static Server::Entrypoint *_ep;

extern "C" void dde_init(void *ep)
{
	_ep = (Server::Entrypoint*)ep;
}


/************
 ** printf **
 ************/

extern "C" void dde_vprintf(const char *fmt, va_list va) {
	Genode::vprintf(fmt, va); }


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

extern "C" void dde_udelay(unsigned long usecs)
{
	/*
	 * This function is called only once during rdtsc calibration (usecs will be
	 * 10000, see dde.c 'udelay'.
	 */
	static Timer::Connection timer;
	timer.usleep(usecs);
}


/***************************
 ** locking/synchronizing **
 ***************************/

/**
 * DDE iPXE mutual exclusion lock
 */
static Genode::Lock _ipxe_lock;

extern "C" void dde_lock_enter(void) { _ipxe_lock.lock(); }
extern "C" void dde_lock_leave(void) { _ipxe_lock.unlock(); }


extern "C" void dde_mdelay(unsigned long msecs)
{
	/*
	 * Using one static timer connection here is safe because
	 * this function is only called while initializing the device
	 * and only be the same thread.
	 */
	static Timer::Connection timer;
	timer.msleep(msecs);
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
		case 1:
			return Platform::Device::ACCESS_8BIT;
		case 2:
			return Platform::Device::ACCESS_16BIT;
		default:
			return Platform::Device::ACCESS_32BIT;
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


	Pci_driver() { }

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

		Genode::size_t donate = 4096;
		Genode::retry<Platform::Device::Quota_exceeded>(
			[&] () { client.config_write(devfn, val, _access_size(val)); } ,
			[&] () {
				char quota[32];
				Genode::snprintf(quota, sizeof(quota), "ram_quota=%zd",
				                 donate);
				Genode::env()->parent()->upgrade(_pci.cap(), quota);
				donate *= 2;
			});
	}

	int first_device(int *bus, int *dev, int *fun)
	{
		_cap = _pci.first_device(CLASS_NETWORK, CLASS_MASK);
		if (!_cap.valid())
			return -1;

		_bus_address(bus, dev, fun);
		return 0;
	}

	int next_device(int *bus, int *dev, int *fun)
	{
		int result = -1;

		_last_cap = _cap;

		_cap = _pci.next_device(_cap, CLASS_NETWORK, CLASS_MASK);
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

			Ram_dataspace_capability ram_cap = Genode::retry<Platform::Session::Out_of_metadata>(
				[&] () { return _pci.alloc_dma_buffer(size); },
				[&] () {
					char quota[32];
					Genode::snprintf(quota, sizeof(quota), "ram_quota=%zd",
					                 donate);
					Genode::env()->parent()->upgrade(_pci.cap(), quota);
					donate = donate * 2 > size ? 4096 : donate * 2;
				});

			_region.mapped_base = (Genode::addr_t)env()->rm_session()->attach(ram_cap);
			_region.base = Dataspace_client(ram_cap).phys_addr();

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
	static Pci_driver _pci_drv;
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
	Server::Entrypoint                     &ep;
	Genode::Irq_session_client              irq;
	Genode::Signal_rpc_member<Irq_handler>  dispatcher;

	typedef void (*irq_handler)(void*);

	irq_handler  handler;
	void        *priv;

	void handle(unsigned)
	{
		handler(priv);
		irq.ack_irq();
	}

	Irq_handler(Server::Entrypoint &ep, Genode::Irq_session_capability cap,
	            irq_handler handler, void *priv)
	:
		ep(ep), irq(cap), dispatcher(ep, *this, &Irq_handler::handle),
		handler(handler), priv(priv)
	{
		irq.sigh(dispatcher);

		/* intial ack so that we will receive IRQ signals */
		irq.ack_irq();
	}
};

static Irq_handler *_irq_handler;

extern "C" int dde_interrupt_attach(void(*handler)(void *), void *priv)
{
	if (_irq_handler) {
		Genode::error("Irq_handler already registered");
		Genode::sleep_forever();
	}

	try {
		Platform::Device_client device(pci_drv()._cap);
		_irq_handler = new (Genode::env()->heap())
			Irq_handler(*_ep, device.irq(0), handler, priv);
	} catch (...) { return -1; }

	return 0;
}


/***************************************************
 ** Support for aligned and DMA memory allocation **
 ***************************************************/

enum { BACKING_STORE_SIZE = 1024 * 1024 };

struct Backing_store
{
	Genode::Allocator_avl _avl{Genode::env()->heap()};
	Backing_store(){
		Genode::addr_t base = pci_drv().alloc_dma_memory(BACKING_STORE_SIZE);
		/* add to allocator */
		_avl.add_range(base, BACKING_STORE_SIZE);
	}
};


static Genode::Allocator_avl& allocator()
{
	static Backing_store _instance;
	return _instance._avl;

}


extern "C" void *dde_dma_alloc(dde_size_t size, dde_size_t align,
                                    dde_size_t offset)
{
	void *ptr;
	if (allocator().alloc_aligned(size, &ptr, Genode::log2(align)).error()) {
		Genode::error("memory allocation failed in alloc_memblock ("
		              "size=",   size, " "
		              "align=",  Genode::Hex(align), " "
		              "offset=", Genode::Hex(offset), ")");
		return 0;
	}
	return ptr;
}


extern "C" void dde_dma_free(void *p, dde_size_t size) {
	allocator().free(p, size); }


extern "C" dde_addr_t dde_dma_get_physaddr(void *virt) {
	return pci_drv().virt_to_phys((Genode::addr_t)virt); }


/**************
 ** I/O port **
 **************/

static Genode::Io_port_session_client *_io_port;

extern "C" void dde_request_io(dde_uint8_t virt_bar_ioport)
{
	using namespace Genode;

	if (_io_port) destroy(env()->heap(), _io_port);

	Platform::Device_client device(pci_drv()._cap);
	Io_port_session_capability cap = device.io_port(virt_bar_ioport);

	_io_port = new (env()->heap()) Io_port_session_client(cap);
}


extern "C" dde_uint8_t dde_inb(dde_addr_t port) {
	return _io_port->inb(port); }


extern "C" dde_uint16_t dde_inw(dde_addr_t port) {
	return _io_port->inw(port); }


extern "C" dde_uint32_t dde_inl(dde_addr_t port) {
	return _io_port->inl(port); }


extern "C" void dde_outb(dde_addr_t port, dde_uint8_t data) {
	_io_port->outb(port, data); }


extern "C" void dde_outw(dde_addr_t port, dde_uint16_t data) {
	_io_port->outw(port, data); }


extern "C" void dde_outl(dde_addr_t port, dde_uint32_t data) {
	_io_port->outl(port, data); }


/**********************
 ** Slab memory pool **
 **********************/

struct Slab_backend_alloc : public Genode::Allocator,
                            public Genode::Rm_connection,
                            public Genode::Region_map_client
{
	enum {
		VM_SIZE    = 1024 * 1024,
		BLOCK_SIZE =  64 * 1024,
		ELEMENTS   = VM_SIZE / BLOCK_SIZE,
	};

	Genode::addr_t                    _base;
	Genode::Ram_dataspace_capability  _ds_cap[ELEMENTS];
	int                               _index;
	Genode::Allocator_avl             _range;
	Genode::Ram_session              &_ram;

	bool _alloc_block()
	{
		using namespace Genode;

		if (_index == ELEMENTS) {
			error("slab backend exhausted!");
			return false;
		}

		try {
			_ds_cap[_index] = _ram.alloc(BLOCK_SIZE);
			Region_map_client::attach_at(_ds_cap[_index], _index * BLOCK_SIZE, BLOCK_SIZE, 0);
		} catch (...) { return false; }

		/* return base + offset in VM area */
		Genode::addr_t block_base = _base + (_index * BLOCK_SIZE);
		++_index;

		_range.add_range(block_base, BLOCK_SIZE);
		return true;
	}

	Slab_backend_alloc(Genode::Ram_session &ram)
	:
		Region_map_client(Rm_connection::create(VM_SIZE)),
		_index(0), _range(Genode::env()->heap()), _ram(ram)
	{
		/* reserver attach us, anywere */
		_base = Genode::env()->rm_session()->attach(dataspace());
	}

	Genode::addr_t start() const { return _base; }
	Genode::addr_t end()   const { return _base + VM_SIZE - 1; }

	/*************************
	 ** Allocator interface **
	 *************************/

	bool alloc(Genode::size_t size, void **out_addr)
	{
		bool done = _range.alloc(size, out_addr);

		if (done)
			return done;

		done = _alloc_block();
		if (!done) {
			Genode::error("backend allocator exhausted");
			return false;
		}

		return _range.alloc(size, out_addr);
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
			Genode::addr_t result;
			return (Slab::alloc(_object_size, (void **)&result) ? result : 0);
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

	Slab_backend_alloc &_back_alloc;
	Slab_alloc         *_allocator[NUM_SLABS];
	Genode::addr_t      _start;
	Genode::addr_t      _end;

	Slab(Slab_backend_alloc &alloc)
	: _back_alloc(alloc), _start(alloc.start()), _end(alloc.end())
	{
		for (unsigned i = 0; i < NUM_SLABS; i++)
			_allocator[i] = new (Genode::env()->heap())
				Slab_alloc(1U << (SLAB_START_LOG2 + i), alloc);
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
		using namespace Genode;
		Genode::addr_t *addr = ((Genode::addr_t *)p)-1;
		unsigned index = *(unsigned*)(addr);
		_allocator[index]->free((void*)(addr));
	}
};


static ::Slab& slab()
{
	static ::Slab_backend_alloc sb(*Genode::env()->ram_session());
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

	Io_memory(Genode::addr_t base, Genode::Io_mem_session_capability cap)
	:
		_mem(cap),
		_mem_ds(_mem.dataspace())
	{
		if (!_mem_ds.valid())
			throw Genode::Exception();

		_vaddr = Genode::env()->rm_session()->attach(_mem_ds);
		_vaddr |= base & 0xfff;
	}

	Genode::addr_t vaddr() const { return _vaddr; }
};


static Io_memory *_io_mem;


extern "C" int dde_request_iomem(dde_addr_t start, dde_addr_t *vaddr)
{
	if (_io_mem) {
		Genode::error("Io_memory already requested");
		Genode::sleep_forever();
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
	if (!cap.valid())
		return -1;

	try {
		_io_mem = new (Genode::env()->heap()) Io_memory(start, cap);
	} catch (...) { return -1; }

	*vaddr = _io_mem->vaddr();
	return 0;
}


extern "C" int dde_release_iomem(dde_addr_t start, dde_size_t size)
{
	try {
		destroy(Genode::env()->heap(), _io_mem);
		_io_mem = 0;
		return 0;
	} catch (...) { return -1; }
}
