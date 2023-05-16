/*
 * \brief  VirtIO queue implementation
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VIRTIO__QUEUE_H_
#define _INCLUDE__VIRTIO__QUEUE_H_

#include <platform_session/dma_buffer.h>
#include <base/stdint.h>
#include <util/misc_math.h>

namespace Virtio
{
	using namespace Genode;

	template<typename, typename> class Queue;
	struct Queue_default_traits;
	struct Queue_description;
}


struct Virtio::Queue_description
{
	/**
	 * Physical address of the descriptor table.
	 */
	addr_t desc;

	/**
	 * Physical address of the available descriptor ring.
	 */
	addr_t avail;

	/**
	 * Physcical address of the used descriptor ring.
	 */
	addr_t used;

	/**
	 * The size of the descriptor table (number of elements).
	 */
	uint16_t size;
};


struct Queue_default_traits
{
	/**
	 * The queue is only supposed to be written to by the device.
	 */
	static const bool device_write_only = false;

	/**
	 * Each queue event has additional data payload associated with it.
	 */
	static const bool has_data_payload = false;
};


/**
 * This class implements VirtIO queue interface as defined in section 2.4 of VirtIO 1.0 specification.
 */
template <typename HEADER_TYPE, typename TRAITS = Queue_default_traits>
class Virtio::Queue
{
	private:

		/*
		 * Noncopyable
		 */
		Queue(Queue const &) = delete;
		Queue &operator = (Queue const &) = delete;

		static addr_t _dma_addr(Platform::Connection & p,
		                        Dataspace_capability   c) {
			return p.dma_addr(static_cap_cast<Ram_dataspace>(c)); }

	protected:

		typedef HEADER_TYPE Header_type;

		struct Descriptor
		{
			Descriptor(Descriptor const &) = delete;
			Descriptor &operator = (Descriptor const &) = delete;

			enum Flags : uint16_t
			{
				NEXT  = 1,
				WRITE = 2,
			};

			uint64_t addr;
			uint32_t len;
			uint16_t flags;
			uint16_t next;
		} __attribute__((packed));

		struct Avail
		{
			enum Flags : uint16_t { NO_INTERRUPT = 1 };
			uint16_t flags;
			uint16_t idx;
			uint16_t ring[];
			/* uint16_t used_event; */
		} __attribute__((packed));

		struct Used
		{
			uint16_t flags;
			uint16_t idx;
			struct {
				uint32_t id;
				uint32_t len;
			} ring[];
			/* uint16_t avail_event; */
		} __attribute__((packed));

		/*
		 * This helper splits one RAM dataspace into multiple, equally sized chunks. The
		 * number of such chunk is expected to be equal to the number of entries in VirtIO
		 * descriptor ring. Each chunk will serve as a buffer for single VirtIO descriptor.
		 */
		class Buffer_pool
		{
			private:

				Buffer_pool(Buffer_pool const &) = delete;
				Buffer_pool &operator = (Buffer_pool const &) = delete;

				Platform::Dma_buffer _ds;
				uint16_t       const _buffer_count;
				uint16_t       const _buffer_size;
				addr_t         const _phys_base;

				static size_t _ds_size(uint16_t buffer_count, uint16_t buffer_size) {
					return buffer_count * align_natural(buffer_size); }

			public:

				struct Buffer
				{
					uint8_t  *local_addr;
					addr_t    phys_addr;
					uint16_t  size;
				};

				Buffer_pool(Platform::Connection & plat,
				            uint16_t         const buffer_count,
				            uint16_t         const buffer_size)
				:
					_ds(plat, buffer_count * align_natural(buffer_size),
					    CACHED),
					_buffer_count(buffer_count),
					_buffer_size(buffer_size),
					_phys_base(_ds.dma_addr()) {}

				const Buffer get(uint16_t descriptor_idx) const
				{
					descriptor_idx %= _buffer_count;
					return {
						(uint8_t*)((addr_t)_ds.local_addr<uint8_t>() +
						           descriptor_idx * align_natural(_buffer_size)),
						_phys_base + descriptor_idx * align_natural(_buffer_size),
						_buffer_size
					};
				}

				uint16_t buffer_size() const { return _buffer_size; }
		};

		class Descriptor_ring
		{
			private:

				Descriptor_ring(Descriptor_ring const &) = delete;
				Descriptor_ring &operator = (Descriptor_ring const &) = delete;

