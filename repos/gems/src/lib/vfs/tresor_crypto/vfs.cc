/*
 * \brief  Integration of the Tresor block encryption
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <vfs/dir_file_system.h>
#include <vfs/single_file_system.h>
#include <util/arg_string.h>
#include <util/xml_generator.h>

/* vfs tresor crypto includes */
#include <interface.h>


namespace Vfs_tresor_crypto {

	using namespace Vfs;
	using namespace Genode;

	class Encrypt_file_system;
	class Decrypt_file_system;

	class Key_local_factory;
	class Key_file_system;

	class Keys_file_system;

	class  Management_file_system;
	struct Add_key_file_system;
	struct Remove_key_file_system;

	struct Local_factory;
	class  File_system;
}


class Vfs_tresor_crypto::Encrypt_file_system : public Vfs::Single_file_system
{
	private:

		Tresor_crypto::Interface &_crypto;
		uint32_t  _key_id;

		struct Encrypt_handle : Single_vfs_handle
		{
			Tresor_crypto::Interface   &_crypto;
			uint32_t  _key_id;

			enum State { NONE, PENDING };
			State _state;

			Encrypt_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Tresor_crypto::Interface            &crypto,
			               uint32_t           key_id)
			:
				Single_vfs_handle(ds, fs, alloc, 0), _crypto(crypto), _key_id(key_id), _state(State::NONE)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (_state != State::PENDING) {
					return READ_ERR_IO;
				}

				_crypto.execute();

				try {
					Tresor_crypto::Interface::Complete_request const cr =
						_crypto.encryption_request_complete(dst);
					if (!cr.valid) {
						return READ_ERR_INVALID;
					}

					_state = State::NONE;

					out_count = dst.num_bytes;
					return READ_OK;
				} catch (Tresor_crypto::Interface::Buffer_too_small) {
					return READ_ERR_INVALID;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src,
			                   size_t &out_count) override
			{
				if (_state != State::NONE) {
					return WRITE_ERR_IO;
				}

				try {
					uint64_t const block_number = seek() / Tresor_crypto::BLOCK_SIZE;
					bool const ok =
						_crypto.submit_encryption_request(block_number, _key_id, src);
					if (!ok) {
						out_count = 0;
						return WRITE_OK;
					}
					_state = State::PENDING;
				} catch (Tresor_crypto::Interface::Buffer_too_small) {
					return WRITE_ERR_INVALID;
				}

				_crypto.execute();
				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Encrypt_file_system(Tresor_crypto::Interface &crypto, uint32_t key_id)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(), Node_rwx::rw(), Xml_node("<encrypt/>")),
			_crypto(crypto), _key_id(key_id)
		{ }

		static char const *type_name() { return "encrypt"; }

		char const *type() override { return type_name(); }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Encrypt_handle(*this, *this, alloc,
					                           _crypto, _key_id);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


class Vfs_tresor_crypto::Decrypt_file_system : public Vfs::Single_file_system
{
	private:

		Tresor_crypto::Interface   &_crypto;
		uint32_t  _key_id;

		struct Decrypt_handle : Single_vfs_handle
		{
			Tresor_crypto::Interface   &_crypto;
			uint32_t  _key_id;

			enum State { NONE, PENDING };
			State _state;

			Decrypt_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Tresor_crypto::Interface            &crypto,
			               uint32_t           key_id)
			:
				Single_vfs_handle(ds, fs, alloc, 0), _crypto(crypto), _key_id(key_id), _state(State::NONE)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (_state != State::PENDING) {
					return READ_ERR_IO;
				}

				_crypto.execute();

				try {
					Tresor_crypto::Interface::Complete_request const cr =
						_crypto.decryption_request_complete(dst);
					if (cr.valid) {
					}
					_state = State::NONE;

					out_count = dst.num_bytes;
					return READ_OK;
				} catch (Tresor_crypto::Interface::Buffer_too_small) {
					return READ_ERR_INVALID;
				}

				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				if (_state != State::NONE) {
					return WRITE_ERR_IO;
				}

