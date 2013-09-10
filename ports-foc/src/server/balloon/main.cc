#include <balloon_session/balloon_session.h>
#include <cap_session/connection.h>
#include <timer_session/connection.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <base/sleep.h>
#include <base/env.h>

using namespace Genode;

class Session_component;
static List<Session_component> session_list;


class Session_component : public Rpc_object<Balloon_session, Session_component>,
                          public List<Session_component>::Element
{
	private:

		Signal_context_capability _handler;

	public:

		Session_component() {
			session_list.insert(this); }

		~Session_component() {
			session_list.remove(this); };

		int increase_quota(Ram_session_capability ram_session, size_t amount)
		{
			PDBG("increase ram_quota of client by %zx", amount);
			while (true) ;
			return 0;
		}

		void balloon_handler(Signal_context_capability handler) {
			_handler = handler; }

		Signal_context_capability handler() { return _handler; }
};


class Root : public Genode::Root_component<Session_component>
{
	protected:

		Session_component *_create_session(const char *args)
		{
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			if (ram_quota < sizeof(Session_component))
				throw Root::Quota_exceeded();

			return new (md_alloc()) Session_component();
		}

	public:

		Root(Rpc_entrypoint *session_ep,
			 Allocator      *md_alloc)
		: Root_component(session_ep, md_alloc) { }
};


int main() {

	enum { STACK_SIZE = 1024*sizeof(Genode::addr_t) };

	static Timer::Connection timer;
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "balloon_ep");
	static ::Root         root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&root));

	while (true) {
		Session_component *c = session_list.first();
		while (c) {
			if (c->handler().valid()) {
				PINF("request memory from client!");
				Signal_transmitter transmitter(c->handler());
				transmitter.submit();
			}
			c = c->next();
		}
		timer.msleep(10000);
	}
	return 0;
}
