#include <clocksource/arm_arch_timer.h>

extern unsigned long long lx_emul_timestamp(void);

u64 (*arch_timer_read_counter)(void) = lx_emul_timestamp;