				try {
					uint64_t const block_number = seek() / Tresor_crypto::BLOCK_SIZE;
					bool const ok =
						_crypto.submit_decryption_request(block_number, _key_id, src);
					if (!ok) {
						out_count = 0;
						return WRITE_OK;
					}
					_state = State::PENDING;
				} catch (Tresor_crypto::Interface::Buffer_too_small) {
					return WRITE_ERR_INVALID;
				}

				_crypto.execute();
				out_count = src.num_bytes;
				return WRITE_OK;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

	public:

		Decrypt_file_system(Tresor_crypto::Interface &crypto, uint32_t key_id)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(), Node_rwx::rw(), Xml_node("<decrypt/>")),
			_crypto(crypto), _key_id(key_id)
		{ }

		static char const *type_name() { return "decrypt"; }

		char const *type() override { return type_name(); }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const *path, unsigned /* flags */,
		                 Vfs::Vfs_handle **out_handle,
		                 Genode::Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			try {
				*out_handle =
					new (alloc) Decrypt_handle(*this, *this, alloc,
					                           _crypto, _key_id);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs_tresor_crypto::Key_local_factory : File_system_factory
{
	Encrypt_file_system _encrypt_fs;
	Decrypt_file_system _decrypt_fs;

	Key_local_factory(Tresor_crypto::Interface &crypto,
	                  uint32_t               key_id)
	:
		_encrypt_fs(crypto, key_id), _decrypt_fs(crypto, key_id)
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Encrypt_file_system::type_name()))
			return &_encrypt_fs;

		if (node.has_type(Decrypt_file_system::type_name()))
			return &_decrypt_fs;

		return nullptr;
	}
};


