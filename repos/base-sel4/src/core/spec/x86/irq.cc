/*
 * \brief  Implementation of the platform specific IRQ association
 * \author Alexander Boettcher
 * \date   2017-07-07
 */

/* core includes */
#include <irq_object.h>

#include <sel4/sel4.h>

using namespace Core;


long Irq_object::_associate(Irq_args const & args)
{
	enum { IRQ_EDGE = 0, IRQ_LEVEL = 1 };
	enum { IRQ_HIGH = 0, IRQ_LOW = 1 };

	seL4_Word level    = (_irq < 16) ? IRQ_EDGE : IRQ_LEVEL;
	seL4_Word polarity = (_irq < 16) ? IRQ_HIGH : IRQ_LOW;

	if (args.trigger() != Irq_session::TRIGGER_UNCHANGED)
		level = (args.trigger() == Irq_session::TRIGGER_LEVEL) ? IRQ_LEVEL : IRQ_EDGE;

	if (args.polarity() != Irq_session::POLARITY_UNCHANGED)
		polarity = (args.polarity() == Irq_session::POLARITY_HIGH) ? IRQ_HIGH : IRQ_LOW;

	seL4_CNode const root   = seL4_CapInitThreadCNode;
	seL4_Word  const index  = _kernel_irq_sel.value();
	seL4_Uint8 const depth  = 32;
	seL4_Word  const ioapic = 0;
	seL4_Word  const pin    = _irq ? _irq : 2;
	seL4_Word  const vector = _irq;
	seL4_Word  const handle = 0;

	switch (args.type()) {
	case Irq_session::TYPE_LEGACY:
		return seL4_IRQControl_GetIOAPIC(seL4_CapIRQControl, root, index, depth,
		                                 ioapic, pin, level, polarity, vector);
	case Irq_session::TYPE_MSI:
	case Irq_session::TYPE_MSIX:
		return seL4_IRQControl_GetMSI(seL4_CapIRQControl, root, index, depth,
		                              args.pci_bus(), args.pci_dev(),
		                              args.pci_func(), handle, vector);
	default:
		return seL4_InvalidArgument;
	}
}
