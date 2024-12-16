/*
 * \brief  RAM and cap account of the core component
 * \author Norman Feske
 * \date   2024-12-17
 */

/*
 * Copyright (C) 2016-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_ACCOUNT_H_
#define _CORE__INCLUDE__CORE_ACCOUNT_H_

namespace Core { struct Core_account; }


struct Core::Core_account : Rpc_object<Pd_account>
{
	Rpc_entrypoint &_ep;

	Ram_quota_guard ram_quota_guard;
	Cap_quota_guard cap_quota_guard;

	Core::Account<Ram_quota> ram_account { ram_quota_guard, "core" };
	Core::Account<Cap_quota> cap_account { cap_quota_guard, "core" };

	Core_account(Rpc_entrypoint &ep, Ram_quota ram, Cap_quota caps)
	: _ep(ep), ram_quota_guard(ram), cap_quota_guard(caps) { ep.manage(this); }

	Transfer_result _with_pd(Capability<Pd_account> cap, auto const &fn)
	{
		if (this->cap() == cap)
			return Transfer_result::OK;

		return _ep.apply(cap, [&] (Pd_session_component *pd_ptr) {
			Transfer_result result = Transfer_result::INVALID;
			if (pd_ptr)
				fn(*pd_ptr, result);
			return result; });
	}

	Transfer_result transfer_quota(Capability<Pd_account> to, Cap_quota amount) override
	{
		return _with_pd(to, [&] (Pd_session_component &pd, Transfer_result &result) {
			pd.with_cap_account([&] (Account<Cap_quota> &to) {
				result = cap_account.transfer_quota(to, amount); }); });
	}

	Transfer_result transfer_quota(Capability<Pd_account> to, Ram_quota amount) override
	{
		return _with_pd(to, [&] (Pd_session_component &pd, Transfer_result &result) {
			pd.with_ram_account([&] (Account<Ram_quota> &to) {
				result = ram_account.transfer_quota(to, amount); }); });
	}
};

#endif /* _CORE__INCLUDE__CORE_ACCOUNT_H_ */
