/*
 * \brief  Implementation of the platform specific IRQ association
 * \author Alexander Boettcher
 * \date   2017-07-07
 */

#include <irq_object.h>

#include <sel4/sel4.h>

using namespace Core;


long Irq_object::_associate(Irq_args const &)
{
	seL4_CNode const root  = seL4_CapInitThreadCNode;
	seL4_Word  const index = _kernel_irq_sel.value();
	seL4_Uint8 const depth = 32;

	return seL4_IRQControl_Get(seL4_CapIRQControl, _irq, root, index, depth);
}
