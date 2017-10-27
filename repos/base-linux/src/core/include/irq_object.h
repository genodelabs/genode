
#ifndef _IRQ_OBJECT_H_
#define _IRQ_OBJECT_H_

#include <base/thread.h>

namespace Genode
{
	class Irq_object;
};

class Genode::Irq_object : public Thread_deprecated<4096>
{

	private:

		Genode::Signal_context_capability _sig_cap;
		Genode::Lock _sync_ack;
		Genode::Lock _sync_bootup;
		unsigned const _irq;
		int _fd;

		bool _associate();

		void entry() override;

	public:

		Irq_object(unsigned irq);
		void sigh(Signal_context_capability cap);
		void ack_irq();

		void start() override;

};

#endif /* ifndef _IRQ_OBJECT_H_ */

