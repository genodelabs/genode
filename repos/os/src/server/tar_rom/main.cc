/*
 * \brief  Service that provides files of a TAR archive as ROM sessions
 * \author Martin stein
 * \author Norman Feske
 * \date   2010-04-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/session_label.h>
#include <root/component.h>

namespace Tar_rom {

	using namespace Genode;
	class Rom_session_component;
	class Rom_root;
	struct Main;
}


/**
 * A 'Rom_session_component' exports a single file of the tar archive
 */
class Tar_rom::Rom_session_component : public Rpc_object<Rom_session>
{
	private:

		/*
		 * Noncopyable
		 */
		Rom_session_component(Rom_session_component const &);
		Rom_session_component &operator = (Rom_session_component const &);

		Ram_allocator &_ram;

		char const * const _tar_addr;
		size_t       const _tar_size;

		Ram_dataspace_capability _file_ds;

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
		void _copy_content_to_dataspace(Region_map &rm, Dataspace_capability dst,
		                                char const *src, size_t len)
		{
			/* temporarily map dataspace */
			Attached_dataspace ds(rm, dst);

			/* copy content */
			size_t bytes_to_copy = min(len, ds.size());
			memcpy(ds.local_addr<char>(), src, bytes_to_copy);
		}

		/**
		 * Initialize dataspace containing the content of the archived file
		 */
		Ram_dataspace_capability _init_file_ds(Ram_allocator &ram, Region_map &rm,
		                                       Session_label const &name)
		{
			/* measure size of archive in blocks */
			unsigned block_id = 0, block_cnt = _tar_size/_BLOCK_LEN;

			char   const *file_content = nullptr;
			unsigned long file_size    = 0;

			/* scan metablocks of archive */
			while (block_id < block_cnt) {

				ascii_to_unsigned(_tar_addr + block_id*_BLOCK_LEN +
				                  _FIELD_SIZE_LEN, file_size, 8);

				/* get name of tar record */
				char const *record_filename = _tar_addr + block_id*_BLOCK_LEN;

				/* skip leading dot of path if present */
				if (record_filename[0] == '.' && record_filename[1] == '/')
					record_filename++;

				/* get infos about current file */
				if (name == record_filename) {
					file_content = _tar_addr + (block_id+1) * _BLOCK_LEN;
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

			if (!file_content) {
				error("couldn't find file '", name, "', empty result");
				return Ram_dataspace_capability();
			}

			/* try to allocate memory for file */
			Ram_dataspace_capability file_ds;
			try {
				file_ds = ram.alloc(file_size);

				/* get content of file copied into dataspace and return */
				_copy_content_to_dataspace(rm, file_ds, file_content, file_size);
			} catch (...) {
				error("couldn't allocate memory for file, empty result");
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
		 * \param  label     name of the requested ROM module
		 *
		 * \throw Service_denied
		 */
		Rom_session_component(Ram_allocator &ram, Region_map &rm,
		                      char const *tar_addr, unsigned tar_size,
		                      Session_label const &label)
		:
			_ram(ram), _tar_addr(tar_addr), _tar_size(tar_size),
			_file_ds(_init_file_ds(ram, rm, label))
		{
			if (!_file_ds.valid())
				throw Service_denied();
		}

		/**
		 * Destructor
		 */
		~Rom_session_component() { _ram.free(_file_ds); }

		/**
		 * Return dataspace with content of file
		 */
		Rom_dataspace_capability dataspace() override
		{
			Dataspace_capability ds = _file_ds;
			return static_cap_cast<Rom_dataspace>(ds);
		}

		void sigh(Signal_context_capability) override { }
};


class Tar_rom::Rom_root : public Root_component<Rom_session_component>
{
	private:

		/*
		 * Noncopyable
		 */
		Rom_root(Rom_root const &);
		Rom_root &operator = (Rom_root const &);

		Env &_env;

		char const * const _tar_addr;
		unsigned     const _tar_size;

		Rom_session_component *_create_session(const char *args) override
		{
			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();
			log("connection for module '", module_name, "' requested");

			/* create new session for the requested file */
			return new (md_alloc()) Rom_session_component(_env.ram(), _env.rm(),
			                                              _tar_addr, _tar_size,
			                                              module_name.string());
		}

	public:

		/**
		 * Constructor
		 *
		 * \param tar_base  local address of tar archive
		 * \param tar_size  size of tar archive in bytes
		 */
		Rom_root(Env &env, Allocator &md_alloc,
		         char const *tar_addr, size_t tar_size)
		:
			Root_component<Rom_session_component>(env.ep(), md_alloc),
			_env(env), _tar_addr(tar_addr), _tar_size(tar_size)
		{ }
};


struct Tar_rom::Main
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	typedef String<64> Name;

	/**
	 * Read name of tar archive from config
	 */
	Name _tar_name()
	{
		try {
			return _config.xml().sub_node("archive").attribute_value("name", Name());
		} catch (...) {
			error("could not read archive name argument from config");
			throw;
		}
	}

	Attached_rom_dataspace _tar_ds { _env, _tar_name().string() };

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Rom_root _root { _env, _sliced_heap, _tar_ds.local_addr<char>(), _tar_ds.size() };

	Main(Env &env) : _env(env)
	{
		log("using tar archive '", _tar_name(), "' with size ", _tar_ds.size());

		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Tar_rom::Main main(env); }