				Descriptor  * const _desc_table;
				uint16_t      const  _size;
				uint16_t             _head = 0;
				uint16_t             _tail = 0;

			public:

				Descriptor_ring(uint8_t const *table, uint16_t ring_size)
				: _desc_table((Descriptor *)table), _size(ring_size) { }

				uint16_t reserve() { return _head++ % _size; }
				void free_all() { _tail = _head; }

				uint16_t available_capacity() const {
					return (uint16_t)((_tail > _head) ? _tail - _head
					                                  : _size - _head + _tail) - 1; }

				Descriptor& get(uint16_t const idx) const {
					return _desc_table[idx % _size]; }
		};


		uint16_t                    const _queue_size;
		Platform::Dma_buffer              _ds;
		Buffer_pool                       _buffers;
		Avail            volatile * const _avail;
		Used             volatile * const _used;
		Descriptor_ring                   _descriptors;
		uint16_t                          _last_used_idx = 0;
		Queue_description           const _description;


		/* As defined in section 2.4 of VIRTIO 1.0 specification. */
		static size_t _desc_size(uint16_t queue_size) {
			return 16 * queue_size; }
		static size_t _avail_size(uint16_t queue_size) {
			return 6 + 2 * queue_size; }
		static size_t _used_size(uint16_t queue_size) {
			return 6 + 8 * queue_size; }

		static uint16_t _check_buffer_size(uint16_t buffer_size)
		{
			/**
			 * Each buffer in the queue should be big enough to hold
			 * at least VirtIO header.
			 */
			if (buffer_size < sizeof(Header_type))
				throw Invalid_buffer_size();
			return buffer_size;
		}

		static size_t _ds_size(uint16_t queue_size)
		{
			size_t size = _desc_size(queue_size) + _avail_size(queue_size);
			size = align_natural(size);
			/* See section 2.4 of VirtIO 1.0 specification */
			size += _used_size(queue_size);
			return align_natural(size);
		}

		static Queue_description _init_description(uint16_t queue_size,
		                                           addr_t   phys_addr)
		{
			uint8_t const *base_phys  = (uint8_t *)phys_addr;
			size_t const avail_offset = _desc_size(queue_size);
			size_t const used_offset  =
				align_natural(avail_offset + _avail_size(queue_size));

			return {
				(addr_t)base_phys,
				(addr_t)(base_phys + avail_offset),
				(addr_t)(base_phys + used_offset),
				queue_size,
			};
		}

		static Avail *_init_avail(uint8_t *base_addr, uint16_t queue_size) {
			return (Avail *)(base_addr + _desc_size(queue_size)); }

		static Used *_init_used(uint8_t *base_addr, uint16_t queue_size) {
			return (Used *)(base_addr + align_natural(_desc_size(queue_size) + _avail_size(queue_size))); }

		void _fill_descriptor_table()
		{
			if (!TRAITS::device_write_only)
				return;

			/*
			 * When the queue is only writeable by the VirtIO device by we need to push
			 * all the descriptors to the available ring. The device will then use them
			 * whenever it wants to send us some data.
			 */
			while (_descriptors.available_capacity() > 0) {
				auto const idx = _descriptors.reserve();
				auto &desc = _descriptors.get(idx);
				auto const buffer = _buffers.get(idx);

				desc.addr  = buffer.phys_addr;
				desc.len   = buffer.size;
				desc.flags = Descriptor::Flags::WRITE;
				desc.next  = 0;

				_avail->ring[idx] = idx;
			}
			_avail->flags = 0;
			_avail->idx = _queue_size;
		}

		struct Write_result {
			uint16_t first_descriptor_idx;
			uint16_t last_descriptor_idx;
		};

