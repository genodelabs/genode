/*
 * \brief  Utility to track RAM and capability quotas
 * \author Norman Feske
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__QUOTA_GUARD_H_
#define _INCLUDE__BASE__QUOTA_GUARD_H_

#include <util/string.h>
#include <base/log.h>
#include <base/exception.h>

namespace Genode {

	struct Ram_quota
	{
		size_t value;

		static char const *name() { return "bytes"; }

		void print(Output &out) const { Genode::print(out, Number_of_bytes(value)); }
	};

	struct Cap_quota
	{
		size_t value;

		static char const *name() { return "caps"; }

		void print(Output &out) const { Genode::print(out, value); }
	};

	class Quota_guard_untyped;

	template <typename> class Quota_guard;

	struct Reservation : Interface
	{
		virtual void cancel() = 0;
	};
}


class Genode::Quota_guard_untyped
{
	private:

		size_t _limit { 0 };
		size_t _used  { 0 };

	public:

		size_t avail() const { return _limit - _used; }
		size_t used()  const { return _used; }
		size_t limit() const { return _limit; }

		/**
		 * Increase quota limit by specified amount
		 *
		 * If the new limit exceeds the integer range of size_t, the upgrade
		 * clamps the limit to the maximum integer value.
		 */
		void upgrade(size_t const amount)
		{
			size_t const new_limit = _limit + amount;

			/* check for integer overflow */
			bool const overflow = (new_limit < _limit);

			if (overflow)
				error("integer overflow during quota upgrade");

			/* clamp limit to upper bound */
			_limit = overflow ? ~0UL : new_limit;
		}

		/**
		 * Try to decrease quota limit by specified amount
		 *
		 * \return true if quota limit could be reduced, or
		 *         false if the requested amount exceeds the available quota
		 */
		[[nodiscard]] bool try_downgrade(size_t const amount)
		{
			if (avail() < amount)
				return false;

			_limit -= amount;
			return true;
		}

		/**
		 * Deduce specified amount from available quota
		 *
		 * \return true on success, or
		 *         false if the amount exceeds the available quota
		 */
		[[nodiscard]] bool try_withdraw(size_t const amount)
		{
			if (amount > avail())
				return false;

			/*
			 * We don't need to check for a possible overflow of (used +
			 * amount) as this condition is impliitly covered by the limit
			 * check above.
			 */
			_used += amount;
			return true;
		}

		/**
		 * Hand back specified amount to available quota
		 */
		void replenish(size_t const amount)
		{
			bool const underflow = (amount > _used);

			/* clamp lower bound of used value to zero */
			_used = underflow ? 0 : _used - amount;
		}

		/**
		 * Guard for rolling back a quota reservation
		 */
		class Reservation_guard : public Reservation, Noncopyable
		{
			private:

				Quota_guard_untyped &_quota_guard;

				size_t const _amount;

				bool _canceled = false;

			public:

				Reservation_guard(Quota_guard_untyped &quota_guard, size_t const amount)
				:
					_quota_guard(quota_guard), _amount(amount)
				{ }

				~Reservation_guard()
				{
					if (_canceled)
						_quota_guard.replenish(_amount);
				}

				/**
				 * Reservation interface
				 */
				void cancel() override { _canceled = true; }
		};
};


template <typename UNIT>
class Genode::Quota_guard
{
	private:

		Quota_guard_untyped _guard { };

	public:

		struct Limit_exceeded : Exception { };

		Quota_guard() { }
		Quota_guard(UNIT amount) { upgrade(amount); }

		UNIT avail() const { return UNIT { _guard.avail() }; }
		UNIT limit() const { return UNIT { _guard.limit() }; }
		UNIT used()  const { return UNIT { _guard.used()  }; }

		/**
		 * Increase quota limit by specified amount
		 */
		void upgrade(UNIT amount) { _guard.upgrade(amount.value); }

		/**
		 * Try to withdraw quota by specified amount
		 */
		bool try_withdraw(UNIT amount)
		{
			return _guard.try_withdraw(amount.value);
		}

		/**
		 * Try to decrease quota limit by specified amount
		 */
		bool try_downgrade(UNIT amount)
		{
			return _guard.try_downgrade(amount.value);
		}

		/**
		 * Consume specified amount of quota
		 *
		 * \throw Limit_exceeded  amount exceeds available quota
		 */
		void withdraw(UNIT amount)
		{
			if (!_guard.try_withdraw(amount.value))
				throw Limit_exceeded();
		}

		/**
		 * Hand back specified amount to available quota
		 */
		void replenish(UNIT amount) { _guard.replenish(amount.value); }

		void print(Output &out) const
		{
			Genode::print(out, "used=",  UNIT{_guard.used()}, ", "
			                   "limit=", UNIT{_guard.limit()});
		}

		/**
		 * Utility used for transactional multi-step resource allocations
		 *
		 * \deprecated  Use 'with_reservation' instead
		 *
		 * Note that this class is not related to the 'Genode::Reservation'
		 * interface.
		 */
		struct Reservation
		{
			Quota_guard &_quota_guard;
			UNIT   const _amount;
			bool         _ack = false;

			/**
			 * Constructor
			 *
			 * \throw Limit_exceeded
			 */
			Reservation(Quota_guard &quota_guard, UNIT amount)
			:
				_quota_guard(quota_guard), _amount(amount)
			{
				_quota_guard.withdraw(_amount);
			}

			~Reservation()
			{
				if (!_ack)
					_quota_guard.replenish(_amount);
			}

			void acknowledge() { _ack = true; }
		};

		template <typename RET>
		RET with_reservation(UNIT const amount,
		                     auto const &fn,
		                     auto const &error_fn)
		{
			if (!_guard.try_withdraw(amount.value))
				return error_fn();

			/*
			 * The withdrawal was successful. Use reservation guard to
			 * rollback the withdrawal depending on 'fn'.
			 */

			Quota_guard_untyped::Reservation_guard
				reservation_guard { _guard, amount.value };

			/* expose only the 'Reservation' interface to 'fn' */
			::Genode::Reservation &interface = reservation_guard;

			return fn(interface);
		}

		bool have_avail(UNIT const amount) const
		{
			return _guard.avail() >= amount.value;
		}
};


namespace Genode {

	using Ram_quota_guard = Quota_guard<Ram_quota>;
	using Cap_quota_guard = Quota_guard<Cap_quota>;

	using Out_of_ram = Ram_quota_guard::Limit_exceeded;
	using Out_of_caps = Cap_quota_guard::Limit_exceeded;
}

#endif /* _INCLUDE__BASE__QUOTA_GUARD_H_ */
