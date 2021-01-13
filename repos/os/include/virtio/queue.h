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

#include <base/attached_ram_dataspace.h>
#include <base/stdint.h>
#include <dataspace/client.h>
#include <util/misc_math.h>

namespace Virtio
{
	template<typename, typename> class Queue;
	struct Queue_default_traits;
	struct Queue_description;
}


struct Virtio::Queue_description
{
	/**
	 * Physical address of the descriptor table.
	 */
	Genode::addr_t desc;

	/**
	 * Physical address of the available descriptor ring.
	 */
	Genode::addr_t avail;

	/**
	 * Physcical address of the used descriptor ring.
	 */
	Genode::addr_t used;

	/**
	 * The size of the descriptor table (number of elements).
	 */
	Genode::uint16_t size;
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
		Queue(Queue const &);
		Queue &operator = (Queue const &);

	protected:

		typedef HEADER_TYPE Header_type;

		struct Descriptor
		{
			enum Flags : Genode::uint16_t
			{
				NEXT  = 1,
				WRITE = 2,
			};

			Genode::uint64_t addr;
			Genode::uint32_t len;
			Genode::uint16_t flags;
			Genode::uint16_t next;
		} __attribute__((packed));

		struct Avail
		{
			enum Flags : Genode::uint16_t { NO_INTERRUPT = 1 };
			Genode::uint16_t flags;
			Genode::uint16_t idx;
			Genode::uint16_t ring[];
			/* Genode::uint16_t used_event; */
		} __attribute__((packed));

		struct Used
		{
			Genode::uint16_t flags;
			Genode::uint16_t idx;
			struct {
				Genode::uint32_t id;
				Genode::uint32_t len;
			} ring[];
			/* Genode::uint16_t avail_event; */
		} __attribute__((packed));

		Genode::uint16_t          const  _queue_size;
		Genode::uint16_t          const  _buffer_size;
		Genode::Attached_ram_dataspace   _ram_ds;
		Descriptor                      *_desc_table = nullptr;
		Avail                  volatile *_avail = nullptr;
		Used                   volatile *_used = nullptr;
		Genode::addr_t                   _buffer_phys_base = 0;
		Genode::addr_t                   _buffer_local_base = 0;
		Genode::uint16_t                 _last_used_idx = 0;
		Queue_description                _description { 0, 0, 0, 0 };


		/* As defined in section 2.4 of VIRTIO 1.0 specification. */
		static Genode::size_t _desc_size(Genode::uint16_t queue_size) {
			return 16 * queue_size; }
		static Genode::size_t _avail_size(Genode::uint16_t queue_size) {
			return 6 + 2 * queue_size; }
		static Genode::size_t _used_size(Genode::uint16_t queue_size) {
			return 6 + 8 * queue_size; }

		Genode::uint16_t _check_buffer_size(Genode::uint16_t buffer_size)
		{
			/**
			 * Each buffer in the queue should be big enough to hold
			 * at least VirtIO header.
			 */
			if (buffer_size < sizeof(Header_type))
				throw Invalid_buffer_size();
			return buffer_size;
		}

		static Genode::size_t _ds_size(Genode::uint16_t queue_size,
		                               Genode::uint16_t buffer_size)
		{
			Genode::size_t size = _desc_size(queue_size) + _avail_size(queue_size);
			size = Genode::align_natural(size);
			/* See section 2.4 of VirtIO 1.0 specification */
			size += _used_size(queue_size);
			size = Genode::align_natural(size);
			return size + (queue_size * Genode::align_natural(buffer_size));
		}

		void _init_tables()
		{
			using namespace Genode;

			Dataspace_client ram_ds_client(_ram_ds.cap());

			uint8_t const *base_phys = (uint8_t *)ram_ds_client.phys_addr();
			uint8_t const *base_local = _ram_ds.local_addr<uint8_t>();

			size_t const avail_offset = _desc_size(_queue_size);
			size_t const used_offset  = align_natural(avail_offset + _avail_size(_queue_size));
			size_t const buff_offset  = align_natural(used_offset + _used_size(_queue_size));

			_desc_table = (Descriptor *)base_local;
			_avail = (Avail *)(base_local + avail_offset);
			_used = (Used *)(base_local + used_offset);
			_buffer_local_base = (addr_t)(base_local + buff_offset);
			_buffer_phys_base  = (addr_t)(base_phys + buff_offset);

			_description.desc  = (addr_t)base_phys;
			_description.avail = (addr_t)(base_phys + avail_offset);
			_description.used  = (addr_t)(base_phys + used_offset);
			_description.size  = _queue_size;
		}

		void _fill_descriptor_table()
		{
			const Genode::uint16_t flags =
				TRAITS::device_write_only ? Descriptor::Flags::WRITE : 0;

			for (Genode::uint16_t idx = 0; idx < _queue_size; idx++) {
				_desc_table[idx] = Descriptor {
					_buffer_phys_base + idx * Genode::align_natural(_buffer_size),
					_buffer_size, flags, 0 };
				_avail->ring[idx] = idx;
			}

			/* Expose all available buffers to the device. */
			if (TRAITS::device_write_only) {
				_avail->flags = 0;
				_avail->idx = _queue_size;
			}
		}

		Genode::uint16_t _avail_capacity() const
		{
			auto const used_idx = _used->idx;
			auto const avail_idx = _avail->idx;
			if (avail_idx >= used_idx) {
				return _queue_size - avail_idx + used_idx;
			} else {
				return used_idx - avail_idx;
			}
		}

