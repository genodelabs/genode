/*
 * \brief  VFS pipe plugin
 * \author Emery Hemingway
 * \author Sid Hussmann
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vfs/file_system_factory.h>
#include <os/path.h>
#include <os/ring_buffer.h>
#include <base/registry.h>

namespace Vfs_pipe {
	using namespace Vfs;
	typedef Vfs::Directory_service::Open_result Open_result;
	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Genode::Path<Vfs::MAX_PATH_LEN> Path;

	enum { PIPE_BUF_SIZE = 8192U };
	typedef Genode::Ring_buffer<unsigned char, PIPE_BUF_SIZE+1> Pipe_buffer;

	struct Pipe_handle;
	typedef Genode::Fifo_element<Pipe_handle> Handle_element;
	typedef Genode::Fifo<Handle_element> Handle_fifo;
	typedef Genode::Registry<Pipe_handle>::Element Pipe_handle_registry_element;
	typedef Genode::Registry<Pipe_handle> Pipe_handle_registry;

	struct Pipe;
	typedef Genode::Id_space<Pipe> Pipe_space;

	struct New_pipe_handle;

	class File_system;
	class Pipe_file_system;
	class Fifo_file_system;
}


struct Vfs_pipe::Pipe_handle : Vfs::Vfs_handle, private Pipe_handle_registry_element
{
	Pipe &pipe;

	Handle_element read_ready_elem { *this };

	bool const writer;

	Pipe_handle(Vfs::File_system &fs,
	            Genode::Allocator &alloc,
	            unsigned flags,
	            Pipe_handle_registry &registry,
	            Pipe &p)
	:
		Vfs::Vfs_handle(fs, fs, alloc, flags),
		Pipe_handle_registry_element(registry, *this),
		pipe(p),
		writer(flags == Directory_service::OPEN_MODE_WRONLY)
	{ }

	virtual ~Pipe_handle();

	Write_result write(Const_byte_range_ptr const &, size_t &out_count);
	Read_result  read (Byte_range_ptr       const &, size_t &out_count);

	bool read_ready()  const;
	bool write_ready() const;
	bool notify_read_ready();
};


struct Vfs_pipe::Pipe
{
	Genode::Env       &env;
	Vfs::Env::User    &vfs_user;
	Genode::Allocator &alloc;

	Pipe_space::Element  space_elem;
	Pipe_buffer          buffer { };
	Pipe_handle_registry registry { };

	Handle_fifo read_ready_waiters { };

	unsigned num_writers = 0;
	bool waiting_for_writers = true;

	Genode::Io_signal_handler<Pipe> _read_notify_handler
		{ env.ep(), *this, &Pipe::notify_read };

	bool new_handle_active { true };

	Pipe(Genode::Env &env, Vfs::Env::User &vfs_user,
	     Genode::Allocator &alloc, Pipe_space &space)
	:
		env(env), vfs_user(vfs_user), alloc(alloc), space_elem(*this, space)
	{ }

	~Pipe() = default;

	typedef Genode::String<8> Name;
	Name name() const
	{
		return Name(space_elem.id().value);
	}

	void notify_read()
	{
		read_ready_waiters.dequeue_all([] (Handle_element &elem) {
			elem.object().read_ready_response(); });
	}

	void submit_read_signal()
	{
		_read_notify_handler.local_submit();
	}

	void submit_write_signal()
	{
		vfs_user.wakeup_vfs_user();
	}

	/**
	 * Check if pipe is referenced, if not, destroy
	 */
	void cleanup()
	{
		bool alive = new_handle_active;
		if (!alive)
			registry.for_each([&alive] (Pipe_handle&) {
				alive = true; });
		if (!alive)
			destroy(alloc, this);
	}

	/**
	 * Remove "/new" handle reference
	 */
	void remove_new_handle() {
		new_handle_active = false; }

	/**
	 * Detach a handle
	 */
	void remove(Pipe_handle &handle)
	{
		if (handle.read_ready_elem.enqueued())
			read_ready_waiters.remove(handle.read_ready_elem);
	}

	/**
	 * Open a write or read handle
	 */
	Open_result open(Vfs::File_system &fs,
	                 Path const &filename,
	                 Vfs::Vfs_handle **handle,
	                 Genode::Allocator &alloc)
	{
		if (filename == "/in") {

			if (0 == num_writers) {
				/* flush buffer */
				if (!buffer.empty())
					Genode::warning("flushing non-empty buffer. capacity=", buffer.avail_capacity());

				buffer.reset();
			}
			*handle = new (alloc)
				Pipe_handle(fs, alloc, Directory_service::OPEN_MODE_WRONLY, registry, *this);
			num_writers++;
			waiting_for_writers = false;
			return Open_result::OPEN_OK;
		}

		if (filename == "/out") {
			*handle = new (alloc)
				Pipe_handle(fs, alloc, Directory_service::OPEN_MODE_RDONLY, registry, *this);

			if (0 == num_writers && buffer.empty()) {
				waiting_for_writers = true;
			}
			return Open_result::OPEN_OK;
		}

		return Open_result::OPEN_ERR_UNACCESSIBLE;
	}

	Write_result write(Pipe_handle &, Const_byte_range_ptr const &src, size_t &out_count)
	{
		size_t out = 0;

		if (buffer.avail_capacity() == 0) {
			out_count = 0;
			return Write_result::WRITE_OK;
		}

		char const *buf_ptr = src.start;
		while (out < src.num_bytes && 0 < buffer.avail_capacity()) {
			buffer.add(*(buf_ptr++));
			++out;
		}

		out_count = out;

		if (out > 0) {
			vfs_user.wakeup_vfs_user();
			notify_read();
		}

		return Write_result::WRITE_OK;
	}

	Read_result read(Pipe_handle &, Byte_range_ptr const &dst, size_t &out_count)
	{
		size_t out = 0;

		char *buf_ptr = dst.start;
		while (out < dst.num_bytes && !buffer.empty()) {
			*(buf_ptr++) = buffer.get();
			++out;
		}

		out_count = out;

		if (out == 0) {

			/* Send only EOF when at least one writer opened the pipe */
			if ((num_writers == 0) && !waiting_for_writers)
				return Read_result::READ_OK; /* EOF */

			return Read_result::READ_QUEUED;
		}

		/* new pipe space may unblock the writer */
		if (out > 0)
			vfs_user.wakeup_vfs_user();

		return Read_result::READ_OK;
	}
};


