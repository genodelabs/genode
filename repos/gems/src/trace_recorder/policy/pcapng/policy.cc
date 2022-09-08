#include <trace/policy.h>
#include <trace_recorder_policy/pcapng.h>

using namespace Genode;

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

size_t checkpoint(char *dst, char const *, unsigned long, void *, unsigned char)
{
	return 0;
}

size_t log_output(char *dst, char const *log_message, size_t len)
{
	return 0;
}

size_t rpc_call(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	return 0;
}

size_t rpc_returned(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	return 0;
}

size_t rpc_dispatch(char *dst, char const *rpc_name)
{
	return 0;
}

size_t rpc_reply(char *dst, char const *rpc_name)
{
	return 0;
}

size_t signal_submit(char *dst, unsigned const)
{
	return 0;
}

size_t signal_receive(char *dst, Signal_context const &, unsigned)
{
	return 0;
}
