#include <util/string.h>
#include <trace/policy.h>

using namespace Genode;

enum { MAX_EVENT_SIZE = 64 };

size_t max_event_size()
{
	return MAX_EVENT_SIZE;
}

size_t rpc_call(char *dst, char const *rpc_name, Msgbuf_base const &, unsigned long long execution_time)
{
	Genode::String<MAX_EVENT_SIZE> buffer(execution_time, ": ", Genode::CString(rpc_name));
	memcpy(dst, (void*) buffer.string(), buffer.length());
	return buffer.length();
}

size_t rpc_returned(char *dst, char const *rpc_name, Msgbuf_base const &, unsigned long long execution_time)
{
	Genode::String<MAX_EVENT_SIZE> buffer(execution_time, ": ", Genode::CString(rpc_name));
	memcpy(dst, (void*) buffer.string(), buffer.length());
	return buffer.length();
}

size_t rpc_dispatch(char *dst, char const *rpc_name, unsigned long long execution_time)
{
	Genode::String<MAX_EVENT_SIZE> buffer(execution_time, ": ", Genode::CString(rpc_name));
	memcpy(dst, (void*) buffer.string(), buffer.length());
	return buffer.length();
}

size_t rpc_reply(char *dst, char const *rpc_name, unsigned long long execution_time)
{
	Genode::String<MAX_EVENT_SIZE> buffer(execution_time, ": ", Genode::CString(rpc_name));
	memcpy(dst, (void*) buffer.string(), buffer.length());
	return buffer.length();
}

size_t signal_submit(char *dst, unsigned const num, unsigned long long execution_time)
{
	Genode::String<MAX_EVENT_SIZE> buffer(execution_time);
	memcpy(dst, (void*) buffer.string(), buffer.length());
	return buffer.length();
}

size_t signal_receive(char *dst, Signal_context const &, unsigned num, unsigned long long execution_time)
{
	Genode::String<MAX_EVENT_SIZE> buffer(execution_time);
	memcpy(dst, (void*) buffer.string(), buffer.length());
	return buffer.length();
}