Vfs_pipe::Pipe_handle::~Pipe_handle()
{
	pipe.remove(*this);
}


Vfs_pipe::Write_result
Vfs_pipe::Pipe_handle::write(Const_byte_range_ptr const &src, size_t &out_count)
{
	return Pipe_handle::pipe.write(*this, src, out_count);
}


Vfs_pipe::Read_result
Vfs_pipe::Pipe_handle::read(Byte_range_ptr const &dst, size_t &out_count)
{
	return Pipe_handle::pipe.read(*this, dst, out_count);
}


bool Vfs_pipe::Pipe_handle::read_ready() const
{
	return !writer && !pipe.buffer.empty();
}


bool Vfs_pipe::Pipe_handle::write_ready() const
{
	/*
	 * Unconditionally return true for the writer side because
	 * WRITE_ERR_WOULD_BLOCK is not yet supported.
	 */
	return writer;
}


bool Vfs_pipe::Pipe_handle::notify_read_ready()
{
	if (!writer && !read_ready_elem.enqueued())
		pipe.read_ready_waiters.enqueue(read_ready_elem);
	return true;
}


struct Vfs_pipe::New_pipe_handle : Vfs::Vfs_handle
{
	Pipe &pipe;

	New_pipe_handle(Vfs::File_system  &fs,
	                Genode::Env       &env,
	                Vfs::Env::User    &vfs_user,
	                Genode::Allocator &alloc,
	                unsigned           flags,
	                Pipe_space        &pipe_space)
	:
		Vfs::Vfs_handle(fs, fs, alloc, flags),
		pipe(*(new (alloc) Pipe(env, vfs_user, alloc, pipe_space)))
	{ }

	~New_pipe_handle()
	{
		pipe.remove_new_handle();
	}

	Read_result read(Byte_range_ptr const &dst, size_t &out_count)
	{
		auto name = pipe.name();
		if (name.length() < dst.num_bytes) {
			memcpy(dst.start, name.string(), name.length());
			out_count = name.length();
			return Read_result::READ_OK;
		}
		return Read_result::READ_ERR_INVALID;
	}
};


class Vfs_pipe::File_system : public Vfs::File_system
{
	protected:

		Vfs::Env  &_env;

		Pipe_space _pipe_space { };

		/*
		 * verifies if a path meets access control requirements
		 */
		virtual bool _valid_path(const char* cpath) const = 0;

		virtual bool _pipe_id(const char* cpath, Pipe_space::Id &id) const = 0;

		template <typename FN>
		void _try_apply(Pipe_space::Id id, FN const &fn)
		{
			try { _pipe_space.apply<Pipe &>(id, fn); }
			catch (Pipe_space::Unknown_id) { }
		}

