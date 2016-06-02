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
			
			/* TODO: add destroy session code */
			
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
