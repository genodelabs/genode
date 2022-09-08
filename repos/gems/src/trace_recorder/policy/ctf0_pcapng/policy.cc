#include <util/string.h>
#include <trace/policy.h>
#include <trace_recorder_policy/ctf_stream0.h>
#include <trace_recorder_policy/pcapng.h>

using namespace Genode;
using namespace Ctf;

enum { MAX_CAPTURE_LEN = 100 };

size_t max_event_size() {
	return Trace_recorder::Pcapng_event::max_size(MAX_CAPTURE_LEN); }

size_t trace_eth_packet(char *dst, char const *if_name, bool out, char *pkt_data, size_t pkt_len)
{
	using namespace Pcapng;
	Trace_recorder::Pcapng_event *e =
		new (dst) Trace_recorder::Pcapng_event(Link_type::ETHERNET, if_name, out, pkt_len, pkt_data, MAX_CAPTURE_LEN);

	return e->total_length();
}

size_t checkpoint(char *dst, char const *name, unsigned long data, void *addr, unsigned char type)
{
	size_t len = strlen(name) + 1;

	new (dst) Checkpoint(name, len, data, addr, type);

	return len + sizeof(Checkpoint);
}

size_t log_output(char *dst, char const *log_message, size_t len) {
	return 0;
}

size_t rpc_call(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	size_t len = strlen(rpc_name) + 1;

	new (dst) Rpc_call(rpc_name, len);

	return len + sizeof(Rpc_call);
}

size_t rpc_returned(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	size_t len = strlen(rpc_name) + 1;

	new (dst) Rpc_returned(rpc_name, len);

	return len + sizeof(Rpc_returned);
}

size_t rpc_dispatch(char *dst, char const *rpc_name)
{
	size_t len = strlen(rpc_name) + 1;

	new (dst) Rpc_dispatch(rpc_name, len);

	return len + sizeof(Rpc_dispatch);
}

size_t rpc_reply(char *dst, char const *rpc_name)
{
	size_t len = strlen(rpc_name) + 1;

	new (dst) Rpc_reply(rpc_name, len);

	return len + sizeof(Rpc_reply);
}

size_t signal_submit(char *dst, unsigned const num)
{
	new (dst) Signal_submit(num);

	return sizeof(Signal_submit);
}

size_t signal_receive(char *dst, Signal_context const & context, unsigned num)
{
	new (dst) Signal_receive(num, (void*)&context);
	return 0;
}
