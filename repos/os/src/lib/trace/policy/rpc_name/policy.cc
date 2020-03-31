#include <util/string.h>
#include <trace/policy.h>

using namespace Genode;

enum { MAX_EVENT_SIZE = 64 };

size_t max_event_size()
{
	return MAX_EVENT_SIZE;
}

size_t log_output(char *dst, char const *log_message, size_t len)
{
	return 0;
}

size_t rpc_call(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	size_t len = strlen(rpc_name);

	memcpy(dst, (void*)rpc_name, len);
	return len;
}

size_t rpc_returned(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	size_t len = strlen(rpc_name);

	memcpy(dst, (void*)rpc_name, len);
	return len;
}

size_t rpc_dispatch(char *dst, char const *rpc_name)
{
	size_t len = strlen(rpc_name);

	memcpy(dst, (void*)rpc_name, len);
	return len;
}

size_t rpc_reply(char *dst, char const *rpc_name)
{
	size_t len = strlen(rpc_name);

	memcpy(dst, (void*)rpc_name, len);
	return len;
}

size_t signal_submit(char *dst, unsigned const)
{
	return 0;
}

size_t signal_receive(char *dst, Signal_context const &, unsigned)
{
	return 0;
}
