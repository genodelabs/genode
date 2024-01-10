/*
 * \brief  Virtio Block implementation
 * \author Stefan Kalkowski
 * \date   2020-12-22
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIRTIO_BLOCK_H_
#define _VIRTIO_BLOCK_H_

#include <base/log.h>
#include <block_session/connection.h>

#include <virtio_device.h>

namespace Vmm
{
	class Virtio_block_queue;
	class Virtio_block_request;
	class Virtio_block_device;

	using namespace Genode;
}


class Vmm::Virtio_block_queue : public Virtio_split_queue
{
	private:

		Ring_index _used_idx {};

		friend class Virtio_block_request;
		friend class Virtio_block_device;

	public:

		using Virtio_split_queue::Virtio_split_queue;

		template <typename FUNC>
		bool notify(FUNC func)
		{
			memory_barrier();
			for (Ring_index avail_idx = _avail.current();
			     _cur_idx != avail_idx; _cur_idx.inc()) {
				func(_avail.get(_cur_idx), _descriptors, _ram);
			}
			return false;
		}

		void ack(Descriptor_index id, size_t written)
		{
			_used.add(_used_idx, id, written);
			_used_idx.inc();
			_used.write<Used_queue::Idx>(_used_idx.idx());
			memory_barrier();
		}
};


class Vmm::Virtio_block_request
{
	public:

		struct Invalid_request : Genode::Exception {};

	private:

		using Index            = Virtio_block_queue::Descriptor_index;
		using Descriptor       = Virtio_block_queue::Descriptor;
		using Descriptor_array = Virtio_block_queue::Descriptor_array;

		struct Request
		{
			enum Type {
				READ         = 0,
				WRITE        = 1,
				FLUSH        = 4,
				DISCARD      = 11,
				WRITE_ZEROES = 13,
			};

			uint32_t type;
			uint32_t reserved;
			uint64_t sector;
		};

		enum Status { OK, IO_ERROR, UNSUPPORTED };

		Index _next(Descriptor & desc) {
			if (!Descriptor::Flags::Next::get(desc.flags())) {
				throw Invalid_request(); }
			return desc.next();
		}

		Descriptor_array & _array;
		Ram              & _ram;

		template <typename T>
		T * _desc_addr(Descriptor const & desc) const {
			return (T*) _ram.to_local_range({(char *)desc.address(), desc.length()}).start; }

		Index              _request_idx;
		Descriptor         _request    { _array.get(_request_idx)       };
		Request          & _vbr        { *_desc_addr<Request>(_request) };
		Index              _data_idx   { _next(_request)                };
		Descriptor         _data       { _array.get(_data_idx)          };
		Index              _status_idx { _next(_data)                   };
		Descriptor         _status     { _array.get(_status_idx)        };
		size_t             _written    { 0 };

		bool _write() const { return _vbr.type == Request::WRITE; }

	public:

		Virtio_block_request(Index              id,
		                     Descriptor_array & array,
		                     Ram              & ram)
		: _array(array), _ram(ram), _request_idx(id)
		{
			if (_request.length() != sizeof(Request) ||
			    _status.length()  != sizeof(uint8_t)) {
				throw Invalid_request(); }
		}

		Block::Operation const operation(Block::Session::Info & info) const
		{
			if (_vbr.type != Request::READ && _vbr.type != Request::WRITE) {
				throw Invalid_request(); }

			size_t sz = _data.length();

			Block::Operation const op {
				.type = _write() ? Block::Operation::Type::WRITE
				                 : Block::Operation::Type::READ,
				.block_number = (_vbr.sector * 512) / info.block_size,
				.count        = (sz<info.block_size) ? 1 : (sz/info.block_size)
			};
			return op;
		}

		void * address() const  { return _desc_addr<void>(_data); }
		size_t size()    const  { return _data.length();          }
		void   written_to_descriptor(size_t sz) { _written = sz;  }

		void done(Virtio_block_queue & queue)
		{
			*_desc_addr<uint8_t>(_status) = OK;
			queue.ack(_request_idx, _written);
		}
};


class Vmm::Virtio_block_device
: public Virtio_device<Virtio_block_queue, 1>
{
	private:

		using Index            = Virtio_block_queue::Descriptor_index;
		using Descriptor_array = Virtio_block_queue::Descriptor_array;

		enum Queue_idx { REQUEST };

		enum { BLOCK_BUFFER_SIZE = 1024*1024 };

		struct Job : Virtio_block_request,
		             Block::Connection<Job>::Job
		{
			Job(Block::Connection<Job>                & con,
			    Block::Session::Info                  & info,
			    Virtio_block_device::Index              id,
			    Virtio_block_device::Descriptor_array & array,
			    Ram                                   & ram)
			: Virtio_block_request(id, array, ram),
			  Block::Connection<Job>::Job(con, Virtio_block_request::operation(info)) {}
		};

		Heap                                   & _heap;
		Allocator_avl                            _block_alloc { &_heap };
		Block::Connection<Job>                   _block;
		Block::Session::Info                     _block_info { _block.info() };
		Cpu::Signal_handler<Virtio_block_device> _handler;

		void _block_signal()
		{
			 Genode::Mutex::Guard guard(_mutex);
			_block.update_jobs(*this);
		}

		void _notify(unsigned) override
		{
			auto lambda = [&] (Index              id,
			                   Descriptor_array & array,
			                   Ram              & ram)
			{
				try {
					new (_heap) Job(_block, _block_info, id, array, ram);
					_block.update_jobs(*this);
				} catch(Virtio_block_request::Invalid_request &) {
					error("Invalid block request ignored!");
				}
				return 0;
			};

			_queue[REQUEST]->notify(lambda);
		}

		enum Device_id { BLOCK = 2 };

		struct Configuration_area : Mmio_register
		{
			uint64_t capacity;

			Register read(Address_range& range,  Cpu&) override
			{
				if (range.start() == 0 && range.size() == 4)
					return capacity & 0xffffffff;

				if (range.start() == 4 && range.size() == 4)
					return capacity >> 32;

				throw Exception("Invalid read access of configuration area ",
				                range);
			}

			Configuration_area(Virtio_block_device & device, uint64_t capacity)
			:
				Mmio_register("Configuration_area", Mmio_register::RO,
				              0x100, 8, device.registers()),
				capacity(capacity) { }
		} _config_area{ *this, _block_info.block_count *
		                       (_block_info.block_size / 512) };

	public:

		Virtio_block_device(const char * const   name,
		                    const uint64_t       addr,
		                    const uint64_t       size,
		                    unsigned             irq,
		                    Cpu                & cpu,
		                    Mmio_bus           & bus,
		                    Ram                & ram,
		                    Virtio_device_list & list,
		                    Env                & env,
		                    Heap               & heap)
		: Virtio_device<Virtio_block_queue,1>(name, addr, size, irq,
		                                      cpu, bus, ram, list, BLOCK),
		  _heap(heap),
		  _block(env, &_block_alloc, BLOCK_BUFFER_SIZE),
		  _handler(cpu, env.ep(), *this, &Virtio_block_device::_block_signal) {
			_block.sigh(_handler); }


		/*****************************************************
		 ** Block::Connection::Update_jobs_policy interface **
		 *****************************************************/

		void produce_write_content(Job & job, off_t offset,
		                           char *dst, size_t length)
		{
			size_t sz = Genode::min(length,job.size());
			memcpy(dst, (char const *)job.address() + offset, sz);
			job.written_to_descriptor(sz);
		}

		void consume_read_result(Job & job, off_t offset,
		                         char const *src, size_t length)
		{
			size_t sz = Genode::min(length,job.size());
			memcpy((char *)job.address() + offset, src, sz);
		}

		void completed(Job &job, bool)
		{
			job.done(*_queue[REQUEST]);
			_buffer_notification();
			destroy(_heap, &job);
		}
};

#endif /* _VIRTIO_BLOCK_H_ */
