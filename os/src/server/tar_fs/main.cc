/*
 * \brief  TAR file system
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2012-08-20
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <os/session_policy.h>
#include <util/xml_node.h>

/* local includes */
#include <directory.h>
#include <file.h>
#include <lookup.h>
#include <node_handle_registry.h>
#include <symlink.h>
#include <util.h>


using namespace Genode;


static bool const verbose = false;
#define PDBGV(...) if (verbose) PDBG(__VA_ARGS__)


/*************************************
 ** Helpers for dispatching signals **
 *************************************/

namespace Genode {

	struct Signal_dispatcher_base : Signal_context
	{
		virtual void dispatch(int num) = 0;
	};


	template <typename T>
	class Signal_dispatcher : private Signal_dispatcher_base,
	                          public  Signal_context_capability
	{
		private:

			T &obj;
			void (T::*member) (int);
			Signal_receiver &sig_rec;

		public:

			/**
			 * Constructor
			 *
			 * \param sig_rec     signal receiver to associate the signal
			 *                    handler with
			 * \param obj,member  object and member function to call when
			 *                    the signal occurs
			 */
			Signal_dispatcher(Signal_receiver &sig_rec,
			                  T &obj, void (T::*member)(int))
			:
				Signal_context_capability(sig_rec.manage(this)),
				obj(obj), member(member),
				sig_rec(sig_rec)
			{ }

			~Signal_dispatcher() { sig_rec.dissolve(this); }

			void dispatch(int num) { (obj.*member)(num); }
	};
}


/*************************
 ** File-system service **
 *************************/

namespace File_system {

	char  *_tar_base;
	size_t _tar_size;

	class Session_component : public Session_rpc_object
	{
		private:

			Directory            &_root;
			Node_handle_registry  _handle_registry;

			Signal_dispatcher<Session_component> _process_packet_dispatcher;


			/******************************
			 ** Packet-stream processing **
			 ******************************/

			/**
			 * Perform packet operation
			 *
			 * \return true on success, false on failure
			 */
			void _process_packet_op(Packet_descriptor &packet, Node &node)
			{
				void     * const content = tx_sink()->packet_content(packet);
				size_t     const length  = packet.length();
				seek_off_t const offset  = packet.position();

				if (!content || (packet.length() > packet.size())) {
					packet.succeeded(false);
					return;
				}

				/* resulting length */
				size_t res_length = 0;

				switch (packet.operation()) {

					case Packet_descriptor::READ:
						PDBGV("READ");
						res_length = node.read((char *)content, length, offset);
						break;

					case Packet_descriptor::WRITE:
						PDBGV("WRITE");
						res_length = node.write((char const *)content, length, offset);
						break;
				}

				packet.length(res_length);
				packet.succeeded(res_length > 0);
			}

			void _process_packet()
			{
				Packet_descriptor packet = tx_sink()->get_packet();

				/* assume failure by default */
				packet.succeeded(false);

				try {
					Node *node = _handle_registry.lookup(packet.handle());
					_process_packet_op(packet, *node);
				}
				catch (Invalid_handle)     { PERR("Invalid_handle");     }
				catch (Size_limit_reached) { PERR("Size_limit_reached"); }

				/*
				 * The 'acknowledge_packet' function cannot block because we
				 * checked for 'ready_to_ack' in '_process_packets'.
				 */
				tx_sink()->acknowledge_packet(packet);
			}

			/**
			 * Called by signal dispatcher, executed in the context of the main
			 * thread (not serialized with the RPC functions)
			 */
			void _process_packets(int)
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
			 * Check if string represents a valid path (most start with '/')
			 */
			static void _assert_valid_path(char const *path)
			{
				if (!valid_path(path)) {
					PWRN("malformed path '%s'", path);
					throw Lookup_failed();
				}
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(size_t tx_buf_size, Rpc_entrypoint &ep,
			                  Signal_receiver &sig_rec,
			                  Directory &root)
			:
				Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep),
				_root(root),
				_process_packet_dispatcher(sig_rec, *this,
				                           &Session_component::_process_packets)
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
				Dataspace_capability ds = tx_sink()->dataspace();
				env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
			}


			/***************************
			 ** File_system interface **
			 ***************************/

