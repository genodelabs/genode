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

/* local includes */
#include <block_driver.h>

/* Genode includes */
#include <util/construct_at.h>
#include <util/xml_node.h>

using namespace Genode;


/*********************************
 ** Block_driver::Request_cache **
 *********************************/

unsigned Block_driver::Request_cache::_find(void *pkt)
{
	for (unsigned i = 0; i < NR_OF_ENTRIES; i++) {
		if (_entries[i].pkt == pkt) {
			return i; }
	}
	throw No_matching_entry();
}


void Block_driver::Request_cache::insert(void *pkt, void *req)
{
	try { _entries[_find(nullptr)] = { pkt, req }; }
	catch (No_matching_entry) { throw Full(); }
}


void Block_driver::Request_cache::remove(void *pkt, void **req)
{
	try {
		unsigned const id = _find(pkt);
		*req = _entries[id].req;
		_free(id);
	}
	catch (No_matching_entry) { }
}


/**************************
 ** Block_driver::Device **
 **************************/

Block_driver::Device::Device(Env              &env,
                             Xml_node          node,
                             Range_allocator  &alloc,
                             Id_space<Device> &id_space,
                             Id                id,
                             Vm_base          &vm)
:
	_vm(vm), _name(node.attribute_value("name", Name())),
	_irq(node.attribute_value("irq", ~(unsigned)0)),
	_irq_handler(env.ep(), *this, &Device::_handle_irq),
	_session(env, &alloc, TX_BUF_SIZE, _name.string()),
	_id_space_elem(*this, id_space, id)
{
	if (_name == Name() || _irq == ~(unsigned)0) {
		throw Invalid(); }
}


void Block_driver::Device::start_irq_handling()
{
	_session.tx_channel()->sigh_ready_to_submit(_irq_handler);
	_session.tx_channel()->sigh_ack_avail(_irq_handler);
}


/******************
 ** Block_driver **
 ******************/

Block_driver::Block_driver(Env       &env,
                           Xml_node   config,
                           Allocator &alloc,
                           Vm_base   &vm) : _dev_alloc(&alloc)
{
	config.for_each_sub_node("block", [&] (Xml_node node) {
		try { new (alloc) Device(env, node, _dev_alloc, _devs,
		                         Device::Id { _dev_count++ }, vm); }
		catch (Device::Invalid) { error("invalid block device"); }
	});
}


void Block_driver::_name(Vm_base &vm)
{
	_dev_apply(Device::Id { vm.smc_arg_2() },
		[&] (Device &dev) { strncpy((char *)_buf, dev.name().string(), _buf_size); },
		[&] ()            { ((char *)_buf)[0] = 0; });
}


void Block_driver::_block_count(Vm_base &vm)
{
	_dev_apply(Device::Id { vm.smc_arg_2() },
		[&] (Device &dev) { vm.smc_ret(dev.block_count()); },
		[&] ()            { vm.smc_ret(0); });
}


void Block_driver::_block_size(Vm_base &vm)
{
	_dev_apply(Device::Id { vm.smc_arg_2() },
		[&] (Device &dev) { vm.smc_ret(dev.block_size()); },
		[&] ()            { vm.smc_ret(0); });
}


void Block_driver::_queue_size(Vm_base &vm)
{
	_dev_apply(Device::Id { vm.smc_arg_2() },
		[&] (Device &dev) { vm.smc_ret(dev.session().tx()->bulk_buffer_size()); },
		[&] ()            { vm.smc_ret(0); });
}


void Block_driver::_writeable(Vm_base &vm)
{
	_dev_apply(Device::Id { vm.smc_arg_2() },
		[&] (Device &dev) { vm.smc_ret(dev.writeable()); },
		[&] ()            { vm.smc_ret(false); });
}


void Block_driver::_irq(Vm_base &vm)
{
	_dev_apply(Device::Id { vm.smc_arg_2() },
		[&] (Device &dev) { vm.smc_ret(dev.irq()); },
		[&] ()            { vm.smc_ret(~(unsigned)0); });
}


void Block_driver::_buffer(Vm_base &vm)
{
	addr_t  const  buf_base = vm.smc_arg_2();
	              _buf_size = vm.smc_arg_3();
	addr_t  const  buf_top  = buf_base + _buf_size;
	Ram     const &ram      = vm.ram();
	addr_t  const  ram_top  = ram.base() + ram.size();

	bool buf_err;
	buf_err  = buf_top  <= buf_base;
	buf_err |= buf_base <  ram.base();
	buf_err |= buf_top  >= ram_top;
	if (buf_err) {
		error("illegal block buffer constraints");
		return;
	}
	addr_t const buf_off = buf_base - ram.base();
	_buf = (void *)(ram.local() + buf_off);
}


