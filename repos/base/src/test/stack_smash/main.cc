/*
 * \brief  Simple stack smashing attempt
 * \author Emery Hemingway
 * \date   2018-12-05
 */

#include <base/component.h>
#include <base/log.h>
#include <util/string.h>


/*
 * FIXME
 *
 * There's a strange optimization implemented in GCC for x86_32 bit using
 * __stack_chk_fail_local() which must be a local hidden symbol (and therefore
 * part of a static library linked to the target. For more info see
 * https://github.com/gcc-mirror/gcc/blob/master/libssp/ssp.c#L195 and
 * https://raw.githubusercontent.com/gcc-mirror/gcc/master/gcc/config/i386/i386.c
 * line 45261.
 */
extern "C" {
	__attribute__((noreturn)) void __stack_chk_fail(void);

	extern "C" __attribute__((noreturn)) __attribute__((visibility("hidden")))
	void __stack_chk_fail_local(void)
	{
		__stack_chk_fail();
	}
}


void Component::construct(Genode::Env &)
{
	using namespace Genode;

	char const *msg = "................ wrote into previous frame";

	char buf[16];
	char *p = buf;

	strncpy(p, msg, strlen(msg)+1);
	log((char const *)p);
}