	public:

		File_system(Vfs::Env &env)
		:
			_env(env)
		{ }

		const char* type() override { return "pipe"; }

		/***********************
		 ** Directory service **
		 ***********************/

		Genode::Dataspace_capability dataspace(char const*) override {
			return Genode::Dataspace_capability(); }

		void release(char const*, Dataspace_capability) override { }

		Open_result open(const char *cpath,
		                 unsigned mode,
		                 Vfs::Vfs_handle **handle,
		                 Genode::Allocator &alloc) override
		{
			if (mode & OPEN_MODE_CREATE) {
				Genode::warning("cannot open fifo pipe with OPEN_MODE_CREATE");
				return OPEN_ERR_NO_PERM;
			}

			if (!((mode == Open_mode::OPEN_MODE_RDONLY) ||
			      (mode == Open_mode::OPEN_MODE_WRONLY))) {
				Genode::error("pipe only supports opening with WO or RO mode");
				return OPEN_ERR_NO_PERM;
			}

			if (!_valid_path(cpath))
				return OPEN_ERR_UNACCESSIBLE;

			Path const path { cpath };
			if (!path.has_single_element()) {
				/*
				 * find out if the last element is "/in" or "/out"
				 * and enforce read/write policy
				 */
				Path io { cpath };
				io.keep_only_last_element();

				if (io == "/in" && mode != Open_mode::OPEN_MODE_WRONLY)
					return OPEN_ERR_NO_PERM;
				if (io == "/out" && mode != Open_mode::OPEN_MODE_RDONLY)
					return OPEN_ERR_NO_PERM;
			}

			auto result { OPEN_ERR_UNACCESSIBLE };
			Pipe_space::Id id { ~0UL };
			if (_pipe_id(cpath, id)) {
				_try_apply(id, [&mode, &result, this, &handle, &alloc] (Pipe &pipe) {
					auto const type { (mode == OPEN_MODE_RDONLY) ? "/out" : "/in" };
					result = pipe.open(*this, type, handle, alloc);
				});
			}

			return result;
		}

		Opendir_result opendir(char const *cpath, bool create,
		                       Vfs_handle **handle,
		                       Allocator &alloc) override
		{
			/* open dummy handles on directories */
			if (create)
				return OPENDIR_ERR_PERMISSION_DENIED;

			Path io { cpath };
			if (io == "/") {
				*handle = new (alloc)
					Vfs_handle(*this, *this, alloc, 0);
				return OPENDIR_OK;
			}

			auto result { OPENDIR_ERR_PERMISSION_DENIED };
			/* create a path that matches with _pipe_id() */
			Path pseudo_path { cpath };
			io.keep_only_last_element();
			pseudo_path.append(io.string());
			Pipe_space::Id id { ~0UL };
			if (_pipe_id(pseudo_path.string(), id)) {
				_try_apply(id, [&handle, &alloc, this, &result] (Pipe &/*pipe*/) {
					*handle = new (alloc)
						Vfs_handle(*this, *this, alloc, 0);
					result = OPENDIR_OK;
				});
			}
			return result;
		}