class Vfs_tresor_crypto::Key_file_system : private Key_local_factory,
                                        public Vfs::Dir_file_system
{
	private:

		uint32_t _key_id;

		typedef String<128> Config;

		static Config _config(uint32_t key_id)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {

				xml.attribute("name", String<16>(key_id));

				xml.node("decrypt", [&] () { });
				xml.node("encrypt", [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		Key_file_system(Vfs::Env &vfs_env,
		                Tresor_crypto::Interface   &crypto,
		                uint32_t  key_id)
		:
			Key_local_factory(crypto, key_id),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(key_id).string()), *this), _key_id(key_id)
		{ }

		static char const *type_name() { return "keys"; }

		char const *type() override { return type_name(); }

		uint32_t key_id() const
		{
			return _key_id;
		}
};


class Vfs_tresor_crypto::Keys_file_system : public Vfs::File_system
{
	private:

		Vfs::Env &_vfs_env;

		bool _root_dir(char const *path) { return strcmp(path, "/keys") == 0; }
		bool _top_dir(char const *path) { return strcmp(path, "/") == 0; }

		struct Key_registry
		{
			Genode::Allocator &_alloc;
			Tresor_crypto::Interface &_crypto;

			struct Invalid_index : Genode::Exception { };
			struct Invalid_path  : Genode::Exception { };

			uint32_t _number_of_keys { 0 };

			Genode::Registry<Genode::Registered<Key_file_system>> _key_fs { };

			Key_registry(Genode::Allocator &alloc, Tresor_crypto::Interface &crypto)
			:
				_alloc(alloc), _crypto(crypto)
			{ }

			void update(Vfs::Env &vfs_env)
			{
				_crypto.for_each_key([&] (uint32_t const id) {

					bool already_known = false;
					auto lookup = [&] (Key_file_system &fs) {
						already_known |= fs.key_id() == id;
					};
					_key_fs.for_each(lookup);

					if (!already_known) {
						new (_alloc) Genode::Registered<Key_file_system>(
							_key_fs, vfs_env, _crypto, id);
						++_number_of_keys;
					}
				});

				auto find_stale_keys = [&] (Key_file_system const &fs) {
					bool active_key = false;
					_crypto.for_each_key([&] (uint32_t const id) {
						active_key |= id == fs.key_id();
					});

					if (!active_key) {
						destroy(&_alloc, &const_cast<Key_file_system&>(fs));
						--_number_of_keys;
					}
				};
				_key_fs.for_each(find_stale_keys);
			}

			uint32_t number_of_keys() const { return _number_of_keys; }

			Key_file_system const &by_index(uint32_t idx) const
			{
				uint32_t i = 0;
				Key_file_system const *fsp { nullptr };
				auto lookup = [&] (Key_file_system const &fs) {
					if (i == idx) {
						fsp = &fs;
					}
					++i;
				};
				_key_fs.for_each(lookup);
				if (fsp == nullptr) {
					throw Invalid_index();
				}
				return *fsp;
			}

			Key_file_system &_by_id(uint32_t id)
			{
				Key_file_system *fsp { nullptr };
				auto lookup = [&] (Key_file_system &fs) {
					if (fs.key_id() == id) {
						fsp = &fs;
					}
				};
				_key_fs.for_each(lookup);
				if (fsp == nullptr) {
					throw Invalid_path();
				}
				return *fsp;
			}

			Key_file_system &by_path(char const *path)
			{
				if (!path) {
					throw Invalid_path();
				}

				if (path[0] == '/') {
					path++;
				}

				uint32_t id { 0 };
				Genode::ascii_to(path, id);
				return _by_id(id);
			}
		};

	public:

		struct Local_vfs_handle : Vfs::Vfs_handle
		{
			using Vfs_handle::Vfs_handle;

			virtual Read_result read(Byte_range_ptr const &dst,
			                         size_t &out_count) = 0;

			virtual Write_result write(Const_byte_range_ptr const &src,
			                           size_t &out_count) = 0;

			virtual Sync_result sync()
			{
				return SYNC_OK;
			}

			virtual bool read_ready() const = 0;
		};


		struct Dir_vfs_handle : Local_vfs_handle
		{
			Key_registry const &_key_reg;

			bool const _root_dir { false };

			Read_result _query_keys(size_t index,
			                        size_t &out_count,
			                        Dirent &out)
			{
				if (index >= _key_reg.number_of_keys()) {
					out_count = sizeof(Dirent);
					out.type = Dirent_type::END;
					return READ_OK;
				}

				try {
					Key_file_system const &fs = _key_reg.by_index((unsigned)index);
					Genode::String<32> name { fs.key_id() };

					out = {
						.fileno = (Genode::addr_t)this | index,
						.type   = Dirent_type::DIRECTORY,
						.rwx    = Node_rwx::rx(),
						.name   = { name.string() },
					};
					out_count = sizeof(Dirent);
					return READ_OK;
				} catch (Key_registry::Invalid_index) {
					return READ_ERR_INVALID;
				}
			}

			Read_result _query_root(size_t  index,
			                        size_t &out_count,
			                        Dirent &out)
			{
				if (index == 0) {
					out = {
						.fileno = (Genode::addr_t)this,
						.type   = Dirent_type::DIRECTORY,
						.rwx    = Node_rwx::rx(),
						.name   = { "keys" }
					};
				} else {
					out.type = Dirent_type::END;
				}

				out_count = sizeof(Dirent);
				return READ_OK;
			}

			Dir_vfs_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Genode::Allocator &alloc,
			               Key_registry const &key_reg,
			               bool root_dir)
			:
				Local_vfs_handle(ds, fs, alloc, 0),
				_key_reg(key_reg), _root_dir(root_dir)
			{ }

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				out_count = 0;

				if (dst.num_bytes < sizeof(Dirent))
					return READ_ERR_INVALID;

				size_t const index = size_t(seek() / sizeof(Dirent));

				Dirent &out = *(Dirent*)dst.start;

				if (!_root_dir) {

					/* opended as "/<name>" */
					return _query_keys(index, out_count, out);

				} else {
					/* opened as "/" */
					return _query_root(index, out_count, out);
				}
			}

			Write_result write(Const_byte_range_ptr const &, size_t &) override
			{
				return WRITE_ERR_INVALID;
			}

			bool read_ready() const override { return true; }

		};

		struct Dir_snap_vfs_handle : Vfs::Vfs_handle
		{
			Vfs_handle &vfs_handle;

			Dir_snap_vfs_handle(Directory_service &ds,
			                    File_io_service   &fs,
			                    Genode::Allocator &alloc,
			                    Vfs::Vfs_handle   &vfs_handle)
			: Vfs_handle(ds, fs, alloc, 0), vfs_handle(vfs_handle) { }

			~Dir_snap_vfs_handle()
			{
				vfs_handle.close();
			}
		};

		Key_registry _key_reg;

		char const *_sub_path(char const *path) const
		{
			/* skip heading slash in path if present */
			if (path[0] == '/') {
				path++;
			}

			Genode::size_t const name_len = strlen(type_name());
			if (strcmp(path, type_name(), name_len) != 0) {
				return nullptr;
			}

			path += name_len;

			/*
			 * The first characters of the first path element are equal to
			 * the current directory name. Let's check if the length of the
			 * first path element matches the name length.
			 */
			if (*path != 0 && *path != '/') {
				return 0;
			}

			return path;
		}


		Keys_file_system(Vfs::Env             &vfs_env,
		                 Tresor_crypto::Interface &crypto)
		:
			_vfs_env(vfs_env), _key_reg(vfs_env.alloc(), crypto)
		{ }

		static char const *type_name() { return "keys"; }

		char const *type() override { return type_name(); }


		/*********************************
		 ** Directory service interface **
		 *********************************/

		Dataspace_capability dataspace(char const *) override
		{
			return Genode::Dataspace_capability();
		}

		void release(char const *, Dataspace_capability) override { }

		Open_result open(char const       *path,
		                 unsigned          mode,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator        &alloc) override
		{
			_key_reg.update(_vfs_env);

			path = _sub_path(path);
			if (!path || path[0] != '/') {
				return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				Key_file_system &fs = _key_reg.by_path(path);
				return fs.open(path, mode, out_handle, alloc);
			} catch (Key_registry::Invalid_path) { }

			return OPEN_ERR_UNACCESSIBLE;
		}

		Opendir_result opendir(char const       *path,
		                       bool              create,
		                       Vfs::Vfs_handle **out_handle,
		                       Allocator        &alloc) override
		{
			if (create) {
				return OPENDIR_ERR_PERMISSION_DENIED;
			}

			_key_reg.update(_vfs_env);

			bool const top = _top_dir(path);
			if (_root_dir(path) || top) {

				*out_handle = new (alloc) Dir_vfs_handle(*this, *this, alloc,
				                                         _key_reg, top);
				return OPENDIR_OK;
			} else {
				char const *sub_path = _sub_path(path);
				if (!sub_path) {
					return OPENDIR_ERR_LOOKUP_FAILED;
				}
				try {
					Key_file_system &fs = _key_reg.by_path(sub_path);
					Vfs::Vfs_handle *handle = nullptr;
					Opendir_result const res = fs.opendir(sub_path, create, &handle, alloc);
					if (res != OPENDIR_OK) {
						return OPENDIR_ERR_LOOKUP_FAILED;
					}
					*out_handle = new (alloc) Dir_snap_vfs_handle(*this, *this,
					                                              alloc, *handle);
					return OPENDIR_OK;
				} catch (Key_registry::Invalid_path) { }
			}
			return OPENDIR_ERR_LOOKUP_FAILED;
		}

		void close(Vfs_handle *handle) override
		{
			if (handle && (&handle->ds() == this))
				destroy(handle->alloc(), handle);
		}

		Stat_result stat(char const *path, Stat &out_stat) override
		{
			out_stat = Stat { };
			path = _sub_path(path);

			/* path does not match directory name */
			if (!path) {
				return STAT_ERR_NO_ENTRY;
			}

			/*
			 * If path equals directory name, return information about the
			 * current directory.
			 */
			if (strlen(path) == 0 || _top_dir(path)) {

				out_stat.type   = Node_type::DIRECTORY;
				out_stat.inode  = 1;
				out_stat.device = (Genode::addr_t)this;
				return STAT_OK;
			}

			if (!path || path[0] != '/') {
				return STAT_ERR_NO_ENTRY;
			}

			try {
				Key_file_system &fs = _key_reg.by_path(path);
				Stat_result const res = fs.stat(path, out_stat);
				return res;
			} catch (Key_registry::Invalid_path) { }

			return STAT_ERR_NO_ENTRY;
		}

		Unlink_result unlink(char const *) override
		{
			return UNLINK_ERR_NO_PERM;
		}

		Rename_result rename(char const *, char const *) override
		{
			return RENAME_ERR_NO_PERM;
		}

		file_size num_dirent(char const *path) override
		{
			_key_reg.update(_vfs_env);

			if (_top_dir(path) || _root_dir(path)) {
				file_size const num = _key_reg.number_of_keys();
				return num;
			}

			path = _sub_path(path);
			if (!path) {
				return 0;
			}
			try {
				Key_file_system &fs = _key_reg.by_path(path);
				file_size const num = fs.num_dirent(path);
				return num;
			} catch (Key_registry::Invalid_path) {
				return 0;
			}
		}

		bool directory(char const *path) override
		{
			if (_root_dir(path)) {
				return true;
			}

			path = _sub_path(path);
			if (!path) {
				return false;
			}
			try {
				Key_file_system &fs = _key_reg.by_path(path);
				return fs.directory(path);
			} catch (Key_registry::Invalid_path) { }

			return false;
		}

		char const *leaf_path(char const *path) override
		{
			path = _sub_path(path);
			if (!path) {
				return nullptr;
			}

			if (strlen(path) == 0 || strcmp(path, "") == 0) {
				return path;
			}

			try {
				Key_file_system &fs = _key_reg.by_path(path);
				char const *leaf_path = fs.leaf_path(path);
				if (leaf_path) {
					return leaf_path;
				}
			} catch (Key_registry::Invalid_path) { }

			return nullptr;
		}


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs::Vfs_handle *,
		                   Const_byte_range_ptr const &, size_t &) override
		{
			return WRITE_ERR_IO;
		}

		bool queue_read(Vfs::Vfs_handle *vfs_handle, size_t size) override
		{
			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().queue_read(&dh->vfs_handle,
				                                      size);
			}

			return true;
		}

		Read_result complete_read(Vfs::Vfs_handle *vfs_handle,
		                          Byte_range_ptr const &dst,
		                          size_t &out_count) override
		{
			Local_vfs_handle *lh =
				dynamic_cast<Local_vfs_handle*>(vfs_handle);
			if (lh) {
				Read_result const res = lh->read(dst, out_count);
				return res;
			}

			Dir_snap_vfs_handle *dh =
				dynamic_cast<Dir_snap_vfs_handle*>(vfs_handle);
			if (dh) {
				return dh->vfs_handle.fs().complete_read(&dh->vfs_handle,
				                                         dst, out_count);
			}

			return READ_ERR_IO;
		}

		bool read_ready(Vfs::Vfs_handle const &) const override
		{
			return true;
		}

		bool write_ready(Vfs::Vfs_handle const &) const override
		{
			/* wakeup from WRITE_ERR_WOULD_BLOCK not supported */
			return true;
		}

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size ) override
		{
			return FTRUNCATE_OK;
		}
};