		void *_buffer_local_addr(Descriptor const *d) {
			return (void *)(_buffer_local_base + (d->addr - _buffer_phys_base)); }

	public:

		struct Invalid_buffer_size : Genode::Exception { };

		Queue_description const description() const { return _description; }

		bool has_used_buffers() const { return _last_used_idx != _used->idx; }

		void ack_all_transfers() { _last_used_idx = _used->idx;}

		Genode::size_t size() const { return _ds_size(_queue_size, _buffer_size); }

		bool write_data(Header_type    const &header,
		                char           const *data,
		                Genode::size_t        data_size,
		                bool                  request_irq = true)
		{
			static_assert(!TRAITS::device_write_only);
			static_assert(TRAITS::has_data_payload);

			const int req_desc_count = 1 + (sizeof(header) + data_size) / _buffer_size;

			if (req_desc_count > _avail_capacity())
				return false;

			Genode::uint16_t avail_idx = _avail->idx;
			auto *desc = &_desc_table[avail_idx % _queue_size];

			Genode::memcpy(_buffer_local_addr(desc), (void *)&header, sizeof(header));
			desc->len = sizeof(header);

			Genode::size_t len = Genode::min(_buffer_size - sizeof(header), data_size);
			Genode::memcpy((char *)_buffer_local_addr(desc) + desc->len, data, len);
			desc->len += len;

			len = data_size + sizeof(header) - desc->len;

			avail_idx++;

			if (len == 0) {
				desc->flags = 0;
				desc->next = 0;
				_avail->flags = request_irq ? 0 : Avail::Flags::NO_INTERRUPT;
				_avail->idx = avail_idx;
				return true;
			}

			desc->flags = Descriptor::Flags::NEXT;
			desc->next = avail_idx % _queue_size;

			Genode::size_t data_offset = desc->len;
			do {
				desc = &_desc_table[avail_idx % _queue_size];
				avail_idx++;

				Genode::size_t write_len = Genode::min(_buffer_size, len);
				Genode::memcpy((char *)_buffer_local_addr(desc), data + data_offset, write_len);

				desc->len = write_len;
				desc->flags = len > 0 ? Descriptor::Flags::NEXT : 0;
				desc->next = len > 0 ? (avail_idx % _queue_size) : 0;

				len -= write_len;
				data_offset += desc->len;
			} while (len > 0);

			_avail->flags = request_irq ? 0 : Avail::Flags::NO_INTERRUPT;
			_avail->idx = avail_idx;

			return true;
		}

		bool write_data(Header_type const &header, bool request_irq = true)
		{
			static_assert(!TRAITS::device_write_only);
			static_assert(!TRAITS::has_data_payload);

			if (_avail_capacity() == 0)
				return false;

			Genode::uint16_t avail_idx = _avail->idx;
			auto *desc = &_desc_table[avail_idx % _queue_size];

			Genode::memcpy(_buffer_local_addr(desc), (void *)&header, sizeof(header));
			desc->len = sizeof(header);
			desc->flags = 0;
			desc->next = 0;
			_avail->flags = request_irq ? 0 : Avail::Flags::NO_INTERRUPT;
			_avail->idx = ++avail_idx;

			return true;
		}

		template <typename FN>
		void read_data(FN const &fn)
		{
			static_assert(TRAITS::has_data_payload);

			if (!has_used_buffers())
				return;

			Genode::uint16_t const idx = _last_used_idx % _queue_size;
			Genode::uint32_t const len = _used->ring[idx].len;

			auto const *desc = &_desc_table[idx];
			char const *desc_data = (char *)_buffer_local_addr(desc);
			Header_type const &header = *((Header_type *)(desc_data));
			char const *data = desc_data + sizeof(Header_type);
			Genode::size_t const data_size = len - sizeof(Header_type);

			if (fn(header, data, data_size)) {
				_last_used_idx++;
				_avail->idx = _avail->idx + 1;
			}
		}

		Header_type read_data()
		{
			static_assert(!TRAITS::has_data_payload);

			if (!has_used_buffers())
				return Header_type();

			Genode::uint16_t const idx = _last_used_idx % _queue_size;

			auto const *desc = &_desc_table[idx];
			char const *desc_data = (char *)_buffer_local_addr(desc);
			Header_type const &header = *((Header_type *)(desc_data));

			_last_used_idx++;
			_avail->idx = _avail->idx + 1;

			return header;
		}

		void print(Genode::Output& output) const
		{
			Genode::print(output, "avail idx: ");
			Genode::print(output, _avail->idx);
			Genode::print(output, ", used idx = ");
			Genode::print(output, _used->idx);
			Genode::print(output, ", last seen used idx = ");
			Genode::print(output, _last_used_idx);
			Genode::print(output, ", capacity = ");
			Genode::print(output, _avail_capacity());
			Genode::print(output, ", size = ");
			Genode::print(output, _queue_size);
		}

		Queue(Genode::Ram_allocator &ram,
		      Genode::Region_map    &rm,
		      Genode::uint16_t       queue_size,
		      Genode::uint16_t       buffer_size)
		: _queue_size(queue_size),
		  _buffer_size(_check_buffer_size(buffer_size)),
		  _ram_ds(ram, rm, _ds_size(queue_size, buffer_size), Genode::UNCACHED)
		{
			_init_tables();
			_fill_descriptor_table();
		}
};

#endif /* _INCLUDE__VIRTIO__QUEUE_H_ */
