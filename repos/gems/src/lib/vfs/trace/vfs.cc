/*
 * \brief  File system for trace buffer access
 * \author Sebastian Sumpf
 * \date   2019-06-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <vfs/dir_file_system.h>
#include <vfs/single_file_system.h>

#include <os/vfs.h>
#include <util/xml_generator.h>

#include <trace_session/connection.h>

#include "directory_tree.h"
#include "trace_buffer.h"
#include "value_file_system.h"


namespace Vfs_trace {

	using namespace Vfs;
	using namespace Genode;
	using Name = String<32>;

	struct File_system;
	class  Local_factory;
	class  Subject;
	struct Subject_factory;
	class  Trace_buffer_file_system;
}


class Vfs_trace::Trace_buffer_file_system : public Single_file_system
{
	private:

		/**
		 * Trace_buffer wrapper
		 */
		struct Trace_entries
		{
			Vfs::Env                   &_env;
			Constructible<Trace_buffer> _buffer { };

			Trace_entries(Vfs::Env &env) : _env(env) { }

			void setup(Dataspace_capability ds)
			{
				_buffer.construct(*((Trace::Buffer *)_env.env().rm().attach(ds)));
			}

			void flush()
			{
				if (!_buffer.constructed()) return;

				_env.env().rm().detach(_buffer->address());
				_buffer.destruct();
			}

			template <typename FUNC>
			void for_each_new_entry(FUNC && functor, bool update = true) {
				if (!_buffer.constructed()) return;
				_buffer->for_each_new_entry(functor, update);
			}
		};

		enum State { OFF, TRACE, PAUSED } _state { OFF };

		Vfs::Env          &_env;
		Trace::Connection &_trace;
		Trace::Policy_id   _policy;
		Trace::Subject_id  _id;
		size_t             _buffer_size { 1024 * 1024 };
		size_t             _stat_size { 0 };
		Trace_entries      _entries { _env };


		typedef String<32> Config;

		static Config _config()
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), type_name(), [&] () { });

			return Config(Cstring(buf));
		}

		void _setup_and_trace()
		{
			_entries.flush();

			try {
				_trace.trace(_id, _policy, _buffer_size);
			} catch (...) {
				error("failed to start tracing");
				return;
			}

			_entries.setup(_trace.buffer(_id));
		}

	public:

		struct Vfs_handle : Single_vfs_handle
		{
			Trace_entries &_entries;

			Vfs_handle(Directory_service &ds,
			           File_io_service   &fs,
			           Genode::Allocator &alloc,
			           Trace_entries     &entries)
			: Single_vfs_handle(ds, fs, alloc, 0), _entries(entries)
			{ }

			Read_result read(char *dst, file_size count,
			                 file_size &out_count) override
			{
				out_count = 0;
				_entries.for_each_new_entry([&](Trace::Buffer::Entry entry) {
					file_size size = min(count - out_count, entry.length());
					memcpy(dst + out_count, entry.data(), size);
					out_count += size;

					if (out_count == count)
						return false;

					return true;
				});

				return READ_OK;
			}

			Write_result write(char const *, file_size,
			                   file_size &out_count) override
			{
				out_count = 0;
				return WRITE_ERR_INVALID;
			}

			bool read_ready() override { return true; }
		};

		Trace_buffer_file_system(Vfs::Env &env,
		                         Trace::Connection &trace,
		                         Trace::Policy_id policy,
		                         Trace::Subject_id id)
		: Single_file_system(Node_type::TRANSACTIONAL_FILE, type_name(),
		                     Node_rwx::rw(), Xml_node(_config().string())),
		  _env(env), _trace(trace), _policy(policy), _id(id)
		{ }

		static char const *type_name() { return "trace_buffer"; }
		char const *type() override { return type_name(); }


		/***************************
		 ** File-system interface **
		 ***************************/

		Open_result open(char const  *path, unsigned,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path))
				return OPEN_ERR_UNACCESSIBLE;

			*out_handle = new (alloc) Vfs_handle(*this, *this, alloc, _entries);
			return OPEN_OK;
		}

		Stat_result stat(char const *path, Stat &out) override
		{
			Stat_result res = Single_file_system::stat(path, out);
			if (res != STAT_OK) return res;

			/* update file size */
			if (_state == TRACE)
				_entries.for_each_new_entry([&](Trace::Buffer::Entry entry) {
					_stat_size += entry.length(); return true; }, false);

			out.size = _stat_size;

			return res;
		}


		/***********************
		 ** FS event handlers **
		 ***********************/

		bool resize_buffer(size_t size)
		{
			if (size == 0) return false;

			_buffer_size = size;

			switch (_state) {
				case TRACE:
					_trace.pause(_id);
					_setup_and_trace();
					break;
				case PAUSED:
					_state = OFF;
					break;
				case OFF:
					break;
			}
			return true;
		}

		void trace(bool enable)
		{
			if (enable) {
				switch (_state) {
					case TRACE:  break;
					case OFF:    _setup_and_trace(); break;
					case PAUSED: _trace.resume(_id); break;

				}
				_state = TRACE;
			} else {
				switch (_state) {
					case OFF:    return;
					case PAUSED: return;
					case TRACE:  _trace.pause(_id); _state = PAUSED; return;
				}
			}
		}
};


struct Vfs_trace::Subject_factory : File_system_factory
{
	Vfs::Env                               &_env;
	Value_file_system<bool, 6>             _enabled_fs
	  { "enable", "false\n"};

	Value_file_system<Number_of_bytes, 16> _buffer_size_fs
	  { "buffer_size", "1M\n"};

	String<17>                             _buffer_string
	  { _buffer_size_fs.buffer() };

	Trace_buffer_file_system               _trace_fs;

