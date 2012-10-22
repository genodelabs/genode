#ifndef __ENV_DDE_KIT_H__
#define __ENV_DDE_KIT_H__

/* bits/io.h */

#include <dde_kit/resources.h>

static inline uint8_t inb(volatile uint8_t *io_addr)
{
	return dde_kit_inb((dde_kit_addr_t) io_addr);
}

static inline uint16_t inw(volatile uint16_t *io_addr)
{
	return dde_kit_inw((dde_kit_addr_t) io_addr);
}

static inline uint32_t inl(volatile uint32_t *io_addr)
{
	return dde_kit_inl((dde_kit_addr_t) io_addr);
}

static inline void outb(uint8_t data, volatile uint8_t *io_addr)
{
	dde_kit_outb((dde_kit_addr_t) io_addr, data);
}

static inline void outw(uint16_t data, volatile uint16_t *io_addr)
{
	dde_kit_outw((dde_kit_addr_t) io_addr, data);
}

static inline void outl(uint32_t data, volatile uint32_t *io_addr)
{
	dde_kit_outl((dde_kit_addr_t) io_addr, data);
}


static inline uint8_t readb(volatile uint8_t *io_addr)
{
	return *io_addr;
}


static inline uint16_t readw(volatile uint16_t *io_addr)
{
	return *io_addr;
}


static inline uint32_t readl(volatile uint32_t *io_addr)
{
	return *io_addr;
}


static inline void writeb(uint8_t data, volatile uint8_t *io_addr)
{
	*io_addr = data;
}


static inline void writew(uint16_t data, volatile uint16_t *io_addr)
{
	*io_addr = data;
}


static inline void writel(uint32_t data, volatile uint32_t *io_addr)
{
	*io_addr = data;
}


static inline void mb(void)
{
	asm volatile ("lock; addl $0, 0(%%esp)" : : : "memory");
}

#endif /* __ENV_DDE_KIT_H__ */
