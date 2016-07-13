/*
 * \brief  Paravirtualized serial device for a Trustzone VM
 * \author Martin Stein
 * \date   2015-10-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <block.h>

/* Genode includes */
#include <block_session/connection.h>
#include <base/thread.h>
#include <util/construct_at.h>
#include <base/allocator_avl.h>
#include <os/config.h>

using namespace Vmm;
using namespace Genode;

/**
 * Reply message from VMM to VM regarding a finished block request
 */
class Reply
{
	private:

		unsigned long _req;
		unsigned long _write;
		unsigned long _data_size;
		unsigned long _data[];

	public:

		/**
		 * Construct a reply for a given block request
		 *
		 * \param req        request base
		 * \param write      wether the request wrote
		 * \param data_size  size of read data
		 * \param data_src   base of read data if any
		 */
		Reply(void * const req, bool const write,
		      size_t const data_size, void * const data_src)
		:
			_req((unsigned long)req), _write(write), _data_size(data_size)
		{
			memcpy(_data, data_src, data_size);
		}

		/**
		 * Return the message size assuming a payload size of 'data_size'
		 */
		static size_t size(size_t const data_size) {
			return data_size + sizeof(Reply); }

} __attribute__ ((__packed__));

/**
 * Cache for pending block requests
 */
class Request_cache
{
	public:

		class Full : public Exception { };

	private:

		class Entry_not_found : public Exception { };

		enum { MAX = Block::Session::TX_QUEUE_SIZE };

		struct Entry
		{
			void * pkt;
			void * req;

		} _cache[MAX];

		unsigned _find(void * const packet)
		{
			for (unsigned i = 0; i < MAX; i++) {
				if (_cache[i].pkt == packet) { return i; }
			}
			throw Entry_not_found();
		}

		void _free(unsigned const id) { _cache[id].pkt = 0; }

	public:

		/**
		 * Construct an empty cache
		 */
		Request_cache() {
			for (unsigned i = 0; i < MAX; i++) { _free(i); } }

		/**
		 * Fill a free entry with packet base 'pkt' and request base 'req'
		 *
		 * \throw Full
		 */
		void insert(void * const pkt, void * const req)
		{
			try {
				unsigned const id = _find(0);
				_cache[id] = { pkt, req };

			} catch (Entry_not_found) { throw Full(); }
		}

		/**
		 * Free entry of packet 'pkt' and return corresponding request in 'req'
		 */
		void remove(void * const pkt, void ** const req)
		{
			try {
				unsigned const id = _find(pkt);
				*req = _cache[id].req;
				_free(id);

			} catch (Entry_not_found) { }
		}
};

/**
 * A block device that is addressable by the VM
 */
class Device
{
	public:

		enum {
			TX_BUF_SIZE  = 5 * 1024 * 1024,
			MAX_NAME_LEN = 64,
		};

	private:

		Request_cache              _cache;
		Allocator_avl              _alloc;
		Block::Connection          _session;
		size_t                     _blk_size;
		Block::sector_t            _blk_cnt;
		Block::Session::Operations _blk_ops;
		Native_capability          _irq_cap;
		Signal_context             _tx;
		char                       _name[MAX_NAME_LEN];
		unsigned const             _irq;
		bool                       _writeable;

	public:

		/**
		 * Construct a device with name 'name' and interrupt 'irq'
		 */
		Device(const char * const name, unsigned const irq)
		:
			_alloc(env()->heap()),
			_session(&_alloc, TX_BUF_SIZE, name), _irq(irq)
		{
			_session.info(&_blk_cnt, &_blk_size, &_blk_ops);
			_writeable = _blk_ops.supported(Block::Packet_descriptor::WRITE);
			strncpy(_name, name, sizeof(_name));
		}

		/*
		 * Accessors
		 */

		Request_cache *     cache()       { return &_cache;    }
		Block::Connection * session()     { return &_session;  }
		Signal_context *    context()     { return &_tx;       }
		size_t              block_size()  { return _blk_size;  }
		size_t              block_count() { return _blk_cnt;   }
		bool                writeable()   { return _writeable; }
		const char *        name()        { return _name;      }
		unsigned            irq()         { return _irq;       }
};

/**
 * Registry of all block devices that are addressable by the VM
 */
class Device_registry
{
	public:

		class Bad_device_id : public Exception { };

	private:

		Device ** _devs;
		unsigned  _count;

		void _init_devs(Xml_node config, unsigned const node_id)
		{
			if (!config.sub_node(node_id).has_type("block")) { return; }

			char label[Device::MAX_NAME_LEN];
			config.sub_node(node_id).attribute("label").value(label, sizeof(label));

			unsigned irq = ~0;
			config.sub_node(node_id).attribute("irq").value(&irq);

			static unsigned dev_id = 0;
			_devs[dev_id] = new (env()->heap()) Device(label, irq);
			dev_id++;
		}

		void _init_count(Xml_node config, unsigned const node_id)
		{
			if (!config.sub_node(node_id).has_type("block")) { return; }
			_count++;
		}

