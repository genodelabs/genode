/*
 * \brief  filesystem filter/router
 * \author Ben Larson
 * \date   2016-05-13
 */

#include <file_system/node_handle_registry.h>
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <util/xml_node.h>
//#include <base/env.h>

/* local includes */
#include "node.h"

using namespace Genode;

namespace File_system {

	struct Attachment : public List<Attachment>::Element
	{
		String fs_label;
		String fs_path;
	
		Attachment(char *label, char *path): fs_label(label), fs_path(path) { }
	}
	
	class Session_component : public Session_rpc_object, public List<Session_component>::Element
	{
		friend class Mixed_directory;
	private:

		Server::Entrypoint   &_ep;
		Connection           *_root_fs;
		Directory            &_root_dir;
		Node_handle_registry  _handle_registry;
		bool                  _writable = false; /* default to read-only */
		List<Attachment>      _attachments;
		//List<FS_reference>   &_filesystems;

		Signal_rpc_member<Session_component> _process_packet_dispatcher;

		void _process_packets(unsigned)
		{
			while (tx_sink()->packet_avail() && (!tx_sink()->ready_to_ack())) {
				Packet_descriptor packet = tx_sink()->peek_packet();
				Node *node = _handle_registry.lookup(packet.handle());
				if (!node) {
					tx_sink()->get_packet(); /* dequeue */
					continue;
				}
				Session::Tx::Source &source = *node->fs->tx();
				if (!source->ready_to_submit()) return;
				/* TODO: send packet and prepare to pass acknowledgement back to client */
			}
		}
		
		void _update_policy(Session_policy policy)
		{
			try {
				_writable = policy.attribute("writeable").has_value("yes"); /* use spelling consistent with other filesystem servers in config */
			} catch (Xml_node::Nonexistent_attribute) {
				_writable = false;
				char root_fs[64];
			}
			try {
				char root_path[256];
				
				policy.attribute("root_fs").value(root_fs, sizeof(root_fs));
				_root_fs = FS_reference::get_fs(root_fs);
				
				policy.attribute("root_path").value(root_path, sizeof(root_path));

				/*
				 * Make sure the root path is specified with a
				 * leading path delimiter. For performing the
				 * lookup, we skip the first character.
				 */
				if (root[0] != '/')
					_root_dir = Directory(); /* make invalid if update fails */
					throw Lookup_failed();

				_root_dir = Directory(_root_fs, _root_fs->dir(root_path)); /* TODO: switch to Mixed_directory */
				
				/* TODO: update attached nodes list */
				try {
					for (Xml_node service_node = config()->xml_node().sub_node("attach");;
						service_node = service_node.next("attach")) {
						
					}
				} catch (Xml_node::Nonexistent_sub_node) { }
				
			} catch (Xml_node::Nonexistent_attribute) {
				PERR("Missing \"root\" attribute in policy definition");
				_root_dir = Directory(); /* make invalid if update fails */
				throw Root::Unavailable();
			} catch (Lookup_failed) {
				PERR("Session root directory \"%s\" does not exist", root);
				_root_dir = Directory(); /* make invalid if update fails */
				throw Root::Unavailable();
			}
		}

	public:
		Session_component(size_t tx_buf_size, Server::Entrypoint &ep,
		                  Session_policy policy)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_ep(ep),
			_process_packet_dispatcher(ep, *this, &Session_component::_process_packets)
		{
			_update_policy(policy);
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
		}

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			/* TODO: dummy--replace with relevant code */
			if (!valid_name(name.string()))
				throw Invalid_name();
			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (!_writable)
				if (mode != STAT_ONLY && mode != READ_ONLY)
					throw Permission_denied();

			if (create) {

				if (!_writable)
					throw Permission_denied();

				if (dir->has_sub_node_unsynchronized(name.string()))
					throw Node_already_exists();

				try {
					File * const file = new (env()->heap())
					                    File(*env()->heap(), name.string());

					dir->adopt_unsynchronized(file);
				}
				catch (Allocator::Out_of_memory) { throw No_space(); }
			}

			File *file = dir->lookup_and_lock_file(name.string());
			Node_lock_guard file_guard(file);
			return _handle_registry.alloc(file);
		}
	};
	
	
	class Root : public Root_component<Session_component>
	{
		private:

			Server::Entrypoint       &_ep;
			List<Session_component>  _sessions;
			List<FS_reference>       _filesystems;

		protected:

			Session_component *_create_session(const char *args)
			{
				Session_label label(args);
				try {
					Session_policy policy(label);

					size_t ram_quota =
						Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
					size_t tx_buf_size =
						Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

					if (!tx_buf_size) {
						PERR("%s requested a session with a zero length transmission buffer", label.string());
						throw Root::Invalid_args();
					}

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

					Session_component *comp = new (md_alloc()) Session_component(tx_buf_size, _ep, policy);
					_sessions.insert(comp);
					return comp;
				} catch (Session_policy::No_policy_defined) {
					PERR("Invalid session request, no matching policy");
					throw Root::Unavailable();
				}
			}
			
	public:
			Root(Server::Entrypoint &ep, Allocator &md_alloc)
			:
				Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
				_ep(ep),
			{ }
			
			void handle_config_update() {
				
			}
	};
}

struct File_system::Main
{
	Server::Entrypoint &ep;

	//Directory root_dir = { "" };

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root = { ep, sliced_heap };

	void handle_config_update(unsigned)
	{
		Genode::config()->reload();
		try {
			for (Xml_node service_node = config()->xml_node().sub_node("fs");;
			     service_node = service_node.next("fs")) {

				char label_buf[64];
				service_node.attribute("label").value(label_buf, sizeof(label_buf));
				FS_reference::add_fs(label_buf);
			}
		} catch (Xml_node::Nonexistent_sub_node) { }

		fs_root.handle_config_update();
	}

	Signal_rpc_member<Main> config_update_dispatcher =
		{ ep, *this, &Main::handle_config_update};

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		handle_config_update(0);
		Genode::config()->sigh(config_update_dispatcher);
		
		env()->parent()->announce(ep.manage(fs_root));
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "ram_fs_ep"; }
Genode::size_t Server::stack_size()                      { return 2048 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static File_system::Main inst(ep); }