class Vfs_tresor_crypto::Management_file_system : public Vfs::Single_file_system
{
	public:

		enum Type { ADD_KEY, REMOVE_KEY };

		static char const *type_string(Type type)
		{
			switch (type) {
			case Type::ADD_KEY:    return "add";
			case Type::REMOVE_KEY: return "remove";
			}
			return nullptr;
		}

	private:

		Management_file_system(Management_file_system const &) = delete;
		Management_file_system &operator=(Management_file_system const&) = delete;

		Type    _type;
		Tresor_crypto::Interface &_crypto;

		struct Manage_handle : Single_vfs_handle
		{
			Type    _type;
			Tresor_crypto::Interface &_crypto;

			Manage_handle(Directory_service &ds,
			              File_io_service   &fs,
			              Genode::Allocator &alloc,
			              Type               type,
			              Tresor_crypto::Interface            &crypto)
			:
				Single_vfs_handle(ds, fs, alloc, 0), _type(type), _crypto(crypto)
			{ }

			Read_result read(Byte_range_ptr const &, size_t &) override
			{
				return READ_ERR_IO;
			}

			Write_result write(Const_byte_range_ptr const &src,
			                   size_t &out_count) override
			{
				out_count = 0;

				if (seek() != 0) {
					return WRITE_ERR_IO;
				}

				if (src.start == nullptr || src.num_bytes < sizeof (uint32_t)) {
					return WRITE_ERR_INVALID;
				}

				uint32_t id = *reinterpret_cast<uint32_t const*>(src.start);
				if (id == 0) {
					return WRITE_ERR_INVALID;
				}

				if (_type == Type::ADD_KEY) {

					if (src.num_bytes != sizeof (uint32_t) + 32 /* XXX Tresor::Key::value*/) {
						return WRITE_ERR_INVALID;
					}

					try {
						char const * value     = src.start     + sizeof (uint32_t);
						size_t const value_len = src.num_bytes - sizeof (uint32_t);
						if (_crypto.add_key(id, value, value_len)) {
							out_count = src.num_bytes;
							return WRITE_OK;
						}
					} catch (...) { }

				} else

				if (_type == Type::REMOVE_KEY) {

					if (src.num_bytes != sizeof (uint32_t)) {
						return WRITE_ERR_INVALID;
					}

					if (_crypto.remove_key(id)) {
						out_count = src.num_bytes;
						return WRITE_OK;
					}
				}

				return WRITE_ERR_IO;
			}

