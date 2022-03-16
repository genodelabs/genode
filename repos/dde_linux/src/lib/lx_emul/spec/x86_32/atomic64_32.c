#include <asm/atomic64_32.h>

/**
 * This is not atomic on 32bit systems but this is not a problem
 * because we will not be preempted.
 */

s64 arch_atomic64_add(s64 i, atomic64_t *v)
{
	v->counter += i;
	return v->counter;
}


s64 arch_atomic64_read(const atomic64_t *v)
{
	return v->counter;
}


s64 arch_atomic64_sub(s64 i, atomic64_t *v)
{
	v->counter -= i;
	return v->counter;
}


s64 arch_atomic64_fetch_add(s64 i, atomic64_t *v)
{
	v->counter += i;
	return v->counter;
}
