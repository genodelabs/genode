/*
 * \brief  SUPLib vCPU utility
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_VCPU_H_
#define _SUP_VCPU_H_

/* Genode includes */
#include <base/env.h>
#include <base/env.h>
#include <vm_session/connection.h>

/* local includes */
#include <sup.h>

namespace Sup { struct Vcpu; }

namespace Pthread { struct Emt; }


struct Sup::Vcpu : Genode::Interface
{
	/* enter VM to run vCPU (called by EMT) */
	virtual VBOXSTRICTRC run() = 0;

	/* request vCPU to exit VM with pause */
	virtual void pause() = 0;

	/* halt until woken up or timeout expiration (called by EMT) */
	virtual void halt(Genode::uint64_t const wait_ns) = 0;

	/* wake up halted EMT */
	virtual void wake_up() = 0;

	/* create VMX vCPU */
	static Vcpu & create_vmx(Genode::Env &, VM &, Vm_connection &, Cpu_index, Pthread::Emt &);

	/* create SVM vCPU */
	static Vcpu & create_svm(Genode::Env &, VM &, Vm_connection &, Cpu_index, Pthread::Emt &);
};


#endif /* _SUP_VCPU_H_ */
