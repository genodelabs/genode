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

#include <util/reconstructible.h>
#include <base/registry.h>
#include <base/quota_guard.h>

namespace Genode { template <typename> class Account; }


template <typename UNIT>
class Genode::Account
{
	private:

		Quota_guard<UNIT> &_quota_guard;

		Session::Label const &_label;

		UNIT const _initial_used = _quota_guard.used();

		Lock mutable _lock;

		/*
		 * Reference account
		 */
		Account *_ref_account = nullptr;

		/*
		 * Registry of accounts that have this account as their reference
		 * account.
		 */
		Registry<Account> _ref_account_members;

		/*
		 * Role as reference-account user
		 */
		Constructible<typename Registry<Account>::Element> _ref_account_member;

		/**
		 * Assign 'this' as reference account of 'account'
		 */
		void _adopt(Account &account)
		{
			account._ref_account_member.construct(_ref_account_members, account);
			account._ref_account = this;
		}

	public:

		typedef typename Quota_guard<UNIT>::Limit_exceeded Limit_exceeded;

		class Unrelated_account : Exception { };

		/**
		 * Constructor for creating a regular account that is rechargeable by
		 * the specified reference account
		 */
		Account(Quota_guard<UNIT> &quota_guard, Session_label const &label,
		        Account &ref_account)
		: _quota_guard(quota_guard), _label(label) { ref_account._adopt(*this); }

		/**
		 * Constructor used for creating the initial account
		 */
		Account(Quota_guard<UNIT> &quota_guard, Session_label const &label)
		: _quota_guard(quota_guard), _label(label) { }

		~Account()
		{
			if (!_ref_account) return;

			Lock::Guard guard(_lock);

			if (_quota_guard.used().value > _initial_used.value) {
				UNIT const dangling { _quota_guard.used().value - _initial_used.value };
				_quota_guard.replenish(dangling);
			}

			/* transfer remaining quota to our reference account */
			_ref_account->_quota_guard.upgrade(_quota_guard.limit());

			/* assign all sub accounts to our reference account */
			_ref_account_members.for_each([&] (Account &orphan) {
				_ref_account->_adopt(orphan); });
		}

		/**
		 * Transfer quota to/from other account
		 *
		 * \throw Unrelated_account
		 * \throw Limit_exceeded
		 */
		void transfer_quota(Account &other, UNIT amount)
		{
			{
				Lock::Guard guard(_lock);

				/* transfers are permitted only from/to the reference account */
				if (_ref_account != &other && other._ref_account != this)
					throw Unrelated_account();

				/* downgrade from this account */
				if (!_quota_guard.try_downgrade(amount))
					throw Limit_exceeded();
			}

			/* credit to 'other' */
			Lock::Guard guard(other._lock);
			other._quota_guard.upgrade(amount);
		}

		UNIT limit() const
		{
			Lock::Guard guard(_lock);
			return _quota_guard.limit();
		}

		UNIT used() const
		{
			Lock::Guard guard(_lock);
			return _quota_guard.used();
		}

		UNIT avail() const
		{
			Lock::Guard guard(_lock);
			return _quota_guard.avail();
		}

		/**
		 * Withdraw quota from account
		 *
		 * Called when allocating physical resources
		 *
		 * \throw Limit_exceeded
		 */
		void withdraw(UNIT amount)
		{
			Lock::Guard guard(_lock);
			_quota_guard.withdraw(amount);
		}

		/**
		 * Replenish quota to account
		 *
		 * Called when releasing physical resources
		 */
		void replenish(UNIT amount)
		{
			Lock::Guard guard(_lock);
			_quota_guard.replenish(amount);
		}

		void print(Output &out) const { Genode::print(out, _quota_guard); }

		Session::Label label() const { return _label; }
};

#endif /* _CORE__INCLUDE__ACCOUNT_H_ */
