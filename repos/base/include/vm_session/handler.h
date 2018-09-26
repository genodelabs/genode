/*
 * \brief  Client-side VM session exception handler
 * \author Alexander Boettcher
 * \date   2018-09-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VM_SESSION__HANDLER_H_
#define _INCLUDE__VM_SESSION__HANDLER_H_

#include <base/signal.h>

namespace Genode {
	class Vm_state;
	class Vm_handler_base;
	template <typename, typename> class Vm_handler;
}

class Genode::Vm_handler_base : public Signal_dispatcher_base
{
	friend class Vm_session_client;

	protected:

		Rpc_entrypoint            &_rpc_ep;
		Signal_context_capability  _cap {};
		Genode::Semaphore          _done { 0 };

	public:

		virtual bool config_vm_event(Genode::Vm_state &, unsigned) = 0;

		Vm_handler_base(Rpc_entrypoint &rpc)
		: _rpc_ep(rpc) { }
};

template <typename T, typename EP = Genode::Entrypoint>
class Genode::Vm_handler : public Vm_handler_base 
{
	private:

		EP &_ep;
		T  &_obj;
		void (T::*_member) ();
		void (T::*_config) (Genode::Vm_state &, unsigned const);

		/*
		 * Noncopyable
		 */
		Vm_handler(Vm_handler const &);
		Vm_handler &operator = (Vm_handler const &);

	public:

		/**
		 * Constructor
		 *
		 * \param obj,member  object and method to call when
		 *                    the vm exception occurs
		 */
		Vm_handler(EP &ep, T &obj, void (T::*member)(),
		           void (T::*config)(Genode::Vm_state&, unsigned) = nullptr)
		:
			Vm_handler_base(ep.rpc_ep()),
			_ep(ep),
			_obj(obj),
			_member(member),
			_config(config)
		{
			_cap = ep.manage(*this);
		}

		~Vm_handler() { _ep.dissolve(*this); }

		/**
		 * Interface of Signal_dispatcher_base
		 */
		void dispatch(unsigned) override
		{
			(_obj.*_member)();
			_done.up();
		}

		bool config_vm_event(Genode::Vm_state &state,
		                     unsigned const vm_event) override
		{
			if (!_config)
				return false;

			(_obj.*_config)(state, vm_event);
			return true;
		}

		operator Capability<Signal_context>() const { return _cap; }
};

#endif /* _INCLUDE__VM_SESSION__HANDLER_H_ */
