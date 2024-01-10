/*
 * \brief  Block I/O utilities
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2023-03-16
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__BLOCK_H_
#define _PART_BLOCK__BLOCK_H_

#include "types.h"

namespace Block {
	struct Job;
	struct Sync_read;
}


struct Block::Job : public Block_connection::Job
{
	Registry<Job>::Element registry_element;

	addr_t  const index;   /* job index */
	long    const number;  /* parition number */
	Request       request;
	addr_t  const addr;    /* target payload address */

	Job(Block_connection &connection,
	    Operation         operation,
	    Registry<Job>    &registry,
	    addr_t const      index,
	    addr_t const      number,
	    Request           request,
	    addr_t            addr)
	:
		Block_connection::Job(connection, operation),
		registry_element(registry, *this),
		index(index), number(number), request(request), addr(addr)
	{ }
};


/*
 * Synchronous block I/O (for reading table info)
 */
class Block::Sync_read : Noncopyable
{
	public:

		struct Handler : Interface
		{
			virtual Block_connection & connection()   = 0;
			virtual void               block_for_io() = 0;
		};

	private:

		Handler                      &_handler;
		Allocator                    &_alloc;
		size_t                        _size      { 0 };
		Constructible<Byte_range_ptr> _buffer    { };
		bool                          _success   { false };

		/*
		 * Noncopyable
		 */
		Sync_read(Sync_read const &) = delete;
		Sync_read & operator = (Sync_read const &) = delete;

	public:

		Sync_read(Handler        &handler,
		          Allocator      &alloc,
		          block_number_t  block_number,
		          block_count_t   count)
		: _handler(handler), _alloc(alloc)
		{
			Operation const operation {
				.type         = Operation::Type::READ,
				.block_number = block_number,
				.count        = count
			};

			Block_connection::Job job { _handler.connection(), operation };

			_handler.connection().update_jobs(*this);
			while (!job.completed()) {
				_handler.block_for_io();
				_handler.connection().update_jobs(*this);
			}
		}

		~Sync_read()
		{
			_alloc.free(_buffer->start, _size);
		}

		bool success() const { return _success; }

		void consume_read_result(Block_connection::Job &, off_t offset,
		                         char const *src, size_t length)
		{
			_buffer.construct((char *)_alloc.alloc(length), length);
			memcpy(_buffer->start + offset, src, length);
			_size += length;
		}

		void produce_write_content(Block_connection::Job &, off_t,
		                           char *, size_t)
		{
		}

		void completed(Block_connection::Job &, bool success)
		{
			if (!success)
				error("IO error during partition parsing");

			_success = success;
		}

		Byte_range_ptr const &buffer() const { return *_buffer; }
};


#endif /* _PART_BLOCK__BLOCK_H_ */
