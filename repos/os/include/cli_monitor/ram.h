/*
 * \brief  RAM management
 * \author Norman Feske
 * \date   2013-10-14
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CLI_MONITOR__RAM_H_
#define _INCLUDE__CLI_MONITOR__RAM_H_

/* Genode includes */
#include <pd_session/client.h>

namespace Cli_monitor { class Ram; }


class Cli_monitor::Ram
{
	private:

		typedef Genode::size_t size_t;

		Genode::Pd_session           &_pd;
		Genode::Pd_session_capability _pd_cap;

		Genode::Lock mutable _lock { };

		Genode::Signal_context_capability _yield_sigh;
		Genode::Signal_context_capability _resource_avail_sigh;

		size_t _preserve;

		void _validate_preservation()
		{
			if (_pd.avail_ram().value < _preserve)
				Genode::Signal_transmitter(_yield_sigh).submit();

			/* verify to answer outstanding resource requests too */
			if (_pd.avail_ram().value > _preserve)
				Genode::Signal_transmitter(_resource_avail_sigh).submit();
		}

	public:

		struct Status
		{
			size_t quota, used, avail, preserve;
			Status(size_t quota, size_t used, size_t avail, size_t preserve)
			: quota(quota), used(used), avail(avail), preserve(preserve) { }
		};

		Ram(Genode::Pd_session               &pd,
		    Genode::Pd_session_capability     pd_cap,
		    size_t                            preserve,
		    Genode::Signal_context_capability yield_sigh,
		    Genode::Signal_context_capability resource_avail_sigh)
		:
			_pd(pd), _pd_cap(pd_cap),
			_yield_sigh(yield_sigh),
			_resource_avail_sigh(resource_avail_sigh),
			_preserve(preserve)
		{ }

		size_t preserve() const
		{
			Genode::Lock::Guard guard(_lock);

			return _preserve;
		}

		void preserve(size_t preserve)
		{
			Genode::Lock::Guard guard(_lock);

			_preserve = preserve;

			_validate_preservation();
		}

		Status status() const
		{
			Genode::Lock::Guard guard(_lock);

			return Status(_pd.ram_quota().value, _pd.used_ram().value,
			              _pd.avail_ram().value, _preserve);
		}

		void validate_preservation()
		{
			Genode::Lock::Guard guard(_lock);

			_validate_preservation();
		}

		/**
		 * Exception type
		 */
		class Transfer_quota_failed { };

		/**
		 * \throw Transfer_quota_failed
		 */
		void withdraw_from(Genode::Ram_session_capability from, size_t amount)
		{
			using namespace Genode;

			Lock::Guard guard(_lock);

			try { Pd_session_client(from).transfer_quota(_pd_cap, Ram_quota{amount}); }
			catch (...) { throw Transfer_quota_failed(); }

			Signal_transmitter(_resource_avail_sigh).submit();
		}

		/**
		 * \throw Transfer_quota_failed
		 */
		void transfer_to(Genode::Ram_session_capability to, size_t amount)
		{
			Genode::Lock::Guard guard(_lock);

			if (_pd.avail_ram().value < (_preserve + amount)) {
				Genode::Signal_transmitter(_yield_sigh).submit();
				throw Transfer_quota_failed();
			}

			try { _pd.transfer_quota(to, Genode::Ram_quota{amount}); }
			catch (...) { throw Transfer_quota_failed(); }
		}

		size_t avail() const { return _pd.avail_ram().value; }
};

#endif /* _INCLUDE__CLI_MONITOR__RAM_H_ */