			bool read_ready()  const override { return true; }
			bool write_ready() const override { return true; }
		};

		char const *_type_name;

	public:

		Management_file_system(Tresor_crypto::Interface &crypto, Type type, char const *type_name)
		:
			Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name, Node_rwx::wo(), Xml_node("<manage_keys/>")),
			_type(type), _crypto(crypto), _type_name(type_name)
		{ }

		char const *type() override { return _type_name; }

		/*********************************
		 ** Directory-service interface **
		 *********************************/

		Open_result open(char const         *path,
		                 unsigned            /* flags */,
		                 Vfs::Vfs_handle   **out_handle,
		                 Genode::Allocator  &alloc) override
		{
			if (!_single_file(path)) {
				return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				*out_handle =
					new (alloc) Manage_handle(*this, *this, alloc,
					                          _type, _crypto);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result result = Single_file_system::stat(path, out);
			return result;
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs::Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}
};


struct Vfs_tresor_crypto::Add_key_file_system : public Vfs_tresor_crypto::Management_file_system
{
	static char const *type_name() { return "add_key"; }

	Add_key_file_system(Tresor_crypto::Interface &crypto)
	: Management_file_system(crypto, Management_file_system::ADD_KEY, type_name()) { }

	char const *type() override { return type_name(); }
};


struct Vfs_tresor_crypto::Remove_key_file_system : public Vfs_tresor_crypto::Management_file_system
{
	static char const *type_name() { return "remove_key"; }

