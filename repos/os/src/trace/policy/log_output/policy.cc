#include <util/string.h>
#include <trace/policy.h>

using namespace Genode;

enum { MAX_EVENT_SIZE = 256 };

size_t max_event_size()
{
	return MAX_EVENT_SIZE;
}

size_t trace_eth_packet(char *, char const *, bool, char *, size_t)
{
	return 0;
}

size_t checkpoint(char *dst, char const *, unsigned long, void *, unsigned char)
{
	return 0;
}

size_t log_output(char *dst, char const *log_message, size_t len)
{
	len = min(len, MAX_EVENT_SIZE);
	memcpy(dst, (void*)log_message, len);
	return len;
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

