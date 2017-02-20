/*
 * \brief  Paravirtualized access to block devices for VMs
 * \author Martin Stein
 * \date   2015-10-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BLOCK_DRIVER_H_
#define _BLOCK_DRIVER_H_

/* Genode includes */
#include <block_session/connection.h>
#include <base/allocator_avl.h>

/* local includes */
#include <vm_base.h>

namespace Genode {

	class Xml_node;
	class Block_driver;
}


class Genode::Block_driver
{
	private:

		using Packet_descriptor = Block::Packet_descriptor;

		struct Device_function_failed : Exception { };

		class Request_cache
		{
			private:

				enum { NR_OF_ENTRIES = Block::Session::TX_QUEUE_SIZE };

				struct No_matching_entry : Exception { };

				struct Entry { void *pkt; void *req; } _entries[NR_OF_ENTRIES];

				unsigned _find(void *packet);
				void     _free(unsigned id) { _entries[id].pkt = 0; }

			public:

				struct Full : Exception { };

				Request_cache() {
					for (unsigned i = 0; i < NR_OF_ENTRIES; i++) { _free(i); } }

				void insert(void *pkt, void *req);
				void remove(void *pkt, void **req);
		};

		class Device
		{
			public:

				enum { TX_BUF_SIZE  = 5 * 1024 * 1024 };

				using Id   = Id_space<Device>::Id;
				using Name = String<64>;

			private:

				Request_cache               _cache;
				Vm_base                    &_vm;
				Name     const              _name;
				unsigned const              _irq;
				Signal_handler<Device>      _irq_handler;
				Block::Connection           _session;
				Id_space<Device>::Element   _id_space_elem;
				size_t                      _blk_size;
				Block::sector_t             _blk_cnt;
				Block::Session::Operations  _blk_ops;
				bool                        _writeable;

			public:

				void _handle_irq() { _vm.inject_irq(_irq); }

			public:

				struct Invalid : Exception { };

				Device(Entrypoint       &ep,
				       Xml_node          node,
				       Range_allocator  &alloc,
				       Id_space<Device> &id_space,
				       Id                id,
				       Vm_base          &vm);

				void start_irq_handling();

				Request_cache     &cache()             { return _cache;     }
				Block::Connection &session()           { return _session;   }
				size_t             block_size()  const { return _blk_size;  }
				size_t             block_count() const { return _blk_cnt;   }
				bool               writeable()   const { return _writeable; }
				Name const        &name()        const { return _name;      }
				unsigned           irq()         const { return _irq;       }
		};

		void             *_buf       = nullptr;
		size_t            _buf_size  = 0;
		Id_space<Device>  _devs;
		unsigned          _dev_count = 0;
		Allocator_avl     _dev_alloc;

		void _buf_to_pkt(void *dst, size_t sz);
		void _name(Vm_base &vm);
		void _block_count(Vm_base &vm);
		void _block_size(Vm_base &vm);
		void _queue_size(Vm_base &vm);
		void _writeable(Vm_base &vm);
		void _irq(Vm_base &vm);
		void _buffer(Vm_base &vm);
		void _device_count(Vm_base &vm);
		void _new_request(Vm_base &vm);
		void _submit_request(Vm_base &vm);
		void _collect_reply(Vm_base &vm);

		template <typename DEV_FUNC, typename ERR_FUNC>
		void _dev_apply(Device::Id      id,
		                DEV_FUNC const &dev_func,
		                ERR_FUNC const &err_func)
		{
			try { _devs.apply<Device>(id, [&] (Device &dev) { dev_func(dev); }); }
			catch (Id_space<Device>::Unknown_id) {
				error("unknown block device ", id);
				err_func();
			}
			catch (Device_function_failed) { err_func(); }
		}

	public:

		void handle_smc(Vm_base &vm);

		Block_driver(Entrypoint &ep,
		             Xml_node    config,
		             Allocator  &alloc,
		             Vm_base    &vm);
};

#endif /* _BLOCK_DRIVER_H_ */
