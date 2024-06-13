/*
 * \brief  Utility for implementing transactional quota transfers
 * \author Norman Feske
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__QUOTA_TRANSFER_H_
#define _INCLUDE__BASE__QUOTA_TRANSFER_H_

#include <base/capability.h>
#include <pd_session/pd_session.h>

namespace Genode {

	template <typename SESSION, typename UNIT, typename RESULT> class Quota_transfer;

	using Ram_transfer = Quota_transfer<Pd_session, Ram_quota, Pd_session::Transfer_ram_quota_result>;
	using Cap_transfer = Quota_transfer<Pd_session, Cap_quota, Pd_session::Transfer_cap_quota_result>;
}


/**
 * Guard for transferring quota donation
 *
 * This class template is used to transfer quotas in a transactional way.
 * Establishing a new session involves several steps, in particular subsequent
 * quota transfers. If one intermediate step fails, we need to revert all quota
 * transfers that already took place. When instantiated at a local scope, a
 * transfer object guards a quota transfer. If the scope is left without an
 * explicit prior acknowledgement of the transfer (for example via an
 * exception), the destructor the transfer object reverts the transfer in
 * flight.
 */
template <typename SESSION, typename UNIT, typename RESULT>
class Genode::Quota_transfer
{
	public:

		struct Account : Noncopyable, Interface
		{
			using Transfer_result = RESULT;

			/**
			 * Return capability used for transfers to the account
			 *
			 * The 'UNIT' argument is solely used as an overload selector
			 * to disambiguate the 'cap' methods of multiple inherited
			 * 'Account' types (as done by 'Service').
			 */
			virtual Capability<SESSION> cap(UNIT) const
			{
				return Capability<SESSION>();
			}

			/**
			 * Transfer quota to the specified account
			 */
			virtual Transfer_result transfer(Capability<SESSION>, UNIT)
			{
				return Transfer_result::OK;
			}

			/**
			 * Try to transfer quota, ignoring possible exceptions
			 *
			 * This method is solely meant to be used in destructors.
			 */
			void try_transfer(Capability<SESSION> to, UNIT amount)
			{
				transfer(to, amount);
			}
		};

		/**
		 * Account implementation that issues quota transfers via RPC
		 */
		struct Remote_account : Account
		{
			Capability<SESSION> _cap;
			SESSION            &_session;

			Remote_account(SESSION &session, Capability<SESSION> cap)
			: _cap(cap), _session(session) { }

			Capability<SESSION> cap(UNIT) const override { return _cap; }

			using Transfer_result = RESULT;

			Transfer_result transfer(Capability<SESSION> to, UNIT amount) override
			{
				return to.valid() ? _session.transfer_quota(to, amount)
				                  : Transfer_result::OK;
			}
		};

	private:

		bool       _ack;
		UNIT const _amount;
		Account   &_from;
		Account   &_to;

		static bool _exceeded(Ram_quota, RESULT r) { return (r == RESULT::OUT_OF_RAM);  }
		static bool _exceeded(Cap_quota, RESULT r) { return (r == RESULT::OUT_OF_CAPS); }

	public:

		class Quota_exceeded : Exception { };

		/**
		 * Constructor
		 *
		 * \param amount  amount of quota to transfer
		 * \param from    donor account
		 * \param to      receiving account
		 * \throw         Quota_exceeded
		 */
		Quota_transfer(UNIT amount, Account &from, Account &to)
		:
			_ack(false), _amount(amount), _from(from), _to(to)
		{
			if (!_from.cap(UNIT()).valid() || !_to.cap(UNIT()).valid())
				return;

			if (_exceeded(UNIT{}, _from.transfer(_to.cap(UNIT()), amount)))
				throw Quota_exceeded();
		}

		/**
		 * Destructor
		 *
		 * The destructor will be called when leaving the scope of the
		 * 'session' function. If the scope is left because of an error
		 * (e.g., an exception), the donation will be reverted.
		 */
		~Quota_transfer()
		{
			if (_ack || !_from.cap(UNIT()).valid() || !_to.cap(UNIT()).valid())
				return;

			_to.try_transfer(_from.cap(UNIT()), _amount);
		}

		/**
		 * Acknowledge quota donation
		 */
		void acknowledge() { _ack = true; }
};

#endif /* _INCLUDE__BASE__QUOTA_TRANSFER_H_ */