		void close(Vfs::Vfs_handle *vfs_handle) override
		{
			Pipe *pipe = nullptr;
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle)) {
				pipe = &handle->pipe;
				if (handle->writer) {
					pipe->num_writers--;

					/* trigger reattempt of read to deliver EOF */
					if (pipe->num_writers == 0)
						pipe->submit_read_signal();
				} else {
					/* a close() may arrive before read() - make sure we deliver EOF */
					pipe->waiting_for_writers = false;
				}
			} else
			if (New_pipe_handle *handle = dynamic_cast<New_pipe_handle*>(vfs_handle))
				pipe = &handle->pipe;

			destroy(vfs_handle->alloc(), vfs_handle);

			if (pipe)
				pipe->cleanup();
		}

		Stat_result stat(const char *cpath, Stat &out) override
		{
			out = Stat { };

			if (!_valid_path(cpath))
				return STAT_ERR_NO_ENTRY;

			Stat_result result { STAT_ERR_NO_ENTRY };
			Path const path { cpath };

			if (path.has_single_element()) {
				Pipe_space::Id id { ~0UL };
				if (_pipe_id(cpath, id)) {
					_try_apply(id, [&out, this, &result] (Pipe const &pipe) {
						out = Stat {
							.size              = file_size(0),
							.type              = Node_type::CONTINUOUS_FILE,
							.rwx               = Node_rwx::rw(),
							.inode             = Genode::addr_t(&pipe),
							.device            = Genode::addr_t(this),
							.modification_time = { }
						};
						result = STAT_OK;
					});
				}
			} else {
				/* find out if the last element is "/in" or "/out" */
				Path io { cpath };
				io.keep_only_last_element();

				Pipe_space::Id id { ~0UL };
				if (_pipe_id(cpath, id)) {
					_try_apply(id, [&io, &out, this, &result] (Pipe const &pipe) {
						if (io == "/in") {
							out = Stat {
								.size              = file_size(pipe.buffer.avail_capacity()),
								.type              = Node_type::CONTINUOUS_FILE,
								.rwx               = Node_rwx::wo(),
								.inode             = Genode::addr_t(&pipe) + 1,
								.device            = Genode::addr_t(this),
								.modification_time = { }
							};
							result = STAT_OK;
						} else
						if (io == "/out") {
							out = Stat {
								.size              = file_size(PIPE_BUF_SIZE
								                             - pipe.buffer.avail_capacity()),
								.type              = Node_type::CONTINUOUS_FILE,
								.rwx               = Node_rwx::ro(),
								.inode             = Genode::addr_t(&pipe) + 2,
								.device            = Genode::addr_t(this),
								.modification_time = { }
							};
							result = STAT_OK;
						}
					});
				}
			}

			return result;
		}

		Unlink_result unlink(const char*) override {
			return UNLINK_ERR_NO_ENTRY; }

		Rename_result rename(const char*, const char*) override {
			return RENAME_ERR_NO_ENTRY; }

		file_size num_dirent(char const *) override { return 0; }

		const char* leaf_path(const char *cpath) override
		{
			Path const path { cpath };
			if (path == "/")
				return cpath;

			if (!_valid_path(cpath))
				return nullptr;

			char const *result { nullptr };
			Pipe_space::Id id { ~0UL };
			if (_pipe_id(cpath, id)) {
				_try_apply(id, [&result, &cpath] (Pipe &) {
					result = cpath;
				});
			}

			return result;
		}



		/**********************
		 ** File I/O service **
		 **********************/

		Write_result write(Vfs_handle *vfs_handle,
		                   Const_byte_range_ptr const &src, size_t &out_count) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->write(src, out_count);

			return WRITE_ERR_INVALID;
		}

		Read_result complete_read(Vfs_handle *vfs_handle,
		                          Byte_range_ptr const &dst, size_t &out_count) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->read(dst, out_count);

			if (New_pipe_handle *handle = dynamic_cast<New_pipe_handle*>(vfs_handle))
				return handle->read(dst, out_count);

			return READ_ERR_INVALID;
		}

		bool read_ready(Vfs_handle const &vfs_handle) const override
		{
			if (Pipe_handle const *handle = dynamic_cast<Pipe_handle const *>(&vfs_handle))
				return handle->read_ready();
			return true;
		}

		bool write_ready(Vfs_handle const &vfs_handle) const override
		{
			if (Pipe_handle const *handle = dynamic_cast<Pipe_handle const *>(&vfs_handle))
				return handle->write_ready();
			return true;
		}

		bool notify_read_ready(Vfs_handle *vfs_handle) override
		{
			if (Pipe_handle *handle = dynamic_cast<Pipe_handle*>(vfs_handle))
				return handle->notify_read_ready();
			return false;
		}

		Ftruncate_result ftruncate(Vfs_handle*, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }

		Sync_result complete_sync(Vfs_handle*) override {
			return SYNC_OK; }
};


class Vfs_pipe::Pipe_file_system : public Vfs_pipe::File_system
{
	protected:

		virtual bool _pipe_id(const char* cpath, Pipe_space::Id &id) const override
		{
			return 0 != ascii_to(cpath + 1, id.value);
		}

		bool _valid_path(const char *cpath) const  override
		{
			/*
			 * a valid pipe path is either
			 * "/pipe_number",
			 * "/pipe_number/in"
			 * or
			 * "/pipe_number/out"
			 */
			Path io { cpath };
			if (io.has_single_element())
				return true;

			io.keep_only_last_element();
			if ((io == "/in" || io == "/out"))
				return true;

			return false;
		}

	public:

		Pipe_file_system(Vfs::Env &env)
		:
			File_system(env)
		{ }

