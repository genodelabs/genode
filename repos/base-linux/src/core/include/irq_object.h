/*
 * \brief  Core-specific instance of the IRQ session interface (Linux)
 * \author Johannes Kliemann
 * \date   2019-11-25
 */

/*
 * Copyright (C) 2007-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _IRQ_OBJECT_H_
#define _IRQ_OBJECT_H_

/* Genode includes */
#include <base/thread.h>

/* core includes */
#include <types.h>

namespace Core { class Irq_object; };


class Core::Irq_object : public Thread
{
	private:

		Signal_context_capability _sig_cap;
		Blockade _sync_ack { };
		Blockade _sync_bootup { };
		unsigned const _irq;
		int _fd;

		bool _associate();

		void entry() override;

	public:

		Irq_object(unsigned irq);
		void sigh(Signal_context_capability cap);
		void ack_irq();

		Start_result start() override;

};

#endif /* ifndef _IRQ_OBJECT_H_ */

