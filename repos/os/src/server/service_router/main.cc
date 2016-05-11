/*
 * \brief  Service router
 * \author Ben Larson
 * \date   2016-03-15
 */
 
#include <base/env.h>
#include <base/sleep.h>
#include <parent/parent.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <root/root.h>
#include <os/server.h>
#include <os/config.h>
//#include <input_session/input_session.h>
#include <input/root.h>
#include <base/service.h>
#include <util/arg_string.h>
#include <os/session_policy.h>


using namespace Genode;

namespace Service_router {

	struct Root : Genode::Root
	{
		Root() { }
	};

	class Service_source : public Rpc_object<Service_router::Root>, public List<Service_source>::Element /* may need to add ram quota handling */
	{

		public:
		char _name[64];
		Session_capability session(Session_args const &args, Affinity const &affinity) override
		{
			char args_buf[Parent::Session_args::MAX_SIZE];
			strncpy(args_buf, args.string(), sizeof(args_buf));
			Session_label in_label(args_buf);
			try {
				Session_policy policy(in_label);

				for (Xml_node service_node = policy.sub_node("service");;
					 service_node = service_node.next("service")) {

					char name_buf[64];
					service_node.attribute("name").value(name_buf, sizeof(name_buf));
					if (strcmp(name_buf,_name,64)) continue;
					
					char source_buf[64];
					service_node.attribute("source").value(source_buf, sizeof(source_buf));
					
					Arg_string::set_arg(args_buf, 64, "label", source_buf);
					Session_args new_args(args_buf);
					return env()->parent()->session(_name, new_args, affinity);
				}
			} catch (Xml_node::Nonexistent_sub_node) {
				PERR("rejecting session request; policy for '%s' has no route for service %s", in_label.string(), _name);
				throw Root::Unavailable();
			} catch (Session_policy::No_policy_defined) {
				PERR("rejecting session request; no matching policy for %s", in_label.string());
				throw Root::Unavailable();
			}
		}

		void upgrade(Session_capability session, Upgrade_args const &args) override
		{
			env()->parent()->upgrade(session, args);
		}

		void close(Session_capability session) override
		{
			env()->parent()->close(session);
		}

		Service_source(char *name) {
			strncpy(_name,name,64);
		}
	};
	struct Main;
}

struct Service_router::Main
{
	Server::Entrypoint &ep;

	List<Service_source> service_sources;

	void handle_config_update(unsigned)
	{
		Genode::config()->reload();

		for (Service_source *src;
		     (src = service_sources.first()); ) {
			service_sources.remove(src);
			destroy(env()->heap(), src);
		}
		
		try {
			for (Xml_node service_node = config()->xml_node().sub_node("service");;
			     service_node = service_node.next("service")) {

				char name_buf[64];
				service_node.attribute("name").value(name_buf, sizeof(name_buf));

				Service_source *src = new (env()->heap()) Service_source(name_buf);
				Root_capability root_cap=ep.manage(*src);
				env()->parent()->announce(name_buf, root_cap);
				service_sources.insert(src);
			}
		} catch (Xml_node::Nonexistent_sub_node) { }

	}

	Signal_rpc_member<Main> config_update_dispatcher =
		{ ep, *this, &Main::handle_config_update};

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		handle_config_update(0);

		Genode::config()->sigh(config_update_dispatcher);
	}

};

/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "service_router_ep"; }

	size_t stack_size() { return 4*1024*sizeof(addr_t); }

	void construct(Entrypoint &ep) { static Service_router::Main srv(ep); }
}
