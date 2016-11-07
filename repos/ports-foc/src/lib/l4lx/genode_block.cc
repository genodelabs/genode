/*
 * \brief  Genode C API block API needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/log.h>
#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <os/config.h>
#include <foc/capability_space.h>

#include <vcpu.h>
#include <linux.h>

namespace Fiasco {
#include <genode/block.h>
#include <l4/sys/irq.h>
#include <l4/sys/kdebug.h>
}

namespace {

	class Req_cache
	{
		private:

			class Req_entry
			{
				public:

					void     *pkt;
					void     *req;

					Req_entry() : pkt(0), req(0) {}
					Req_entry(void *packet, void *request)
					: pkt(packet), req(request) {}
			};


			enum { MAX = Block::Session::TX_QUEUE_SIZE };

			Req_entry _cache[MAX];

			int _find(void *packet)
			{
				for (int i=0; i < MAX; i++)
					if (_cache[i].pkt == packet)
						return i;
				return -1;
			}

		public:

			void insert(void *packet, void *request)
			{
				int idx = _find(0);
				if (idx == 0) {
					Genode::error("Req cache full!");
					enter_kdebug("Req_cache");
				}

				_cache[idx] = Req_entry(packet, request);
			}

			void remove(void *packet, void **request)
			{
				int idx = _find(packet);
				if (idx == 0) {
					Genode::error("Req cache entry not found!");
					enter_kdebug("Req_cache");
				}

				*request = _cache[idx].req;
				_cache[idx].pkt = 0;
				_cache[idx].req = 0;
			}
	};


	class Block_device
	{
		private:

			enum Dimensions {
				TX_BUF_SIZE   = 5 * 1024 * 1024
			};

			Req_cache                  _cache;
			Genode::Allocator_avl      _alloc;
			Block::Connection          _session;
			Genode::size_t             _blk_size;
			Block::sector_t            _blk_cnt;
			Block::Session::Operations _blk_ops;
			Genode::Native_capability  _irq_cap;
			Genode::Signal_context     _tx;
			char                       _name[32];

			Genode::Native_capability _alloc_irq()
			{
				Genode::Foc_native_cpu_client
					native_cpu(L4lx::cpu_connection()->native_cpu());

				return native_cpu.alloc_irq();
			}

		public:

			Block_device(const char *label)
			: _alloc(Genode::env()->heap()),
			  _session(&_alloc, TX_BUF_SIZE, label),
			  _irq_cap(_alloc_irq())
			{
				_session.info(&_blk_cnt, &_blk_size, &_blk_ops);
				Genode::strncpy(_name, label, sizeof(_name));
			}

			Fiasco::l4_cap_idx_t irq_cap()
			{
				return Genode::Capability_space::kcap(_irq_cap);
			}

			Req_cache              *cache()       { return &_cache;         }
			Block::Connection      *session()     { return &_session;       }
			Genode::Signal_context *context()     { return &_tx;            }
			Genode::size_t          block_size()  { return  _blk_size;      }
			Genode::size_t          block_count() { return  _blk_cnt;       }
			bool                    writeable()   {
				return _blk_ops.supported(Block::Packet_descriptor::WRITE); }
			const char             *name()        { return  _name;          }
	};


	class Signal_thread : public Genode::Thread_deprecated<8192>
	{
		private:

			unsigned       _count;
			Block_device **_devs;
			Genode::Lock   _ready_lock;

		protected:

			void entry()
			{
				using namespace Fiasco;
				using namespace Genode;

				Signal_receiver receiver;
				for (unsigned i = 0; i < _count; i++) {
					Signal_context_capability cap(receiver.manage(_devs[i]->context()));
					_devs[i]->session()->tx_channel()->sigh_ready_to_submit(cap);
					_devs[i]->session()->tx_channel()->sigh_ack_avail(cap);
				}

				_ready_lock.unlock();

				while (true) {
					Signal s = receiver.wait_for_signal();
					for (unsigned i = 0; i < _count; i++) {
						if (_devs[i]->context() == s.context()) {
							if (l4_error(l4_irq_trigger(_devs[i]->irq_cap())) != -1)
								Genode::warning("IRQ block trigger failed");
							break;
						}
					}
				}
			}

		public:

			Signal_thread(Block_device **devs)
			: Genode::Thread_deprecated<8192>("blk-signal-thread"),
			  _count(Fiasco::genode_block_count()), _devs(devs),
			  _ready_lock(Genode::Lock::LOCKED) {}

			void start()
			{
				Genode::Thread::start();

				/*
				 * Do not return until the new thread has initialized the
				 * signal handlers.
				 */
				_ready_lock.lock();
			}
	};

}


