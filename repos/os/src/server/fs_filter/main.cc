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
#include <os/session_policy.h>
#include <util/xml_node.h>
#include <base/component.h>
//#include <base/env.h>

/* local includes */
#include "node.h"
#include "session.h"

using namespace Genode;

namespace File_system {

	struct Main;

	class Root : public Root_component<Session_component>
	{
		private:

			Env                     &_env;
			Allocator_avl           &_avl;
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

					Session_component *comp = new (md_alloc()) Session_component(tx_buf_size, _env, _avl, policy);
					_sessions.insert(comp);
					return comp;
				} catch (Session_policy::No_policy_defined) {
					PERR("Invalid session request, no matching policy");
					throw Root::Unavailable();
				}
			}
			
			/* TODO: add destroy session code */
			
	public:
			Root(Env &env, Allocator &md_alloc, Allocator_avl &avl)
			:
				Root_component<Session_component>(env.ep(), md_alloc),
				_env(env), _avl(avl)
			{ }
			
			void handle_config_update() {
				
			}
	};
};

struct File_system::Main
{
	Env        &env;
	Entrypoint &ep;

	//Directory root_dir = { "" };

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap { env.ram(), env.rm() };
	Allocator_avl avl { &sliced_heap };

	Root fs_root { env, sliced_heap, avl };

	void handle_config_update()
	{
		config()->reload();
		try {
			for (Xml_node service_node = config()->xml_node().sub_node("fs");;
			     service_node = service_node.next("fs")) {

				char label_buf[64];
				service_node.attribute("label").value(label_buf, sizeof(label_buf));
				FS_reference::add_fs(env, avl, label_buf);
			}
		} catch (Xml_node::Nonexistent_sub_node) { }

		fs_root.handle_config_update();
	}

	Signal_handler<Main> config_update_dispatcher =
		{ ep, *this, &Main::handle_config_update };

	Main(Env &env) : env(env), ep(env.ep())
	{
		handle_config_update();
		Genode::config()->sigh(config_update_dispatcher);
		
		env.parent().announce(ep.manage(fs_root));
	}
};

Genode::size_t Component::stack_size()                         { return 2048 * sizeof(long); }
void           Component::construct(Env &env) { static File_system::Main inst(env); }
