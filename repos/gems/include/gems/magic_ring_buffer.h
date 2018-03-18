/*
 * \brief  Region magic ring buffer
 * \author Emery Hemingway
 * \date   2018-02-01
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__RING_BUFFER_H_
#define _INCLUDE__GEMS__RING_BUFFER_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <rm_session/connection.h>
#include <region_map/client.h>
#include <dataspace/client.h>

namespace Genode {
	template <typename TYPE>
	struct Magic_ring_buffer;
}

/**
 * A ring buffer that uses a single dataspace mapped twice in consecutive
 * regions. This allows any operation that is less or equal to the size of
 * the buffer to be read or written in a single pass.
 */
template <typename TYPE>
class Genode::Magic_ring_buffer
{
	private:

		Magic_ring_buffer(Magic_ring_buffer const &);
		Magic_ring_buffer &operator = (Magic_ring_buffer const &);

		Genode::Env &_env;

		Ram_dataspace_capability _buffer_ds;

		size_t const _ds_size = Dataspace_client(_buffer_ds).size();
		size_t const _capacity = _ds_size / sizeof(TYPE);

		Rm_connection _rm_connection { _env };

		/* create region map (reserve address space) */
		Region_map_client _rm { _rm_connection.create(_ds_size*2) };

		/* attach map to global region map */
		TYPE *_buffer = (TYPE *)_env.rm().attach(_rm.dataspace());

		size_t _wpos = 0;
		size_t _rpos = 0;


	public:

		/**
		 * Ring capacity of TYPE items
		 */
		size_t capacity() { return _capacity; }

		/**
		 * Constructor
		 *
		 * \param TYPE  Ring item type, size of type must be a
		 *              power of two and less than the page size
		 *
		 * \param env       Env for dataspace allocation and mapping
		 * \param num_bytes Size of ring in bytes, may be rounded up
		 *                  to the next page boundry
		 *
		 * \throw Region_map::Region_conflict
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 *
		 */
		Magic_ring_buffer(Genode::Env &env, size_t num_bytes)
		: _env(env), _buffer_ds(_env.pd().alloc(num_bytes))
		{
			if (_ds_size % sizeof(TYPE)) {
				error("Magic_ring_buffer cannot hold unaligned TYPE");
				throw Exception();
			}

			/* attach buffer dataspace twice into reserved region */
			_rm.attach_at(_buffer_ds, 0, _ds_size);
			_rm.attach_at(_buffer_ds, _ds_size, _ds_size);
		}

		~Magic_ring_buffer()
		{
			/* detach dataspace from reserved region */
			_rm.detach((addr_t)_ds_size);
			_rm.detach((addr_t)0);

			/* detach reserved region */
			_env.rm().detach((addr_t)_buffer);

			/* free buffer */
			_env.ram().free(_buffer_ds);
		}

		/**
		 * Number of items that may be written to ring
		 */
		size_t write_avail() const
		{
			if (_wpos > _rpos)
				return ((_rpos - _wpos + _capacity) % _capacity) - 2;
			else if (_wpos < _rpos)
				return _rpos - _wpos;
			else
				return _capacity - 2;
		}

		/**
		 * Number of items that may be read from ring
		 */
		size_t read_avail() const
		{
			if (_wpos > _rpos)
				return _wpos - _rpos;
			else
				return (_wpos - _rpos + _capacity) % _capacity;
		}

		/**
		 * Pointer to ring write address
		 */
		TYPE *write_addr() const { return &_buffer[_wpos]; }

		/**
		 * Pointer to ring read address
		 */
		TYPE  *read_addr() const { return &_buffer[_rpos]; }

		/**
		 * Advance the ring write pointer
		 */
		void fill(size_t items) {
			_wpos = (_wpos+items) % _capacity; }

		/**
		 * Advance the ring read pointer
		 */
		void drain(size_t items) {
			_rpos = (_rpos+items) % _capacity; }
};

#endif