void Block_driver::_new_request(Vm_base &vm)
{
	auto dev_func = [&] (Device &dev) {
		try {
			size_t  const size = vm.smc_arg_3();
			void   *const req  = (void*)vm.smc_arg_4();

			Packet_descriptor pkt  = dev.session().alloc_packet(size);
			void             *addr = dev.session().tx()->packet_content(pkt);
			dev.cache().insert(addr, req);
			vm.smc_ret((long)addr, pkt.offset());
		}
		catch (Request_cache::Full) {
			error("block request cache full");
			throw Device_function_failed();
		}
		catch (Block::Session::Tx::Source::Packet_alloc_failed) {
			error("failed to allocate packet for block request");
			throw Device_function_failed();
		}
	};
	_dev_apply(Device::Id { vm.smc_arg_2() }, dev_func, [&] () { vm.smc_ret(0, 0); });
}


void Block_driver::_submit_request(Vm_base &vm)
{
	auto dev_func = [&] (Device &dev) {

		off_t               const queue_offset = vm.smc_arg_3();
		size_t              const size         = vm.smc_arg_4();
		bool                const write        = vm.smc_arg_7();
		void               *const dst          = (void *)vm.smc_arg_8();
		unsigned long long  const disc_offset  =
			(unsigned long long)vm.smc_arg_5() << 32 | vm.smc_arg_6();

		if (write) {
			if (size > _buf_size) {
				error("oversized block request");
				throw Device_function_failed();
			}
			memcpy(dst, _buf, size);
		}
		size_t const sector     = disc_offset / dev.block_size();
		size_t const sector_cnt = size / dev.block_size();
		Packet_descriptor pkt(Packet_descriptor(queue_offset, size),
		                      write ? Packet_descriptor::WRITE :
		                              Packet_descriptor::READ,
		                      sector, sector_cnt);

		dev.session().tx()->submit_packet(pkt);
	};
	_dev_apply(Device::Id { vm.smc_arg_2() }, dev_func, [] () { });
}


void Block_driver::_collect_reply(Vm_base &vm)
{
	auto dev_func = [&] (Device &dev) {

		struct Reply
		{
			unsigned long _req;
			unsigned long _write;
			unsigned long _dat_size;
			unsigned long _dat[0];

			Reply(void *req, bool write, size_t dat_size, void *dat_src)
			:
				_req((unsigned long)req), _write(write), _dat_size(dat_size)
			{
				memcpy(_dat, dat_src, dat_size);
			}
		} __attribute__ ((__packed__));

		/* get next packet/request pair and release invalid packets */
		void * req = 0;
		Packet_descriptor pkt;
		for (; !req; dev.session().tx()->release_packet(pkt)) {

			/* check for packets and tell VM to stop if none available */
			if (!dev.session().tx()->ack_avail()) {
				vm.smc_ret(0);
				return;
			}
			/* lookup request of next packet and free cache slot */
			pkt = dev.session().tx()->get_acked_packet();
			dev.cache().remove(dev.session().tx()->packet_content(pkt), &req);
		}
		/* get packet values */
		void   *const dat      = dev.session().tx()->packet_content(pkt);
		bool    const write    = pkt.operation() == Packet_descriptor::WRITE;
		size_t  const dat_size = pkt.size();

		/* communicate response, release packet and tell VM to continue */
		if (dat_size + sizeof(Reply) > _buf_size) {
			error("oversized block reply");
			throw Device_function_failed();
		}
		construct_at<Reply>(_buf, req, write, dat_size, dat);
		dev.session().tx()->release_packet(pkt);
		vm.smc_ret(1);
	};
	_dev_apply(Device::Id { vm.smc_arg_2() }, dev_func, [&] () { vm.smc_ret(-1); });
}


void Block_driver::handle_smc(Vm_base &vm)
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
	switch (vm.smc_arg_1()) {
	case DEVICE_COUNT:   vm.smc_ret(_dev_count);                  break;
	case BLOCK_COUNT:    _block_count(vm);                        break;
	case BLOCK_SIZE:     _block_size(vm);                         break;
	case WRITEABLE:      _writeable(vm);                          break;
	case QUEUE_SIZE:     _queue_size(vm);                         break;
	case IRQ:            _irq(vm);                                break;
	case START_CALLBACK: _devs.for_each<Device>([&] (Device &dev)
	                           { dev.start_irq_handling(); });    break;
	case NEW_REQUEST:    _new_request(vm);                        break;
	case SUBMIT_REQUEST: _submit_request(vm);                     break;
	case COLLECT_REPLY:  _collect_reply(vm);                      break;
	case BUFFER:         _buffer(vm);                             break;
	case NAME:           _name(vm);                               break;
	default:
		error("unknown block-driver function ", vm.smc_arg_1());
		throw Vm_base::Exception_handling_failed();
	}
}
