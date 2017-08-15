/*
 * \brief  Jitterentropy based random file system
 * \author Josef Soentgen
 * \date   2014-08-19
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JITTERENTROPY_FILE_SYSTEM_H_
#define _JITTERENTROPY_FILE_SYSTEM_H_

/* Genode includes */
#include <util/xml_node.h>
#include <vfs/single_file_system.h>

/* jitterentropy includes */
#include <jitterentropy.h>

class Jitterentropy_file_system : public Vfs::Single_file_system
{
	private:

		struct rand_data *_ec_stir;
		bool              _initialized;

		bool _init_jitterentropy(Genode::Allocator &alloc)
		{
			/* initialize private allocator backend */
			jitterentropy_init(alloc);

			int err = jent_entropy_init();
			if (err) {
				Genode::error("jitterentropy library could not be initialized!");
				return false;
			}

			/* use the default behaviour as specified in jitterentropy(3) */
			_ec_stir = jent_entropy_collector_alloc(0, 0);
			if (!_ec_stir) {
				Genode::error("jitterentropy could not allocate entropy collector!");
				return false;
			}

			return true;
		}

		class Jitterentropy_vfs_handle : public Single_vfs_handle
		{
			private:

				struct rand_data *_ec_stir;
				bool             &_initialized;

			public:

				Jitterentropy_vfs_handle(Directory_service &ds,
				                         File_io_service   &fs,
				                         Genode::Allocator &alloc,
				                         struct rand_data  *ec_stir,
				                         bool              &initialized)
				: Single_vfs_handle(ds, fs, alloc, 0),
				  _ec_stir(ec_stir),
				  _initialized(initialized) { }

				Read_result read(char *dst, Vfs::file_size count,
				                 Vfs::file_size &out_count) override
				{
					if (!_initialized)
						return READ_ERR_IO;

					enum { MAX_BUF_LEN = 256 };
					char buf[MAX_BUF_LEN];

					size_t len = count > MAX_BUF_LEN ? MAX_BUF_LEN : count;

					if (jent_read_entropy(_ec_stir, buf, len) < 0)
						return READ_ERR_IO;

					Genode::memcpy(dst, buf, len);

					out_count = len;

					return READ_OK;
				}

				Write_result write(char const *src, Vfs::file_size count,
				                   Vfs::file_size &out_count) override
				{
					return WRITE_ERR_IO;
				}

				bool read_ready() { return true; }
		};

	public:

		Jitterentropy_file_system(Genode::Allocator &alloc,
		                          Genode::Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config),
			_ec_stir(0),
			_initialized(_init_jitterentropy(alloc))
		{ }

		~Jitterentropy_file_system()
		{
			if (_initialized)
				jent_entropy_collector_free(_ec_stir);
		}

		static char const *name()   { return "jitterentropy"; }
		char const *type() override { return "jitterentropy"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const         *path, unsigned,
		                 Vfs::Vfs_handle   **out_handle,
		                 Genode::Allocator  &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc)
				Jitterentropy_vfs_handle(*this, *this, alloc, _ec_stir,
				                         _initialized);
			return OPEN_OK;
		}

};

#endif /* _JITTERENTROPY_FILE_SYSTEM_H_ */