	Remove_key_file_system(Tresor_crypto::Interface &crypto)
	: Management_file_system(crypto, Management_file_system::REMOVE_KEY, type_name()) { }

	char const *type() override { return type_name(); }
};


struct Vfs_tresor_crypto::Local_factory : File_system_factory
{
	Keys_file_system       _keys_fs;
	Add_key_file_system    _add_key_fs;
	Remove_key_file_system _remove_key_fs;

	Local_factory(Vfs::Env &env,
	              Tresor_crypto::Interface &crypto)
	:
		_keys_fs(env, crypto), _add_key_fs(crypto), _remove_key_fs(crypto)
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Add_key_file_system::type_name())) {
			return &_add_key_fs;
		}

		if (node.has_type(Remove_key_file_system::type_name())) {
			return &_remove_key_fs;
		}

		if (node.has_type(Keys_file_system::type_name())) {
			return &_keys_fs;
		}

		return nullptr;
	}
};


class Vfs_tresor_crypto::File_system : private Local_factory,
                                    public Vfs::Dir_file_system
{
	private:

		typedef String<128> Config;

		static Config _config(Xml_node node)
		{
			(void)node;
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof (buf), "dir", [&] () {
				xml.attribute(
					"name", node.attribute_value("name", String<64>("")));

				xml.node("add_key",    [&] () { });
				xml.node("remove_key", [&] () { });
				xml.node("keys",       [&] () { });
			});

			return Config(Cstring(buf));
		}

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory(vfs_env, Tresor_crypto::get_interface()),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(node).string()), *this)
		{ }

		~File_system() { }
};


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &vfs_env,
		                         Genode::Xml_node node) override
		{
			try {
				return new (vfs_env.alloc())
					Vfs_tresor_crypto::File_system(vfs_env, node);
			} catch (...) {
				Genode::error("could not create 'tresor_crypto_aes_cbc'");
			}
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}