		void _init()
		{
			Xml_node config = Genode::config()->xml_node();
			size_t node_cnt = config.num_sub_nodes();

			for (unsigned i = 0; i < node_cnt; i++) { _init_count(config, i); }
			if (_count == 0) { return; }

			size_t const size = _count * sizeof(Device *);
			_devs = (Device **)env()->heap()->alloc(size);

			for (unsigned i = 0; i < node_cnt; i++) { _init_devs(config, i); }
		}

		Device_registry()
		{
			try { _init(); }
			catch(...) { Genode::error("blk: config parsing error"); }
		}

	public:

		static Device_registry * singleton()
		{
			static Device_registry s;
			return &s;
		}

		/**
		 * Return device with ID 'id' if existent
		 *
		 * \throw Bad_device_id
		 */
		Device * dev(unsigned const id) const
		{
			if (id >= _count) { throw Bad_device_id(); }
			return _devs[id];
		}

		/*
		 * Accessors
		 */

		unsigned count() const { return _count; }
};

/**
 * Thread that listens to device interrupts and propagates them to a VM
 */
class Callback : public Thread_deprecated<8192>
{
	private:

		Lock _ready_lock;

		/*
		 * FIXME
		 * If we want to support multiple VMs at a time, this should be part of
		 * the requests that are saved in the device request-cache.
		 */
		Vm_base * const _vm;

		/*
		 * Thread interface
		 */

		void entry()
		{
			Signal_receiver receiver;
			unsigned const count = Device_registry::singleton()->count();
			for (unsigned i = 0; i < count; i++) {
				Device * const dev = Device_registry::singleton()->dev(i);
				Signal_context_capability cap(receiver.manage(dev->context()));
				dev->session()->tx_channel()->sigh_ready_to_submit(cap);
				dev->session()->tx_channel()->sigh_ack_avail(cap);
			}

			_ready_lock.unlock();

			while (true) {
				Signal s = receiver.wait_for_signal();
				for (unsigned i = 0; i < count; i++) {
					Device * const dev = Device_registry::singleton()->dev(i);
					if (dev->context() == s.context()) {

						/*
						 * FIXME
						 * If we want to support multiple VMs, this should
						 * be read from the corresponding request.
						 */
						_vm->inject_irq(dev->irq());
						break;
					}
				}
			}
		}

	public:

		/**
		 * Construct a callback thread for VM 'vm'
		 */
		Callback(Vm_base * const vm)
		:
			Thread_deprecated<8192>("blk-signal-thread"),
			_ready_lock(Lock::LOCKED),
			_vm(vm)
		{
			Thread::start();
			_ready_lock.lock();
		}
};


/*
 * Vmm::Block implementation
 */

void Vmm::Block::_buf_to_pkt(void * const dst, size_t const sz)
{
	if (sz > _buf_size) { throw Oversized_request(); }
	memcpy(dst, _buf, sz);
}


void Vmm::Block::_name(Vm_base * const vm)
{
	try {
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		strncpy((char *)_buf, dev->name(), _buf_size);

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
	}
}


void Vmm::Block::_block_count(Vm_base * const vm)
{
	try {
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		vm->smc_ret(dev->block_count());

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
		vm->smc_ret(0);
	}
}


void Vmm::Block::_block_size(Vm_base * const vm)
{
	try {
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		vm->smc_ret(dev->block_size());

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
		vm->smc_ret(0);
	}
}


void Vmm::Block::_queue_size(Vm_base * const vm)
{
	try {
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		vm->smc_ret(dev->session()->tx()->bulk_buffer_size());
		return;

	} catch (Device_registry::Bad_device_id) { Genode::error("bad block device ID"); }
	vm->smc_ret(0);
}


void Vmm::Block::_writeable(Vm_base * const vm)
{
	try {
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		vm->smc_ret(dev->writeable());

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
		vm->smc_ret(0);
	}
}


void Vmm::Block::_irq(Vm_base * const vm)
{
	try {
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		vm->smc_ret(dev->irq());

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
		vm->smc_ret(0);
	}
}


void Vmm::Block::_buffer(Vm_base * const vm)
{
	addr_t const buf_base = vm->smc_arg_2();
	            _buf_size = vm->smc_arg_3();
	addr_t const buf_top  = buf_base + _buf_size;
	Ram * const  ram      = vm->ram();
	addr_t const ram_top  = ram->base() + ram->size();

	bool buf_err;
	buf_err  = buf_top  <= buf_base;
	buf_err |= buf_base <  ram->base();
	buf_err |= buf_top  >= ram_top;
	if (buf_err) {
		Genode::error("illegal block buffer constraints");
		return;
	}
	addr_t const buf_off = buf_base - ram->base();
	_buf = (void *)(ram->local() + buf_off);
}


void Vmm::Block::_start_callback(Vm_base * const vm) {
	static Callback c(vm); }


void Vmm::Block::_device_count(Vm_base * const vm)
{
	vm->smc_ret(Device_registry::singleton()->count());
}


