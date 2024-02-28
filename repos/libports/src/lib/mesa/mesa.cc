/* Genode */
#include <base/log.h>
#include <format/snprintf.h>
#include <util/string.h>

/* libc */
#include <pthread.h>

/* Mesa */
#include <util/log.h>

extern "C" void pthread_set_name_np(pthread_t, const char *)
{ }


/* mesa/src/util/log.c */
extern "C" void
mesa_log(enum mesa_log_level level, const char *tag, const char *format, ...)
{
	using namespace Genode;

	va_list list;
	va_start(list, format);

	char buf[128] { };
	Format::String_console(buf, sizeof(buf)).vprintf(format, list);

	switch (level) {
	case MESA_LOG_ERROR: log("Mesa error: ",   Cstring(buf)); break;
	case MESA_LOG_WARN : log("Mesa warning: ", Cstring(buf)); break;
	case MESA_LOG_INFO : log("Mesa info: ",    Cstring(buf)); break;
	case MESA_LOG_DEBUG: log("Mesa debug: ",   Cstring(buf)); break;
	}

	va_end(list);
}
