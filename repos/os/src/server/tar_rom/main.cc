/*
 * \brief  Service that provides files of a TAR archive as ROM sessions
 * \author Martin stein
 * \author Norman Feske
 * \date   2010-04-21
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rom_session/rom_session.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <util/arg_string.h>
#include <base/rpc_server.h>
#include <base/sleep.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/config.h>
#include <base/session_label.h>


/**
 * A 'Rom_session_component' exports a single file of the tar archive
 */
class Rom_session_component : public Genode::Rpc_object<Genode::Rom_session>
{
	private:

		const char *_tar_addr, *_filename, *_file_addr;
		Genode::size_t _file_size, _tar_size;
		Genode::Ram_dataspace_capability _file_ds;

		enum {
			/* length of on data block in tar */
			_BLOCK_LEN = 512,

			/* length of the header field "file-size" in tar */
			_FIELD_SIZE_LEN = 124
		};

		/**
		 * Copy file content into dataspace
		 *
		 * \param dst  destination dataspace
		 */
		void _copy_content_to_dataspace(Genode::Dataspace_capability dst)
		{
			using namespace Genode;

			/* map dataspace locally */
			char *dst_addr = env()->rm_session()->attach(dst);
			Dataspace_client dst_client(dst);

			/* copy content */
			size_t dst_ds_size   = Dataspace_client(dst).size();
			size_t bytes_to_copy = min(_file_size, dst_ds_size);
			memcpy(dst_addr, _file_addr, bytes_to_copy);

			/* unmap dataspace */
			env()->rm_session()->detach(dst_addr);
		}

		/**
		 * Initialize dataspace containing the content of the archived file
		 */
		Genode::Ram_dataspace_capability _init_file_ds()
		{
			bool file_found = false;

			/* measure size of archive in blocks */
			unsigned block_id = 0, block_cnt = _tar_size/_BLOCK_LEN;

			/* scan metablocks of archive */
			while (block_id < block_cnt) {

				unsigned long file_size = 0;
				Genode::ascii_to_unsigned(_tar_addr + block_id*_BLOCK_LEN +
				                          _FIELD_SIZE_LEN, file_size, 8);

				/* get name of tar record */
				char const *record_filename = _tar_addr + block_id*_BLOCK_LEN;

				/* skip leading dot of path if present */
				if (record_filename[0] == '.' && record_filename[1] == '/')
					record_filename++;

				/* get infos about current file */
				if (Genode::strcmp(_filename, record_filename) == 0) {
					_file_size = file_size;
					_file_addr = _tar_addr + (block_id+1) * _BLOCK_LEN;
					file_found = true;
					break;
				}

				/* some datablocks */       /* one metablock */
				block_id = block_id + (file_size / _BLOCK_LEN) + 1;

				/* round up */
				if (file_size % _BLOCK_LEN != 0) block_id++;

				/* check for end of tar archive */
				if (block_id*_BLOCK_LEN >= _tar_size)
					break;

				/* lookout for empty eof-blocks */
				if (*(_tar_addr + (block_id*_BLOCK_LEN)) == 0x00)
					if (*(_tar_addr + (block_id*_BLOCK_LEN + 1)) == 0x00)
						break;
			}

			if (!file_found) {
				Genode::error("couldn't find file '", _filename, "', empty result");
				return Genode::Ram_dataspace_capability();
			}

			/* try to allocate memory for file */
			Genode::Ram_dataspace_capability file_ds;
			try {
				file_ds = Genode::env()->ram_session()->alloc(_file_size);

				/* get content of file copied into dataspace and return */
				_copy_content_to_dataspace(file_ds);
			} catch (...) {
				Genode::error("couldn't allocate memory for file, empty result");
				return file_ds;
			}

			return file_ds;
		}

	public:

		/**
		 * Constructor scans and seeks to file
		 *
		 * \param  tar_addr  local address to tar archive
		 * \param  tar_size  size of tar archive in bytes
		 * \param  filename  name of the requested file
		 */
		Rom_session_component(const char *tar_addr, unsigned tar_size,
		                      const char *filename)
		:
			_tar_addr(tar_addr), _filename(filename), _file_addr(0), _file_size(0),
			_tar_size(tar_size),
			_file_ds(_init_file_ds())
		{
			if (!_file_ds.valid())
				throw Genode::Root::Invalid_args();
		}

		/**
		 * Destructor
		 */
		~Rom_session_component() { Genode::env()->ram_session()->free(_file_ds); }

		/**
		 * Return dataspace with content of file
		 */
		Genode::Rom_dataspace_capability dataspace()
		{
			Genode::Dataspace_capability ds = _file_ds;
			return Genode::static_cap_cast<Genode::Rom_dataspace>(ds);
		}

		void sigh(Genode::Signal_context_capability) { }
};


class Rom_root : public Genode::Root_component<Rom_session_component>
{
	private:

		char    *_tar_addr;
		unsigned _tar_size;

		Rom_session_component *_create_session(const char *args)
		{
			using namespace Genode;

			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();

			Genode::log("connection for module '", module_name, "' requested");

			/* create new session for the requested file */
			return new (md_alloc()) Rom_session_component(_tar_addr, _tar_size,
			                                              module_name.string());
		}

	public:

		/**
		 * Constructor
		 *
		 * \param  entrypoint  entrypoint to be used for ROM sessions
		 * \param  md_alloc    meta-data allocator used for ROM sessions
		 * \param  tar_base    local address of tar archive
		 * \param  tar_size    size of tar archive in bytes
		 */
		Rom_root(Genode::Rpc_entrypoint *entrypoint,
		         Genode::Allocator      *md_alloc,
		         char *tar_addr, Genode::size_t tar_size)
		:
			Genode::Root_component<Rom_session_component>(entrypoint, md_alloc),
			_tar_addr(tar_addr), _tar_size(tar_size)
		{ }
};


using namespace Genode;

int main(void)
{
	/* read name of tar archive from config */
	enum { TAR_FILENAME_MAX_LEN = 64 };
	static char tar_filename[TAR_FILENAME_MAX_LEN];
	try {
		Xml_node archive_node =
			config()->xml_node().sub_node("archive");
		archive_node.attribute("name").value(tar_filename, sizeof(tar_filename));
	} catch (...) {
		Genode::error("could not read 'filename' argument from config");
		return -1;
	}

	/* obtain dataspace of tar archive from ROM service */
	static char  *tar_base = 0;
	static size_t tar_size = 0;
	try {
		static Rom_connection tar_rom(tar_filename);
		tar_base = env()->rm_session()->attach(tar_rom.dataspace());
		tar_size = Dataspace_client(tar_rom.dataspace()).size();
	} catch (...) {
		Genode::error("could not obtain tar archive from ROM service");
		return -2;
	}

	Genode::log("using tar archive '", Cstring(tar_filename), "' with size ", tar_size);

	/* connection to capability service needed to create capabilities */
	static Cap_connection cap;

	/* creation of the entrypoint and the root interface */
	static Sliced_heap sliced_heap(env()->ram_session(),
	                               env()->rm_session());

	enum { STACK_SIZE = 8*1024 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "tar_rom_ep");
	static Rom_root rom_root(&ep, &sliced_heap, tar_base, tar_size);

	/* announce server*/
	env()->parent()->announce(ep.manage(&rom_root));

	/* wait for activation through client */
	sleep_forever();
	return 0;

}