		Open_result open(const char *cpath,
		                 unsigned mode,
		                 Vfs::Vfs_handle **handle,
		                 Genode::Allocator &alloc) override
		{
			Path const path { cpath };

			if (path == "/new") {
				if ((OPEN_MODE_ACCMODE & mode) == OPEN_MODE_WRONLY)
					return OPEN_ERR_NO_PERM;
				*handle = new (alloc)
					New_pipe_handle(*this, _env.env(), _env.user(), alloc, mode, _pipe_space);
				return OPEN_OK;
			}

			return File_system::open(cpath, mode, handle, alloc);
		}

		Stat_result stat(const char *cpath, Stat &out) override
		{
			out = Stat { };
			Path const path { cpath };

			if (path == "/new") {
				out = Stat {
					.size              = 1,
					.type              = Node_type::TRANSACTIONAL_FILE,
					.rwx               = Node_rwx::ro(),
					.inode             = Genode::addr_t(this),
					.device            = Genode::addr_t(this),
					.modification_time = { }
				};
				return STAT_OK;
			}

			return File_system::stat(cpath, out);
		}

		bool directory(char const *cpath) override
		{
			Path const path { cpath };
			if (path == "/") return true;
			if (path == "/new") return false;

			if (!path.has_single_element()) return false;

			bool result { false };
			Pipe_space::Id id { ~0UL };
			if (_pipe_id(cpath, id)) {
				_try_apply(id, [&result] (Pipe &) {
					result = true;
				});
			}

			return result;
		}

		const char* leaf_path(const char *cpath) override
		{
			Path path { cpath };
			if (path == "/new")
				return cpath;

			return File_system::leaf_path(cpath);
		}
};


class Vfs_pipe::Fifo_file_system : public Vfs_pipe::File_system
{
	private:

		struct Fifo_item
		{
			Genode::Registry<Fifo_item>::Element _element;
			Path const path;
			Pipe_space::Id const id;

			Fifo_item(Genode::Registry<Fifo_item> &registry,
			          Path const &path, Pipe_space::Id const &id)
			:
				_element(registry, *this), path(path), id(id)
			{ }
		};

		Genode::Registry<Fifo_item>  _items { };

	protected:

		bool _valid_path(const char *cpath) const  override
		{
			/*
			 * either we have no access control (single file in path)
			 * or we need to verify access control
			 */
			Path io { cpath };
			if (io.has_single_element())
				return true;

			/*
			 * a valid access control path is either
			 * "/.pipename/in/in"
			 * or
			 * "/.pipename/out/out"
			 */
			if (io.base()[1] != '.')
				return false;

			io.strip_last_element();
			if (io.has_single_element())
				return false;

			io.keep_only_last_element();
			if (!(io == "/in" || io == "/out"))
				return false;

			Path io_file { cpath };
			io_file.keep_only_last_element();
			if (io_file == io)
				return true;

			return false;
		}

		virtual bool _pipe_id(const char* cpath, Pipe_space::Id &id) const override
		{
			Path path { cpath };
			if (!path.has_single_element()) {
				/* remove /in/in or /out/out */
				path.strip_last_element();
				path.strip_last_element();
				/* remove the "." from /.pipe_name */
				if (strlen(path.base()) <= 2)
					return false;
				path = Path { path.base() + 2 };
			}

			bool result { false };
			_items.for_each([&path, &id, &result] (Fifo_item const &item) {
				if (item.path == path) {
					id = item.id;
					result = true;
				}
			});
			return result;
		}

	public:

		Fifo_file_system(Vfs::Env &env, Genode::Xml_node const &config)
		:
			File_system(env)
		{
			config.for_each_sub_node("fifo", [&env, this] (Xml_node const &fifo) {
				Path const path { fifo.attribute_value("name", String<MAX_PATH_LEN>()) };

				Pipe &pipe = *new (env.alloc())
					Pipe(env.env(), env.user(), env.alloc(), _pipe_space);
				new (env.alloc())
					Fifo_item(_items, path, pipe.space_elem.id());
			});
		}

		~Fifo_file_system()
		{
			_items.for_each([this] (Fifo_item &item) {
				destroy(_env.alloc(), &item);
			});
		}

		bool directory(char const *cpath) override
		{
			Path const path { cpath };
			if (path == "/") return true;
			if (_valid_path(cpath)) return false;

			Path io { cpath };
			io.keep_only_last_element();
			if (io == "/in") return true;
			if (io == "/out") return true;
			if (!path.has_single_element()) return false;

			return false;
		}

};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node node) override
		{
			if (node.has_sub_node("fifo")) {
				return new (env.alloc()) Vfs_pipe::Fifo_file_system(env, node);
			} else {
				return new (env.alloc()) Vfs_pipe::Pipe_file_system(env);
			}
		}
	};

	static Factory f;
	return &f;
}
