/*
 * \brief  Linux host file system
 * \author Norman Feske
 * \author Emery Hemingway
 * \author Sid Hussmann
 * \author Pirmin Duss
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <file_system_session/rpc_object.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/xml_node.h>

/* local includes */
#include "directory.h"
#include "notifier.h"
#include "open_node.h"
#include "watch.h"

namespace Lx_fs {

	using namespace File_system;
	using File_system::Packet_descriptor;
	using File_system::Path;
	using Genode::Attached_ram_dataspace;
	using Genode::Attached_rom_dataspace;

	struct Main;
	class Session_resources;
	class Session_component;
	class Root;

	/**
	 * Convenience utities for parsing quotas
	 */
	Genode::Ram_quota parse_ram_quota(char const *args) {
		return Genode::Ram_quota{ Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0)}; }
	Genode::Cap_quota parse_cap_quota(char const *args) {
		return Genode::Cap_quota{ Genode::Arg_string::find_arg(args, "cap_quota").ulong_value(0)}; }
	Genode::size_t parse_tx_buf_size(char const *args) {
		return Genode::Arg_string::find_arg(args, "tx_buf_size").ulong_value(0); }
}


/**
 * Base class to manage session quotas and allocations
 */
class Lx_fs::Session_resources
{
	protected:

		Genode::Ram_quota_guard           _ram_guard;
		Genode::Cap_quota_guard           _cap_guard;
		Genode::Constrained_ram_allocator _ram_alloc;
		Genode::Attached_ram_dataspace    _packet_ds;
		Genode::Heap                      _alloc;

		Session_resources(Genode::Ram_allocator &ram,
		                  Genode::Region_map    &region_map,
		                  Genode::Ram_quota     ram_quota,
		                  Genode::Cap_quota     cap_quota,
		                  Genode::size_t        buffer_size)
		:
			_ram_guard(ram_quota), _cap_guard(cap_quota),
			_ram_alloc(ram, _ram_guard, _cap_guard),
			_packet_ds(_ram_alloc, region_map, buffer_size),
			_alloc(_ram_alloc, region_map)
		{ }
};


