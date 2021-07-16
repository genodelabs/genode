
/*******************
 ** asm/barrier.h **
 *******************/

#define mb()  asm volatile ("mfence": : :"memory")
#define rmb() asm volatile ("lfence": : :"memory")
#define wmb() asm volatile ("sfence": : :"memory")

#define dma_wmb() barrier()
#define dma_rmb() barrier()

/*
 * This is the "safe" implementation as needed for a configuration
 * with SMP enabled.
 */

#define smp_mb()  mb()
#define smp_rmb() barrier()
#define smp_wmb() barrier()

static inline void barrier() { asm volatile ("": : :"memory"); }
