#include <trace/policy.h>

using namespace Genode;

size_t max_event_size()
{
	return 0;
}

size_t rpc_call(char *dst, char const *rpc_name, Msgbuf_base const &, unsigned long long)
{
	return 0;
}

size_t rpc_returned(char *dst, char const *rpc_name, Msgbuf_base const &, unsigned long long)
{
	return 0;
}

size_t rpc_dispatch(char *dst, char const *rpc_name, unsigned long long)
{
	return 0;
}

size_t rpc_reply(char *dst, char const *rpc_name, unsigned long long)
{
	return 0;
}

size_t signal_submit(char *dst, unsigned const, unsigned long long)
{
	return 0;
}

size_t signal_receive(char *dst, Signal_context const &, unsigned, unsigned long long)
{
	return 0;
}

