/*
 * \brief  Resource account handling
 * \author Norman Feske
 * \date   2017-04-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ACCOUNT_H_
#define _CORE__INCLUDE__ACCOUNT_H_

#include <base/registry.h>
#include <base/quota_guard.h>
#include <pd_session/pd_session.h>

/* core includes */
#include <types.h>

namespace Core { template <typename> class Account; }


template <typename UNIT>
class Core::Account
{
	public:

		using Guard = Quota_guard<UNIT>;

	private:

		/*
		 * Noncopyable
		 */
		Account(Account const &);
		Account &operator = (Account const &);

		Guard &_quota_guard;

		Session::Label const &_label;

		UNIT const _initial_used  = _quota_guard.used();

		/*
		 * The initial limit corresponds to the static session quota donated
		 * by the client at the session-creation time. During the lifetime
		 * of the session, the account's limit must never become lower than
		 * the initial limit (e.g., via transfer_quota) to allow the initial
		 * limit to be transferred back to the client at session-destruction
		 * time.
		 */
		UNIT const _initial_limit;

		/**
		 * Return maximum amount of transferrable quota
		 */
		UNIT const _transferrable_quota() const
		{
			return UNIT { _quota_guard.limit().value - _initial_limit.value };
		}

		Mutex mutable _mutex { };

		/*
		 * Reference account
		 */
		Account *_ref_account = nullptr;

		/*
		 * Registry of accounts that have this account as their reference
		 * account.
		 */
		Registry<Account> _ref_account_members { };

		/*
		 * Role as reference-account user
		 */
		Constructible<typename Registry<Account>::Element> _ref_account_member { };

		/**
		 * Assign 'this' as reference account of 'account'
		 */
		void _adopt(Account &account)
		{
			account._ref_account_member.construct(_ref_account_members, account);
			account._ref_account = this;
		}

	public:

		/**
		 * Constructor for creating a regular account that is rechargeable by
		 * the specified reference account
		 */
		Account(Guard &quota_guard, Session_label const &label, Account &ref_account)
		:
			_quota_guard(quota_guard), _label(label),
			_initial_limit(_quota_guard.limit())
		{
			ref_account._adopt(*this);
		}

		/**
		 * Constructor used for creating the initial account
		 */
		Account(Guard &quota_guard, Session_label const &label)
		: _quota_guard(quota_guard), _label(label), _initial_limit(UNIT{0}) { }

		~Account()
		{
			if (!_ref_account) return;

			Mutex::Guard guard(_mutex);

			if (_quota_guard.used().value > _initial_used.value) {
				UNIT const dangling { _quota_guard.used().value - _initial_used.value };
				_quota_guard.replenish(dangling);
			}

			/* transfer remaining quota to our reference account */
			UNIT const downgrade = _transferrable_quota();
			_ref_account->_quota_guard.upgrade(downgrade);
			if (!_quota_guard.try_downgrade(downgrade))
				error(_label, ": final quota downgrade unexpectedly failed");

			/* assign all sub accounts to our reference account */
			_ref_account_members.for_each([&] (Account &orphan) {
				_ref_account->_adopt(orphan); });
		}

		using Transfer_result = Pd_account::Transfer_result;

		/**
		 * Transfer quota to/from other account
		 */
		[[nodiscard]] Transfer_result transfer_quota(Account &other, UNIT amount)
		{
			{
				Mutex::Guard guard(_mutex);

				/* transfers are permitted only from/to the reference account */
				if (_ref_account != &other && other._ref_account != this)
					return Transfer_result::INVALID;

				/* make sure to stay within the initial limit */
				if (amount.value > _transferrable_quota().value)
					return Transfer_result::EXCEEDED;

				/* downgrade from this account */
				if (!_quota_guard.try_downgrade(amount))
					return Transfer_result::EXCEEDED;
			}

			/* credit to 'other' */
			Mutex::Guard guard(other._mutex);
			other._quota_guard.upgrade(amount);
			return Transfer_result::OK;
		}

		UNIT limit() const
		{
			Mutex::Guard guard(_mutex);
			return _quota_guard.limit();
		}

		UNIT used() const
		{
			Mutex::Guard guard(_mutex);
			return _quota_guard.used();
		}

		UNIT avail() const
		{
			Mutex::Guard guard(_mutex);
			return _quota_guard.avail();
		}

		/**
		 * Withdraw quota from account
		 *
		 * \return true if withdrawal of 'amount' succeeded
		 */
		[[nodiscard]] bool try_withdraw(UNIT amount)
		{
			Mutex::Guard guard(_mutex);
			return _quota_guard.try_withdraw(amount);
		}

		/**
		 * Replenish quota to account
		 *
		 * Called when releasing physical resources
		 */
		void replenish(UNIT amount)
		{
			Mutex::Guard guard(_mutex);
			_quota_guard.replenish(amount);
		}

		void print(Output &out) const { Genode::print(out, _quota_guard); }

		Session::Label label() const { return _label; }
};

#endif /* _CORE__INCLUDE__ACCOUNT_H_ */