class Lx_fs::Session_component : private Session_resources,
                                 public Session_rpc_object,
                                 private Watch_node::Response_handler
{
	private:

		using Open_node      = File_system::Open_node<Node>;
		using Signal_handler = Genode::Signal_handler<Session_component>;

		Genode::Env                 &_env;
		Directory                   &_root;
		Id_space<File_system::Node>  _open_node_registry { };
		bool                         _writeable;
		Absolute_path const          _root_dir;
		Signal_handler               _process_packet_dispatcher;
		Notifier                    &_notifier;

		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 *
		 * \return true on success, false on failure
		 */
		void _process_packet_op(Packet_descriptor &packet, Open_node &open_node)
		{
			size_t const length  = packet.length();

			/* resulting length */
			size_t res_length = 0;
			bool succeeded = false;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				if (tx_sink()->packet_valid(packet) && (packet.length() <= packet.size())) {
					res_length = open_node.node().read((char *)tx_sink()->packet_content(packet), length,
					                                   packet.position());

					/* read data or EOF is a success */
					succeeded = res_length || (packet.position() >= open_node.node().status().size);
				}
				break;

			case Packet_descriptor::WRITE:
				if (tx_sink()->packet_valid(packet) && (packet.length() <= packet.size())) {
					res_length = open_node.node().write((char const *)tx_sink()->packet_content(packet),
					                                    length,
					                                    packet.position());

					/* File system session can't handle partial writes */
					if (res_length != length) {
						/* don't acknowledge */
						return;
					}
					succeeded = true;
				}
				break;

			case Packet_descriptor::WRITE_TIMESTAMP:
				if (tx_sink()->packet_valid(packet) && (packet.length() <= packet.size())) {

					packet.with_timestamp([&] (File_system::Timestamp const time) {
						open_node.node().update_modification_time(time);
						succeeded = true;
					});
				}
				break;

			case Packet_descriptor::CONTENT_CHANGED:
				open_node.register_notify(*tx_sink());
				/* notify_listeners may bounce the packet back*/
				open_node.node().notify_listeners();
				/* otherwise defer acknowledgement of this packet */
				return;

			case Packet_descriptor::READ_READY:
				succeeded = true;
				/* not supported */
				break;

			case Packet_descriptor::SYNC:

				if (tx_sink()->packet_valid(packet)) {
					succeeded = open_node.node().sync();
				}

				break;
			}

			packet.length(res_length);
			packet.succeeded(succeeded);
			tx_sink()->acknowledge_packet(packet);
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			auto process_packet_fn = [&] (Open_node &open_node) {
				_process_packet_op(packet, open_node);
			};

			try {
				_open_node_registry.apply<Open_node>(packet.handle(), process_packet_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				Genode::error("Invalid_handle");
				tx_sink()->acknowledge_packet(packet);
			}
		}

		/**
		 * Called by signal dispatcher, executed in the context of the main
		 * thread (not serialized with the RPC functions)
		 */
		void _process_packets()
		{
			while (tx_sink()->packet_avail()) {

				/*
				 * Make sure that the '_process_packet' function does not
				 * block.
				 *
				 * If the acknowledgement queue is full, we defer packet
				 * processing until the client processed pending
				 * acknowledgements and thereby emitted a ready-to-ack
				 * signal. Otherwise, the call of 'acknowledge_packet()'
				 * in '_process_packet' would infinitely block the context
				 * of the main thread. The main thread is however needed
				 * for receiving any subsequent 'ready-to-ack' signals.
				 */
				if (!tx_sink()->ready_to_ack())
					return;

				_process_packet();
			}
		}

		/**
		 * Check if string represents a valid path (must start with '/')
		 */
		static void _assert_valid_path(char const *path)
		{
			if (!path || path[0] != '/') {
				Genode::warning("malformed path '", path, "'");
				throw Lookup_failed();
			}
		}

		/**
		 * Watch_node::Response_handler interface
		 */
		void handle_watch_node_response(Lx_fs::Watch_node &node) override
		{
			using Fs_node = File_system::Open_node<Lx_fs::Node>;
			_process_packet_op(node.acked_packet(),
			                   *(reinterpret_cast<Fs_node*>(node.open_node())));
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env         &env,
		                  Genode::Ram_quota    ram_quota,
		                  Genode::Cap_quota    cap_quota,
		                  size_t               tx_buf_size,
		                  char const          *root_dir,
		                  bool                 writeable,
		                  Notifier            &notifier)
		:
			Session_resources { env.pd(), env.rm(), ram_quota, cap_quota, tx_buf_size },
			Session_rpc_object {_packet_ds.cap(), env.rm(), env.ep().rpc_ep() },
			_env { env },
			_root { *new (&_alloc) Directory { _alloc, root_dir, false } },
			_writeable { writeable },
			_root_dir { root_dir },
			_process_packet_dispatcher { env.ep(), *this, &Session_component::_process_packets },
			_notifier { notifier }
		{
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			List<List_element<Open_node>> node_list;

			auto collect_fn = [&node_list, this] (Open_node &open_node) {
				node_list.insert(new (_alloc) List_element<Open_node>{ &open_node });
			};

			try {
				_open_node_registry.for_each<Open_node>(collect_fn);

				while (node_list.first()) {

					auto *e = node_list.first();
					Node &node = e->object()->node();
					destroy(_alloc, e->object());
					destroy(_alloc, &node);
					node_list.remove(e);
					destroy(_alloc, e);
				}

			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				error("invalid handle while cleaning up");
			}

			destroy(&_alloc, &_root);
		}

		/**
		 * Increase quotas
		 */
		void upgrade(Genode::Ram_quota ram)  { _ram_guard.upgrade(ram); }
		void upgrade(Genode::Cap_quota caps) { _cap_guard.upgrade(caps); }

		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create) override
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			auto file_fn = [&] (Open_node &open_node) {

				Node &dir = open_node.node();

				if (!_writeable)
					if (create || (mode != STAT_ONLY && mode != READ_ONLY))
						throw Permission_denied();

				File *file = dir.file(name.string(), mode, create);

				Open_node *open_file =
					new (_alloc) Open_node(*file, _open_node_registry);

				return open_file->id();
			};

			try {
				return File_handle {
					_open_node_registry.apply<Open_node>(dir_handle, file_fn).value
				};
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		Symlink_handle symlink(Dir_handle, Name const &, bool /* create */) override
		{
			/*
			 * We do not support symbolic links for security reasons.
			 */
			Genode::error(__func__, " not implemented");
			throw Permission_denied();
		}

		Dir_handle dir(Path const &path, bool create) override
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			/* skip leading '/' */
			path_str++;

			if (!_writeable && create)
				throw Permission_denied();

			if (!path.valid_string())
				throw Name_too_long();

			Directory *dir = _root.subdir(path_str, create);

			Open_node *open_dir =
				new (_alloc) Open_node(*dir, _open_node_registry);

			return Dir_handle { open_dir->id().value };
		}

		Node_handle node(Path const &path) override
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			Node *node = _root.node(path_str + 1);

			Open_node *open_node =
				new (_alloc) Open_node(*node, _open_node_registry);

			return open_node->id();
		}

		Watch_handle watch(Path const &path) override
		{
			_assert_valid_path(path.string());

			/* re-root the path */
			Path_string watch_path { _root.path().string(), path.string() };

			Watch_node *watch =
				new (_alloc) Watch_node { _env, watch_path.string(), *this, _notifier };
			Lx_fs::Open_node<Watch_node>  *open_watch =
				new (_alloc) Lx_fs::Open_node<Watch_node>(*watch, _open_node_registry);

			Watch_handle handle { open_watch->id().value };
			watch->open_node(open_watch);

			return handle;
		}

		void close(Node_handle handle) override
		{
			auto close_fn = [&] (Open_node &open_node) {
				Node &node = open_node.node();
				destroy(_alloc, &open_node);
				destroy(_alloc, &node);
			};

			try {
				_open_node_registry.apply<Open_node>(handle, close_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		Status status(Node_handle node_handle) override
		{
			auto status_fn = [&] (Open_node &open_node) {
				return open_node.node().status();
			};

			try {
				return _open_node_registry.apply<Open_node>(node_handle, status_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void control(Node_handle, Control) override
		{
			Genode::error(__func__, " not implemented");
		}

		void unlink(Dir_handle dir_handle, Name const &name) override
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			if (!_writeable)
				throw Permission_denied();

			auto unlink_fn = [&] (Open_node &open_node) {

				Absolute_path absolute_path("/");

				try {
					absolute_path.append(open_node.node().path().string());
					absolute_path.append("/");
					absolute_path.append(name.string());
				} catch (Path_base::Path_too_long const &) {
					Genode::error("Path too long. path=", absolute_path);
					throw Invalid_name();
				}

				char const *path_str = absolute_path.string();
				_assert_valid_path(path_str);

				struct stat s;
				int ret = lstat(path_str, &s);
				if (ret == -1) {
					throw Lookup_failed();
				}

				if (S_ISDIR(s.st_mode)) {
					ret = rmdir(path_str);
				} else if (S_ISREG(s.st_mode) || S_ISLNK(s.st_mode)) {
					ret = ::unlink(path_str);
				} else {
					throw Lookup_failed();
				}

				if (ret == -1) {
					auto err = errno;
					if (err == EACCES)
						throw Permission_denied();
				}
			};

			try {
				_open_node_registry.apply<Open_node>(dir_handle, unlink_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void truncate(File_handle file_handle, file_size_t size) override
		{
			if (!_writeable)
				throw Permission_denied();

			auto truncate_fn = [&] (Open_node &open_node) {
				open_node.node().truncate(size);
			};

			try {
				_open_node_registry.apply<Open_node>(file_handle, truncate_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void move(Dir_handle dir_from, Name const & name_from,
		          Dir_handle dir_to,   Name const & name_to) override
		{
			typedef File_system::Open_node<Directory> Dir_node;

			Directory *to = 0;

			auto to_fn = [&] (Dir_node &dir_node) {
				to = &dir_node.node();
			};

			try {
				_open_node_registry.apply<Dir_node>(dir_to, to_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}

			auto move_fn = [&] (Dir_node &dir_node) {
				dir_node.node().rename(*to, name_from.string(), name_to.string());
			};

			try {
				_open_node_registry.apply<Dir_node>(dir_from, move_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}
};


class Lx_fs::Root : public Root_component<Session_component>
{
	private:

		Genode::Env                    &_env;
		Genode::Attached_rom_dataspace  _config   { _env, "config" };
		Notifier                        _notifier { _env };

		static inline bool writeable_from_args(char const *args)
		{
			return { Arg_string::find_arg(args, "writeable").bool_value(true) };
		}

	protected:

		Session_component *_create_session(const char *args) override
		{
			/*
			 * Determine client-specific policy defined implicitly by
			 * the client's label.
			 */

			char const *root_dir  = ".";
			bool        writeable = false;

			Session_label  const label = label_from_args(args);
			Session_policy const policy(label, _config.xml());

			if (!policy.has_attribute("root")) {
				Genode::error("missing \"root\" attribute in policy definition");
				throw Service_denied();
			}

			/*
			 * Determine directory that is used as root directory of
			 * the session.
			 */
			typedef String<256> Root;
			Root const root = policy.attribute_value("root", Root());

			/*
			 * Make sure the root path is specified with a
			 * leading path delimiter. For performing the
			 * lookup, we remove all leading slashes.
			 */
			if (root.string()[0] != '/') {
				Genode::error("Root directory must start with / but is \"", root, "\"");
				throw Service_denied();
			}

			for (root_dir = root.string(); *root_dir == '/'; ++root_dir) ;

			/* sanitize possibly empty root_dir to current directory */
			if (*root_dir == 0)
				root_dir = ".";

			/*
			 * Determine if write access is permitted for the session.
			 */
			writeable = policy.attribute_value("writeable", false) &&
			            writeable_from_args(args);

			auto const initial_ram_usage { _env.pd().used_ram().value };
			auto const initial_cap_usage { _env.pd().used_caps().value };
			auto const ram_quota         { parse_ram_quota(args).value };
			auto const cap_quota         { parse_cap_quota(args).value };
			auto const tx_buf_size       { parse_tx_buf_size(args) };

			if (!tx_buf_size) {
				Genode::error(label, " requested a session with a zero length transmission buffer");
				throw Genode::Service_denied();
			}

			/*
			 * Check if donated ram quota suffices for communication buffer.
			 */
			if (tx_buf_size > ram_quota) {
				Genode::error("insufficient 'ram_quota', "
				              "got ", ram_quota, ", need ", tx_buf_size);
				throw Insufficient_ram_quota();
			}

			try {
				auto session = new (md_alloc())
				       Session_component { _env,
				                           Genode::Ram_quota { ram_quota },
				                           Genode::Cap_quota { cap_quota },
				                           tx_buf_size,
				                           absolute_root_dir(root_dir).string(),
				                           writeable, _notifier };

				auto ram_used { _env.pd().used_ram().value - initial_ram_usage };
				auto cap_used { _env.pd().used_caps().value - initial_cap_usage };

				if ((ram_used > ram_quota) || (cap_used > cap_quota)) {
					if (ram_used > ram_quota)
						Genode::warning("ram donation is ", ram_quota,
						                " but used RAM is ", ram_used, "B"
						                ", '", label, "'");
					if (cap_used > cap_quota)
						Genode::warning("cap donation is ", cap_quota,
						                " but used caps is ", cap_used,
						                ", '", label, "'");
				}

				return session;
			}
			catch (Lookup_failed) {
				Genode::error("session root directory \"", root, "\" "
				              "does not exist");
				throw Service_denied();
			}
		}

		void _destroy_session(Session_component *session) override
		{
			Genode::destroy(md_alloc(), session);
		}

		/**
		 * Session upgrades are important for the lx_fs server,
		 * this allows sessions to open arbitrarily large amounts
		 * of handles without starving other sessions.
		 */
		void _upgrade_session(Session_component *session,
		                      char        const *args) override
		{
			Genode::Ram_quota more_ram  { parse_ram_quota(args) };
			Genode::Cap_quota more_caps { parse_cap_quota(args) };

			if (more_ram.value > 0)
				session->upgrade(more_ram);
			if (more_caps.value > 0)
				session->upgrade(more_caps);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};


struct Lx_fs::Main
{
	Genode::Env &env;

	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root fs_root = { env, sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(fs_root));
	}
};


void Component::construct(Genode::Env &env) { static Lx_fs::Main inst(env); }
