/*
 * \brief  Client-side VM session vCPU exception handler
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2018-09-29
 */

/*
 * Copyright (C) 2018-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VM_SESSION__HANDLER_H_
#define _INCLUDE__VM_SESSION__HANDLER_H_

#include <base/signal.h>

namespace Genode {
	class Vcpu_state;
	class Vcpu_handler_base;
	template <typename, typename> class Vcpu_handler;
}

class Genode::Vcpu_handler_base : public Signal_dispatcher_base
{
	protected:

		Entrypoint                &_ep;
		Signal_context_capability  _signal_cap { };
		Genode::Semaphore          _ready_semaphore { 0 };

	public:

		Vcpu_handler_base(Entrypoint &ep)
		: _ep(ep) { }

		Rpc_entrypoint &          rpc_ep()          { return _ep.rpc_ep(); }
		Entrypoint &              ep()              { return _ep; }
		Signal_context_capability signal_cap()      { return _signal_cap; }
		Genode::Semaphore &       ready_semaphore() { return _ready_semaphore; }
};

template <typename T, typename EP = Genode::Entrypoint>
class Genode::Vcpu_handler : public Vcpu_handler_base
{
	private:

		EP &_ep;
		T  &_obj;
		void (T::*_member) ();

		/*
		 * Noncopyable
		 */
		Vcpu_handler(Vcpu_handler const &);
		Vcpu_handler &operator = (Vcpu_handler const &);

	public:

		/**
		 * Constructor
		 *
		 * \param obj,member  object and method to call when
		 *                    the vm exception occurs
		 */
		Vcpu_handler(EP &ep, T &obj, void (T::*member)())
		:
			Vcpu_handler_base(ep),
			_ep(ep),
			_obj(obj),
			_member(member)
		{
			_signal_cap = _ep.manage(*this);
		}

		~Vcpu_handler() { _ep.dissolve(*this); }

		/**
		 * Interface of Signal_dispatcher_base
		 */
		void dispatch(unsigned) override
		{
			(_obj.*_member)();
			_ready_semaphore.up();
		}

		operator Capability<Signal_context>() const { return _signal_cap; }
};

#endif /* _INCLUDE__VM_SESSION__HANDLER_H_ */