		/*
		 * Write header and data (if data_size > 0) to a descirptor, or chain of descriptors.
		 * Returns the indexes of first and last descriptor in the chain. The caller must
		 * ensure there are enough descriptors to service the request.
		 */
		Write_result _write_data(Header_type    const &header,
		                         char           const *data,
		                         size_t                data_size)
		{
			static_assert(!TRAITS::device_write_only);
			static_assert(TRAITS::has_data_payload);

			auto const first_desc_idx = _descriptors.reserve();
			auto &desc  = _descriptors.get(first_desc_idx);
			auto buffer = _buffers.get(first_desc_idx);

			desc.addr = buffer.phys_addr;

			memcpy(buffer.local_addr, (void *)&header, sizeof(header));
			desc.len = sizeof(header);

			if (data_size > 0) {
				/*
				 * Try to fit payload data into descriptor which holds the header.
				 *
				 * The size of the buffer is uint16_t so the result should also fit in the same quantity.
				 * The size of the header can never be larger than the size of the buffer, see
				 * _check_buffer_size function.
				 */
				auto const len = (uint16_t)min((size_t)buffer.size - sizeof(header), data_size);
				memcpy(buffer.local_addr + sizeof(header), data, len);
				desc.len += len;
			}

			size_t remaining_data_len = data_size + sizeof(header) - desc.len;

			if (remaining_data_len == 0) {
				/*
				 * There is no more data left to send, everything fit into the first descriptor.
				 */
				desc.flags = 0;
				desc.next = 0;
				return { first_desc_idx, first_desc_idx };
			}

			/*
			 * Some data did not fit into the first descriptor. Chain additional ones.
			 */
			auto chained_idx = _descriptors.reserve();

			desc.flags = Descriptor::Flags::NEXT;
			desc.next = chained_idx % _queue_size;

			size_t data_offset = desc.len - sizeof(header);
			do {
				auto &chained_desc = _descriptors.get(chained_idx);
				buffer = _buffers.get(chained_idx);

				/*
				 * The size of the buffer is specified in uint16_t so the result should also
				 * fit in the same quantity.
				 */
				auto const write_len = (uint16_t)min((size_t)buffer.size, remaining_data_len);
				memcpy(buffer.local_addr, data + data_offset, write_len);

				chained_desc.addr = buffer.phys_addr;
				chained_desc.len = write_len;

				remaining_data_len -= write_len;
				data_offset += write_len;

				if (remaining_data_len > 0) {
					/*
					 * There is still more data to send, chain additional descriptor.
					 */
					chained_idx = _descriptors.reserve();
					chained_desc.flags = Descriptor::Flags::NEXT;
					chained_desc.next = chained_idx;
				} else {
					/*
					 * This was the last descriptor in the chain.
					 */
					chained_desc.flags = 0;
					chained_desc.next = 0;
				}
			} while (remaining_data_len > 0);

			return { first_desc_idx, chained_idx};
		}

	public:

		struct Invalid_buffer_size : Exception { };

		Queue_description const description() const { return _description; }

		bool has_used_buffers() const { return _last_used_idx != _used->idx; }

		void ack_all_transfers()
		{
			static_assert(!TRAITS::device_write_only);

			_last_used_idx = _used->idx;

			if (!TRAITS::device_write_only)
				_descriptors.free_all();
		}

		bool write_data(Header_type    const &header,
		                char           const *data,
		                size_t                data_size)
		{
			static_assert(!TRAITS::device_write_only);
			static_assert(TRAITS::has_data_payload);

			size_t const req_desc_count = 1UL + (sizeof(header) + data_size) / _buffers.buffer_size();
			if (req_desc_count > _descriptors.available_capacity())
				return false;

			auto const write_result = _write_data(header, data, data_size);

			/*
			 * Only the first descritor in the chain needs to be pushed to the available ring.
			 */
			_avail->ring[_avail->idx % _queue_size] = write_result.first_descriptor_idx;
			_avail->idx = _avail->idx + 1;
			_avail->flags = Avail::Flags::NO_INTERRUPT;

			return true;
		}

		template <typename FN>
		void read_data(FN const &fn)
		{
			if (!has_used_buffers())
				return;

			auto const used_idx   = _last_used_idx % _queue_size;
			auto const buffer_idx = (uint16_t)(_used->ring[used_idx].id % _queue_size);
			auto const len        = _used->ring[used_idx].len;

			auto const buffer = _buffers.get(buffer_idx);
			char const *desc_data = (char *)buffer.local_addr;
			Header_type const &header = *((Header_type *)(desc_data));
			char const *data = desc_data + sizeof(Header_type);
			size_t const data_size = len - sizeof(Header_type);

			_last_used_idx += 1;
			_avail->idx = _last_used_idx - 1;

			fn(header, data, data_size);
		}

		Header_type read_data()
		{
			static_assert(!TRAITS::has_data_payload);

			if (!has_used_buffers())
				return Header_type{};

			auto const used_idx   = _last_used_idx % _queue_size;
			auto const buffer_idx = (uint16_t)(_used->ring[used_idx].id % _queue_size);

			auto const buffer = _buffers.get(buffer_idx);
			char const *desc_data = (char *)buffer.local_addr;

			_last_used_idx += 1;
			_avail->idx = _last_used_idx - 1;

			return *((Header_type *)(desc_data));
		}

