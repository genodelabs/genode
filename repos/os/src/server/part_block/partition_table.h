/*
 * \brief  Partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__PARTITION_TABLE_H_
#define _PART_BLOCK__PARTITION_TABLE_H_

#include <base/env.h>
#include <base/log.h>
#include <base/registry.h>
#include <block_session/connection.h>
#include <os/reporter.h>

namespace Block {
	struct Partition;
	class  Partition_table;
	struct Job;
	using namespace Genode;
	typedef Block::Connection<Job> Block_connection;
}


struct Block::Partition
{
	block_number_t lba;     /* logical block address on device */
	block_count_t  sectors; /* number of sectors in patitions */

	Partition(block_number_t lba, block_count_t sectors)
	: lba(lba), sectors(sectors) { }
};


struct Block::Job : public Block_connection::Job
{
	Registry<Job>::Element registry_element;

	addr_t  const index;                /* job index */
	long    const number;               /* parition number */
	Request       request;
	addr_t  const addr;                 /* target payload address */
	bool          completed { false };
	off_t         offset { 0 };         /* current offset in payload for partial jobs */

	Job(Block_connection &connection,
	    Operation         operation,
	    Registry<Job>    &registry,
	    addr_t const      index,
	    addr_t const      number,
	    Request           request,
	    addr_t            addr)
	: Block_connection::Job(connection, operation),
	  registry_element(registry, *this),
	  index(index), number(number), request(request), addr(addr) { }
};


struct Block::Partition_table : Interface
{
		struct Sector;

		struct Sector_data
		{
			Env              &env;
			Block_connection &block;
			Allocator        &alloc;
			Sector          *current = nullptr;

			Sector_data(Env &env, Block_connection &block, Allocator &alloc)
			: env(env), block(block), alloc(alloc) { }
		};

		/**
		 * Read sectors synchronously
		 */
		class Sector
		{
			private:

				Sector_data &_data;
				bool         _completed { false };
				size_t       _size { 0 };
				void        *_buffer { nullptr };

				Sector(Sector const &);
				Sector &operator = (Sector const &);

			public:

				Sector(Sector_data   &data,
				       block_number_t block_number,
				       block_count_t  count)
				 : _data(data)
				{
					Operation const operation {
						.type         = Operation::Type::READ,
						.block_number = block_number,
						.count        = count
					};

					Block_connection::Job job { data.block, operation };
					_data.block.update_jobs(*this);

					_data.current  = this;

					while (!_completed)
						data.env.ep().wait_and_dispatch_one_io_signal();

					_data.current = nullptr;
				}

				~Sector()
				{
					_data.alloc.free(_buffer, _size);
				}

				void handle_io()
				{
					_data.block.update_jobs(*this);
				}

				void consume_read_result(Block_connection::Job &, off_t,
				                         char const *src, size_t length)
				{
					_buffer = _data.alloc.alloc(length);
					memcpy(_buffer, src, length);
					_size = length;
				}

				void produce_write_content(Block_connection::Job &, off_t, char *, size_t) { }

				void completed(Block_connection::Job &, bool success)
				{
					_completed = true;

					if (!success) {
						error("IO error during partition parsing");
						throw -1;
					}
				}

				template <typename T> T addr() const {
					return reinterpret_cast<T>(_buffer); }
		};

		Env              &env;
		Block_connection &block;
		Reporter         &reporter;
		Sector_data       data;

		Io_signal_handler<Partition_table> io_sigh {
			env.ep(), *this, &Partition_table::handle_io };

		void handle_io()
		{
			if (data.current) { data.current->handle_io(); }
		}

		Partition_table(Env              &env,
		                Block_connection &block,
		                Allocator        &alloc,
		                Reporter         &r)
		: env(env), block(block), reporter(r), data(env, block, alloc)
		{ }

		virtual Partition &partition(long num) = 0;

		virtual bool parse() = 0;
};

#endif /* _PART_BLOCK__PARTITION_TABLE_H_ */