void Vmm::Block::_new_request(Vm_base * const vm)
{
	unsigned const      id  = vm->smc_arg_2();
	unsigned long const sz  = vm->smc_arg_3();
	void * const        req = (void*)vm->smc_arg_4();
	try {
		Device * const dev = Device_registry::singleton()->dev(id);
		::Block::Connection * session = dev->session();
		::Block::Packet_descriptor p = session->tx()->alloc_packet(sz);
		void *addr = session->tx()->packet_content(p);
		dev->cache()->insert(addr, req);
		vm->smc_ret((long)addr, p.offset());

	} catch (Request_cache::Full) {
		Genode::error("block request cache full");
		vm->smc_ret(0, 0);

	} catch (::Block::Session::Tx::Source::Packet_alloc_failed) {
		Genode::error("failed to allocate packet for block request");
		vm->smc_ret(0, 0);

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
		vm->smc_ret(0, 0);
	}
}


void Vmm::Block::_submit_request(Vm_base * const vm)
{
	unsigned const      id           = vm->smc_arg_2();
	unsigned long const queue_offset = vm->smc_arg_3();
	unsigned long const size         = vm->smc_arg_4();
	int const           write        = vm->smc_arg_7();
	void * const        dst          = (void *)vm->smc_arg_8();

	unsigned long long const disc_offset =
		(unsigned long long)vm->smc_arg_5() << 32 | vm->smc_arg_6();

	try {
		Device * const dev = Device_registry::singleton()->dev(id);
		if (write) { _buf_to_pkt(dst, size); }

		size_t sector     = disc_offset / dev->block_size();
		size_t sector_cnt = size / dev->block_size();

		using ::Block::Packet_descriptor;
		Packet_descriptor p(
			Packet_descriptor(queue_offset, size),
			write ? Packet_descriptor::WRITE : Packet_descriptor::READ,
			sector, sector_cnt
		);
		dev->session()->tx()->submit_packet(p);

	} catch (Oversized_request) {
		Genode::error("oversized block request");

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
	}
}


void Vmm::Block::_collect_reply(Vm_base * const vm)
{
	try {
		/* lookup device */
		unsigned const id  = vm->smc_arg_2();
		Device * const dev = Device_registry::singleton()->dev(id);
		::Block::Connection * const con = dev->session();

		/* get next packet/request pair and release invalid packets */
		typedef ::Block::Packet_descriptor Packet;
		void * rq = 0;
		Packet pkt;
		for (; !rq; con->tx()->release_packet(pkt)) {

			/* check for packets and tell VM to stop if none available */
			if (!con->tx()->ack_avail()) {
				vm->smc_ret(0);
				return;
			}
			/* lookup request of next packet and free cache slot */
			pkt = con->tx()->get_acked_packet();
			dev->cache()->remove(con->tx()->packet_content(pkt), &rq);
		}
		/* get packet values */
		void * const dat    = con->tx()->packet_content(pkt);
		bool const   w      = pkt.operation() == Packet::WRITE;
		size_t const dat_sz = pkt.size();

		/* communicate response, release packet and tell VM to continue */
		if (Reply::size(dat_sz) > _buf_size) { throw Oversized_request(); }
		construct_at<Reply>(_buf, rq, w, dat_sz, dat);
		con->tx()->release_packet(pkt);
		vm->smc_ret(1);

	} catch (Oversized_request) {
		Genode::error("oversized block request");
		vm->smc_ret(-1);

	} catch (Device_registry::Bad_device_id) {
		Genode::error("bad block device ID");
		vm->smc_ret(-1);
	}
}


void Vmm::Block::handle(Vm_base * const vm)
{
	enum {
		DEVICE_COUNT   = 0,
		BLOCK_COUNT    = 1,
		BLOCK_SIZE     = 2,
		WRITEABLE      = 3,
		QUEUE_SIZE     = 4,
		IRQ            = 5,
		START_CALLBACK = 6,
		NEW_REQUEST    = 7,
		SUBMIT_REQUEST = 8,
		COLLECT_REPLY  = 9,
		BUFFER         = 10,
		NAME           = 11,
	};
	switch (vm->smc_arg_1()) {
	case DEVICE_COUNT:   _device_count(vm);   break;
	case BLOCK_COUNT:    _block_count(vm);    break;
	case BLOCK_SIZE:     _block_size(vm);     break;
	case WRITEABLE:      _writeable(vm);      break;
	case QUEUE_SIZE:     _queue_size(vm);     break;
	case IRQ:            _irq(vm);            break;
	case START_CALLBACK: _start_callback(vm); break;
	case NEW_REQUEST:    _new_request(vm);    break;
	case SUBMIT_REQUEST: _submit_request(vm); break;
	case COLLECT_REPLY:  _collect_reply(vm);  break;
	case BUFFER:         _buffer(vm);         break;
	case NAME:           _name(vm);           break;
	default:
		Genode::error("Unknown function ", vm->smc_arg_1(), " requested on VMM block");
		break;
	}
}