			File_handle file(Dir_handle dir_handle, Name const &name,
			                 Mode mode, bool create)
			{
				PDBGV("_root = %s, dir_name = %s, name = %s, create = %d",
 				      _root.record()->name(),
				      _handle_registry.lookup(dir_handle)->record()->name(),
				      name.string(),
				      create);

				if (!valid_filename(name.string()))
					throw Lookup_failed();

				if (create)
					throw Permission_denied();

				Directory *dir = _handle_registry.lookup(dir_handle);

				Absolute_path abs_path(dir->record()->name());
				try {
					abs_path.append("/");
					abs_path.append(name.base());
				} catch (Path_base::Path_too_long) {
					throw Name_too_long();
				}

				PDBGV("abs_path = %s", abs_path.base());

				Lookup_exact lookup_criterion(abs_path.base());
				Record *record = _lookup(&lookup_criterion);

				if (!record) {
					PERR("Could not find record for %s", abs_path.base());
					throw Lookup_failed();
				}

				if (record->type() != Record::TYPE_FILE)
					throw Lookup_failed();

				File *file_node = new (env()->heap()) File(record);
				return _handle_registry.alloc(file_node);
			}

			Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
			{
				PDBGV("_root = %s, dir_name = %s, name = %s, create = %d",
 				      _root.record()->name(),
				      _handle_registry.lookup(dir_handle)->record()->name(),
				      name.string(),
				      create);

				if (!valid_filename(name.string()))
					throw Lookup_failed();

				if (create)
					throw Permission_denied();

				Directory *dir = _handle_registry.lookup(dir_handle);

				Absolute_path abs_path(dir->record()->name());
				try {
					abs_path.append("/");
					abs_path.append(name.base());
				} catch (Path_base::Path_too_long) {
					throw Name_too_long();
				}

				PDBGV("abs_path = %s", abs_path.base());

				Lookup_exact lookup_criterion(abs_path.base());
				Record *record = _lookup(&lookup_criterion);

				if (!record) {
					PERR("Could not find record for %s", abs_path.base());
					throw Lookup_failed();
				}

				if (record->type() != Record::TYPE_SYMLINK)
					throw Lookup_failed();

				Symlink *symlink_node = new (env()->heap()) Symlink(record);
				return _handle_registry.alloc(symlink_node);
			}

			Dir_handle dir(Path const &path, bool create)
			{
				PDBGV("_root = %s, path = %s, create = %d",
					  _root.record()->name(), path.string(), create);

				_assert_valid_path(path.string());

				if (create)
					throw Permission_denied();

				Absolute_path abs_path(_root.record()->name());
				try {
					abs_path.append(path.string());
				} catch (Path_base::Path_too_long) {
					throw Name_too_long();
				}

				Lookup_exact lookup_criterion(abs_path.base());
				Record *record = _lookup(&lookup_criterion);

				if (!record) {
					PERR("Could not find record for %s", path.string());
					throw Lookup_failed();
				}

				if (record->type() != Record::TYPE_DIR)
					throw Lookup_failed();

				Directory *dir_node = new (env()->heap()) Directory(record);

				return _handle_registry.alloc(dir_node);
			}

			Node_handle node(Path const &path)
			{
				PDBGV("path = %s", path.string());

				if (!valid_path(path.string()) &&
				    !valid_filename(path.string()))
					throw Lookup_failed();

				Absolute_path abs_path(_root.record()->name());
				try {
					abs_path.append(path.string());
				} catch (Path_base::Path_too_long) {
					throw Lookup_failed();
				}

				PDBGV("abs_path = %s", abs_path.base());

				Lookup_exact lookup_criterion(abs_path.base());
				Record *record = _lookup(&lookup_criterion);

				if (!record) {
					PERR("Could not find record for %s", path.string());
					throw Lookup_failed();
				}

				Node *node = new (env()->heap()) Node(record);

				return _handle_registry.alloc(node);
			}

			void close(Node_handle handle)
			{
				Node *node;

				try {
					node = _handle_registry.lookup(handle);
				} catch(Invalid_handle) {
					PERR("close() called with invalid handle");
					return;
				}

				PDBGV("name = %s", node->record()->name());

				/* free the handle */
				_handle_registry.free(handle);

				Directory *dir = dynamic_cast<Directory *>(node);
				if (dir) {
					/* free the node */
					destroy(env()->heap(), dir);
					return;
				}

				File *file = dynamic_cast<File *>(node);
				if (file) {
					/* free the node */
					destroy(env()->heap(), file);
					return;
				}

				/* free the node */
				destroy(env()->heap(), node);
			}

			Status status(Node_handle node_handle)
			{
				Status status;
				status.inode = 1;
				status.size  = 0;
				status.mode  = 0;

				Node *node = _handle_registry.lookup(node_handle);

				status.size = node->record()->size();

				/* convert TAR record modes to stat modes */
				switch (node->record()->type()) {
					case Record::TYPE_DIR:     status.mode |= Status::MODE_DIRECTORY; break;
					case Record::TYPE_FILE:    status.mode |= Status::MODE_FILE; break;
					case Record::TYPE_SYMLINK: status.mode |= Status::MODE_SYMLINK; break;
					default:
						if (verbose)
							PWRN("unhandled record type %d", node->record()->type());
				}

				PDBGV("name = %s", node->record()->name());

				return status;
			}

