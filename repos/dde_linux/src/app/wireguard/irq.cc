
#include <lx_emul/irq.h>
#include <lx_kit/env.h>


extern "C" void lx_emul_irq_unmask(unsigned int ) { }


extern "C" void lx_emul_irq_mask(unsigned int ) { }


extern "C" void lx_emul_irq_eoi(unsigned int ) { }


extern "C" unsigned int lx_emul_irq_last()
{
	return Lx_kit::env().last_irq;
}
