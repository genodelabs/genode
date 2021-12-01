#include <util/string.h>
#include <trace/policy.h>

using namespace Genode;

enum { MAX_EVENT_SIZE = 64 };

static inline int div_zero()
{
	static int zero;

	/* use variable to prevent compiler warning about division by zero */
	return 1 / zero;
}

size_t max_event_size()
{
	return MAX_EVENT_SIZE;
}


size_t checkpoint(char *dst, char const *, unsigned long, void *, unsigned char)
{
	return div_zero();
}

size_t log_output(char *dst, char const *log_message, size_t len)
{
	return div_zero();
}

size_t rpc_call(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	return div_zero();
}

size_t rpc_returned(char *dst, char const *rpc_name, Msgbuf_base const &)
{
	return div_zero();
}

size_t rpc_dispatch(char *dst, char const *rpc_name)
{
	return div_zero();
}

size_t rpc_reply(char *dst, char const *rpc_name)
{
	return div_zero();
}

size_t signal_submit(char *dst, unsigned const)
{
	return div_zero();
}

size_t signal_receive(char *dst, Signal_context const &, unsigned)
{
	return div_zero();
}