		template <typename REPLY_TYPE, typename WAIT_REPLY_FN, typename REPLY_FN>
		bool write_data_read_reply(Header_type    const &header,
		                           char           const *data,
		                           size_t        data_size,
		                           WAIT_REPLY_FN  const &wait_for_reply,
		                           REPLY_FN       const &read_reply)
		{
			static_assert(!TRAITS::device_write_only);
			static_assert(TRAITS::has_data_payload);

			/*
			 * This restriction could be lifted by chaining multiple descriptors to receive
			 * the reply. Its probably better however to just ensure buffers are large enough
			 * when configuring the queue instead of adding more complexity to this function.
			 */
			if (sizeof(REPLY_TYPE) > _buffers.buffer_size())
				return false;

			/*
			 * The value of 2 is not a mistake. One additional desciptor is needed for
			 * receiving the response.
			 */
			auto const req_desc_count = 2 + (sizeof(header) + data_size) / _buffers.buffer_size();
			if (req_desc_count > _descriptors.available_capacity())
				return false;

			auto const write_result = _write_data(header, data, data_size);
			auto &last_write_desc = _descriptors.get(write_result.last_descriptor_idx);

			/*
			 * Chain additional descriptor for receiving response.
			 */
			auto const reply_desc_idx = _descriptors.reserve();
			auto &reply_desc = _descriptors.get(reply_desc_idx);
			auto reply_buffer = _buffers.get(reply_desc_idx);

			last_write_desc.flags = Descriptor::Flags::NEXT;
			last_write_desc.next  = reply_desc_idx;

			reply_desc.addr  = reply_buffer.phys_addr;
			reply_desc.len   = sizeof(REPLY_TYPE);
			reply_desc.flags = Descriptor::Flags::WRITE;
			reply_desc.next  = 0;

			/*
			 * Only the first descritor in the chain needs to be pushed to the available ring.
			 */
			_avail->ring[_avail->idx % _queue_size] = write_result.first_descriptor_idx;
			_avail->idx += 1;
			_avail->flags = Avail::Flags::NO_INTERRUPT;

			wait_for_reply();

			/*
			 * Make sure wait call did what it was supposed to do.
			 */
			if (!has_used_buffers())
				return false;

			/*
			 * We need to ACK the transfers regardless if the user provider read_reply function
			 * likes the reply or not. From our POV the transfer was succesful. Its irrelevant if
			 * the user likes the response, or not.
			 */
			ack_all_transfers();

			return read_reply(*reinterpret_cast<REPLY_TYPE const *>(reply_buffer.local_addr));
		}

		template <typename REPLY_TYPE, typename WAIT_REPLY_FN, typename REPLY_FN>
		bool write_data_read_reply(Header_type    const &header,
		                           WAIT_REPLY_FN  const &wait_for_reply,
		                           REPLY_FN       const &read_reply)
		{
			return write_data_read_reply<REPLY_TYPE>(
				header, nullptr, 0, wait_for_reply, read_reply);
		}

		void print(Output& output) const
		{
			print(output, "avail idx: ");
			print(output, _avail->idx % _queue_size);
			print(output, ", used idx = ");
			print(output, _used->idx);
			print(output, ", last seen used idx = ");
			print(output, _last_used_idx);
			print(output, ", capacity = ");
			print(output, _descriptors.available_capacity());
			print(output, ", size = ");
			print(output, _queue_size);
		}

		Queue(Platform::Connection & plat,
		      uint16_t               queue_size,
		      uint16_t               buffer_size)
		: _queue_size(queue_size),
		  _ds(plat, _ds_size(queue_size), UNCACHED),
		  _buffers(plat, queue_size, _check_buffer_size(buffer_size)),
		  _avail(_init_avail(_ds.local_addr<uint8_t>(), queue_size)),
		  _used(_init_used(_ds.local_addr<uint8_t>(), queue_size)),
		  _descriptors(_ds.local_addr<uint8_t>(), queue_size),
		  _description(_init_description(queue_size, _ds.dma_addr()))
		{
			_fill_descriptor_table();
		}
};

#endif /* _INCLUDE__VIRTIO__QUEUE_H_ */