			void control(Node_handle, Control) { }

			void unlink(Dir_handle dir_handle, Name const &name)
			{
				PDBGV("name = %s", name.string());

				throw Permission_denied();
			}

			void truncate(File_handle file_handle, file_size_t size)
			{
				PDBGV("truncate()");

				throw Permission_denied();
			}

			void move(Dir_handle from_dir_handle, Name const &from_name,
			          Dir_handle to_dir_handle,   Name const &to_name)
			{
				PDBGV("from_name = %s, to_name = %s", from_name.string(), to_name.string());

				throw Permission_denied();
			}
	};


	class Root : public Root_component<Session_component>
	{
		private:

			Rpc_entrypoint  &_channel_ep;
			Signal_receiver &_sig_rec;
			Directory       &_root_dir;

		protected:

			Session_component *_create_session(const char *args)
			{
				/*
				 * Determine client-specific policy defined implicitly by
				 * the client's label.
				 */
				Directory *session_root_dir = 0;

				enum { ROOT_MAX_LEN = 256 };
				char root[ROOT_MAX_LEN];
				root[0] = 0;

				try {
					Session_policy policy(args);

					/*
					 * Determine directory that is used as root directory of
					 * the session.
					 */
					try {
						policy.attribute("root").value(root, sizeof(root));
						if (is_root(root)) {
							session_root_dir = &_root_dir;
						} else {
							/*
							 * Make sure the root path is specified with a
							 * leading path delimiter. For performing the
							 * lookup, we skip the first character.
							 */
							if (root[0] != '/')
								throw Lookup_failed();

							/* TODO: lookup root directory */
							Lookup_exact lookup_criterion(root);
							Record *record = _lookup(&lookup_criterion);
							if (!record) {
								PERR("Could not find record for %s", root);
								throw Lookup_failed();
							}

							session_root_dir = new (env()->heap()) Directory(record);
						}
					} catch (Xml_node::Nonexistent_attribute) {
						PERR("Missing \"root\" attribute in policy definition");
						throw Root::Unavailable();
					} catch (Lookup_failed) {
						PERR("Session root directory \"%s\" does not exist", root);
						throw Root::Unavailable();
					}

				} catch (Session_policy::No_policy_defined) {
					PERR("Invalid session request, no matching policy");
					throw Root::Unavailable();
				}

				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

				/*
				 * Check if donated ram quota suffices for session data,
				 * and communication buffer.
				 */
				size_t session_size = sizeof(Session_component) + tx_buf_size;
				if (max((size_t)4096, session_size) > ram_quota) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, session_size);
					throw Root::Quota_exceeded();
				}
				return new (md_alloc())
					Session_component(tx_buf_size, _channel_ep, _sig_rec,
					                  *session_root_dir);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  session entrypoint
			 * \param sig_rec     signal receiver used for handling the
			 *                    data-flow signals of packet streams
			 * \param md_alloc    meta-data allocator
			 */
			Root(Rpc_entrypoint &session_ep, Allocator &md_alloc,
			     Signal_receiver &sig_rec, Directory &root_dir)
			:
				Root_component<Session_component>(&session_ep, &md_alloc),
				_channel_ep(session_ep), _sig_rec(sig_rec), _root_dir(root_dir)
			{ }
	};
};


int main(int, char **)
{
	using namespace File_system;

	enum { STACK_SIZE = 3*sizeof(addr_t)*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "tar_fs_ep");
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());
	static Signal_receiver sig_rec;

	/* read name of tar archive from config */
	enum { TAR_FILENAME_MAX_LEN = 64 };
	static char tar_filename[TAR_FILENAME_MAX_LEN];
	try {
		Xml_node archive_node = config()->xml_node().sub_node("archive");
		try {
			archive_node.attribute("name").value(tar_filename, sizeof(tar_filename));
		} catch (...) {
			PERR("Could not read 'name' attribute of 'archive' config node");
			return -1;
		}
	} catch (...) {
		PERR("Could not read 'archive' config node");
		return -1;
	}

	/* obtain dataspace of tar archive from ROM service */
	try {
		static Rom_connection tar_rom(tar_filename);
		_tar_base = env()->rm_session()->attach(tar_rom.dataspace());
		_tar_size = Dataspace_client(tar_rom.dataspace()).size();
	} catch (...) {
		PERR("Could not obtain tar archive from ROM service");
		return -2;
	}

	PINF("using tar archive '%s' with size %zd", tar_filename, _tar_size);

	static Record root_record; /* every member is 0 */
	static Directory root_dir(&root_record);

	static File_system::Root root(ep, sliced_heap, sig_rec, root_dir);

	env()->parent()->announce(ep.manage(&root));

	for (;;) {
		Signal s = sig_rec.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}