static FASTCALL void (*end_request)(void*, short, void*, unsigned long) = 0;
static Block_device **devices = 0;

using namespace Fiasco;

extern "C" {

	unsigned genode_block_count()
	{
		using namespace Genode;

		Linux::Irq_guard guard;

		static unsigned count = 0;
		if (count == 0) {
			try {
				Xml_node config = Genode::config()->xml_node();
				size_t sn_cnt   = config.num_sub_nodes();
				for (unsigned i = 0; i < sn_cnt; i++)
					if (config.sub_node(i).has_type("block"))
						count++;

				if (count == 0)
					return count;

				devices = (Block_device**)
					env()->heap()->alloc(count * sizeof(Block_device*));

				char label[64];
				for (unsigned i = 0, j = 0; i < sn_cnt; i++) {
					if (config.sub_node(i).has_type("block")) {
						config.sub_node(i).attribute("label").value(label,
						                                            sizeof(label));
						devices[j] = new (env()->heap()) Block_device(label);
						j++;
					}
				}
			} catch(...) { Genode::warning("config parsing error!"); }
		}
		return count;
	}


	const char* genode_block_name(unsigned idx)
	{
		if (idx >= genode_block_count()) {
			Genode::warning(__func__, ": invalid index!");
			return 0;
		}
		return devices[idx]->name();
	}


	l4_cap_idx_t genode_block_irq_cap(unsigned idx)
	{
		if (idx >= genode_block_count()) {
			Genode::warning(__func__, ": invalid index!");
			return 0;
		}
		return devices[idx]->irq_cap();
	}


	void genode_block_register_callback(FASTCALL void (*func)(void*, short,
	                                                 void*, unsigned long))
	{
		Linux::Irq_guard guard;

		static Signal_thread thread(devices);
		if (!end_request) {
			end_request = func;
			thread.start();
		}
	}


	void
	genode_block_geometry(unsigned idx, unsigned long *cnt, unsigned long *sz,
	                      int *write, unsigned long *queue_sz)
	{
		if (idx >= genode_block_count()) {
			Genode::warning(__func__, ": invalid index!");
			return;
		}

		Linux::Irq_guard guard;

		*cnt      = devices[idx]->block_count();
		*sz       = devices[idx]->block_size();
		*queue_sz = devices[idx]->session()->tx()->bulk_buffer_size();
		*write    = devices[idx]->writeable() ? 1 : 0;
	}


	void* genode_block_request(unsigned idx, unsigned long sz,
	                           void *req, unsigned long *offset)
	{
		if (idx >= genode_block_count()) {
			Genode::warning(__func__, ": invalid index!");
			return 0;
		}

		Linux::Irq_guard guard;

		try {
			Block::Connection *session = devices[idx]->session();
			Block::Packet_descriptor p = session->tx()->alloc_packet(sz);
			void *addr = session->tx()->packet_content(p);
			devices[idx]->cache()->insert(addr, req);
			*offset = p.offset();
			return addr;
		} catch (Block::Session::Tx::Source::Packet_alloc_failed) { }
		return 0;
	}


	void genode_block_submit(unsigned idx, unsigned long queue_offset,
	                         unsigned long size, unsigned long long disc_offset, int write)
	{
		if (idx >= genode_block_count()) {
			Genode::warning(__func__, ": invalid index!");
			return;
		}

		Linux::Irq_guard guard;

		Genode::size_t sector     = disc_offset / devices[idx]->block_size();
		Genode::size_t sector_cnt = size / devices[idx]->block_size();
		Block::Packet_descriptor p(Block::Packet_descriptor(queue_offset, size),
		                           write ? Block::Packet_descriptor::WRITE
		                           : Block::Packet_descriptor::READ,
		                           sector, sector_cnt);
		devices[idx]->session()->tx()->submit_packet(p);
	}


	void genode_block_collect_responses(unsigned idx)
	{
		if (idx >= genode_block_count()) {
			Genode::warning(__func__, ": invalid index!");
			return;
		}

		unsigned long flags;
		l4x_irq_save(&flags);

		Block::Connection *session = devices[idx]->session();
		void *req;
		while (session->tx()->ack_avail()) {
			Block::Packet_descriptor packet = session->tx()->get_acked_packet();
			void *addr = session->tx()->packet_content(packet);
			bool write = packet.operation() == Block::Packet_descriptor::WRITE;
			devices[idx]->cache()->remove(session->tx()->packet_content(packet), &req);
			if (req && end_request) {
				l4x_irq_restore(flags);
				end_request(req, write, addr, packet.size());
				l4x_irq_save(&flags);
			}
			session->tx()->release_packet(packet);
		}
		l4x_irq_restore(flags);
	}
} // extern "C"