	Subject_factory(Vfs::Env &env,
	                Trace::Connection &trace,
	                Trace::Policy_id policy,
	                Trace::Subject_id id)
	: _env(env), _trace_fs(env, trace, policy, id) { }

	Vfs::File_system *create(Vfs::Env &, Xml_node node) override
	{
		if (node.has_type(Value_file_system<unsigned>::type_name())) {
			if (_enabled_fs.matches(node))     return &_enabled_fs;
			if (_buffer_size_fs.matches(node)) return &_buffer_size_fs;
		}

		if (node.has_type(Trace_buffer_file_system::type_name()))
			return &_trace_fs;

		return nullptr;
	}
};


class Vfs_trace::Subject : private Subject_factory,
                           public  Vfs::Dir_file_system
{
	private:

		typedef String<200> Config;

		Watch_handler<Subject> _enable_handler {
		  _enabled_fs, "/enable",
		  Subject_factory::_env.alloc(),
		  *this, &Subject::_enable_subject };

		Watch_handler<Subject> _buffer_size_handler {
		  _buffer_size_fs, "/buffer_size",
		  Subject_factory::_env.alloc(),
		  *this, &Subject::_buffer_size };


		static Config _config(Xml_node node)
		{
			char buf[Config::capacity()] { };

			Xml_generator xml(buf, sizeof(buf), "dir", [&] () {
				xml.attribute("name", node.attribute_value("name", Vfs_trace::Name()));
				xml.node("value", [&] () { xml.attribute("name", "enable"); });
				xml.node("value", [&] () { xml.attribute("name", "buffer_size"); });
				xml.node(Trace_buffer_file_system::type_name(), [&] () {});
			});

			return Config(Cstring(buf));
		}


		/********************
		 ** Watch handlers **
		 ********************/

		void _enable_subject()
		{
			_enabled_fs.value(_enabled_fs.value() ? "true\n" : "false\n");
			_trace_fs.trace(_enabled_fs.value());
		}

		void _buffer_size()
		{
			Number_of_bytes size = _buffer_size_fs.value();

			if (_trace_fs.resize_buffer(size) == false) {
				/* restore old value */
				_buffer_size_fs.value(_buffer_string);
				return;
			}

			_buffer_string = _buffer_size_fs.buffer();
		}

	public:

		Subject(Vfs::Env &env, Trace::Connection &trace,
		        Trace::Policy_id policy, Xml_node node)
		: Subject_factory(env, trace, policy, node.attribute_value("id", 0u)),
		  Dir_file_system(env, Xml_node(_config(node).string()), *this)
		{ }


		static char const *type_name() { return "trace_node"; }
		char const *type() override { return type_name(); }
};


struct Vfs_trace::Local_factory : File_system_factory
{
	Vfs::Env          &_env;

	Trace::Connection  _trace;
	enum { MAX_SUBJECTS = 128 };
	Trace::Subject_id  _subjects[MAX_SUBJECTS];
	unsigned           _subject_count { 0 };
	Trace::Policy_id   _policy_id { 0 };

	Directory_tree     _tree { _env.alloc() };

	void _install_null_policy()
	{
		using namespace Genode;
		Constructible<Attached_rom_dataspace> null_policy;

		try {
			null_policy.construct(_env.env(), "null");
			_policy_id = _trace.alloc_policy(null_policy->size());
		}
		catch (Out_of_caps) { throw; }
		catch (Out_of_ram)  { throw; }
		catch (...) {
				error("failed to attach 'null' trace policy."
				      "Please make sure it is provided as a ROM module.");
				throw;
		}

		/* copy policy into trace session */
		void *dst = _env.env().rm().attach(_trace.policy(_policy_id));
		memcpy(dst, null_policy->local_addr<void*>(), null_policy->size());
		_env.env().rm().detach(dst);
	}

	size_t _config_session_ram(Xml_node config)
	{
		if (!config.has_attribute("ram")) {
			Genode::error("mandatory 'ram' attribute missing");
			throw Genode::Exception();
		}
		return config.attribute_value("ram", Number_of_bytes(0));
	}

	Local_factory(Vfs::Env &env, Xml_node config)
	: _env(env), _trace(env.env(), _config_session_ram(config), 512*1024, 0)
	{
		bool success = false;
		while (!success) {
			try {
				_subject_count = _trace.subjects(_subjects, MAX_SUBJECTS);
				success = true;
			} catch(Genode::Out_of_ram) {
				_trace.upgrade_ram(4096);
				success = false;
			}
		}

		for (unsigned i = 0; i < _subject_count; i++) {
			_tree.insert(_trace.subject_info(_subjects[i]), _subjects[i]);
		}

		_install_null_policy();
	}

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type(Subject::type_name()))
			return new (_env.alloc()) Subject(_env, _trace, _policy_id, node);

		return nullptr;
	}
};


class Vfs_trace::File_system : private Local_factory,
                               public  Vfs::Dir_file_system
{
	private:

		typedef String<512*1024> Config;

		static char const *_config(Vfs::Env &vfs_env, Directory_tree &tree)
		{
			char *buf = (char *)vfs_env.alloc().alloc(Config::capacity());
			Xml_generator xml(buf, Config::capacity(), "node", [&] () {
				tree.xml(xml);
			});

			return buf;
		}

	public:

		File_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		: Local_factory(vfs_env, node),
			Vfs::Dir_file_system(vfs_env, Xml_node(_config(vfs_env, _tree)), *this)
		{ }

		char const *type() override { return "trace"; }
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
			try { return new (vfs_env.alloc())
				Vfs_trace::File_system(vfs_env, node); }
			catch (...) { Genode::error("could not create 'trace_fs' "); }
			return nullptr;
		}
	};

	static Factory factory;
	return &factory;
}
