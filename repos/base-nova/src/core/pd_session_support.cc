/*
 * \brief  Extension of core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pd_session_component.h>
#include <assertion.h>

#include <nova_util.h> /* kernel_hip */
#include <nova_msr.h>

using namespace Core;

inline Nova::uint8_t retry_syscall(addr_t pd_sel, auto const &fn)
{
	Nova::uint8_t res;
	do {
		res = fn();
	} while (res == Nova::NOVA_PD_OOM &&
	         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
	                                                   pd_sel,
	                                                   "core", "ep",
	                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

	return res;
}

bool Pd_session_component::assign_pci(addr_t pci_config_memory, uint16_t bdf)
{
	return retry_syscall(_pd->pd_sel(), [&] {
		return Nova::assign_pci(_pd->pd_sel(), pci_config_memory, bdf);
	}) == Nova::NOVA_OK;
}


Pd_session::Map_result Pd_session_component::map(Pd_session::Virt_range const virt_range)
{
	Platform_pd &target_pd = *_pd;
	Nova::Utcb  &utcb      = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
	addr_t const pd_core   = platform_specific().core_pd_sel();
	addr_t const pd_dst    = target_pd.pd_sel();

	auto map_memory = [&] (Mapping const &mapping)
	{
		/* asynchronously map memory */
		uint8_t err = retry_syscall(_pd->pd_sel(), [&] {
			utcb.set_msg_word(0);

			bool res = utcb.append_item(nova_src_crd(mapping), 0, true, false,
			                            false,
			                            mapping.dma_buffer,
			                            mapping.write_combined);

			/* one item ever fits on the UTCB */
			(void)res;

			return Nova::delegate(pd_core, pd_dst, nova_dst_crd(mapping));
		});

		if (err != Nova::NOVA_OK) {
			error("could not eagerly map memory ",
			      Hex_range<addr_t>(mapping.dst_addr, 1UL << mapping.size_log2) , " "
			      "error=", err);
		}
	};

	addr_t virt = virt_range.start;
	size_t size = virt_range.num_bytes;
	try {
		while (size) {

			Fault const artificial_fault {
				.hotspot = { virt },
				.access  = Access::READ,
				.rwx     = Rwx::rwx(),
				.bounds  = { .start = 0, .end  = ~0UL },
			};

			_address_space.with_mapping_for_fault(artificial_fault,
				[&] (Mapping const &mapping)
				{
					map_memory(mapping);

					size_t const mapped_bytes = 1 << mapping.size_log2;

					virt += mapped_bytes;
					size  = size < mapped_bytes ? 0 : size - mapped_bytes;
				},

				[&] (Region_map_component &, Fault const &) { /* don't reflect */ }
			);
		}
	}
	catch (Out_of_ram)  { return Map_result::OUT_OF_RAM;   }
	catch (Out_of_caps) { return Map_result::OUT_OF_CAPS; }
	catch (...) {
		error(__func__, " failed ", Hex(virt), "+", Hex(size));
	}
	return Map_result::OK;
}


using State = Genode::Pd_session::Managing_system_state;


class System_control_component : public Genode::Rpc_object<Pd_session::System_control>
{
	public:

		State system_control(State const &) override;
};


class System_control_impl : public Core::System_control
{
	private:

		System_control_component objects [Core::Platform::MAX_SUPPORTED_CPUS] { };

		auto with_location(auto const &location, auto const &fn)
		{
			unsigned const index = platform_specific().pager_index(location);

			if (index < Core::Platform::MAX_SUPPORTED_CPUS)
				return fn (objects[index]);

			return Capability<Pd_session::System_control> { };
		}

		auto with_location(auto const &location, auto const &fn) const
		{
			unsigned const index = platform_specific().pager_index(location);

			if (index < Core::Platform::MAX_SUPPORTED_CPUS)
				return fn (objects[index]);

			return Capability<Pd_session::System_control> { };
		}

	public:

		Capability<Pd_session::System_control> control_cap(Affinity::Location const) const override;

		void manage(Rpc_entrypoint &ep, Affinity::Location const &location)
		{
			with_location(location, [&](auto &object) {
				ep.manage(&object);
				return object.cap();
			});
		}

};


static System_control_impl &system_instance()
{
	static System_control_impl system_control { };
	return system_control;
}


System_control & Core::init_system_control(Allocator &alloc, Rpc_entrypoint &)
{
	enum { ENTRYPOINT_STACK_SIZE = 20 * 1024 };

	platform_specific().for_each_location([&](Affinity::Location const &location) {

		unsigned const kernel_cpu_id = platform_specific().kernel_cpu_id(location);

		if (!kernel_hip().is_cpu_enabled(kernel_cpu_id))
			return;

		auto ep = new (alloc) Rpc_entrypoint (nullptr, ENTRYPOINT_STACK_SIZE,
		                                      "system_control", location);

		system_instance().manage(*ep, location);
	});

	return system_instance();
};


Capability<Pd_session::System_control> System_control_impl::control_cap(Affinity::Location const location) const
{
	return with_location(location, [&](auto &object) {
		return object.cap();
	});
}


static State acpi_suspend(State const &request)
{
	State respond { .trapno = 0 };

	/*
	 * The trapno/ip/sp registers used below are just convention to transfer
	 * the intended sleep state S0 ... S5. The values are read out by an
	 * ACPI AML component and are of type TYP_SLPx as described in the
	 * ACPI specification, e.g. TYP_SLPa and TYP_SLPb. The values differ
	 * between different PC systems/boards.
	 *
	 * \note trapno/ip/sp registers are chosen because they exist in
	 *       Managing_system_state for x86_32 and x86_64.
	 */
	uint8_t const sleep_type_a = uint8_t(request.ip);
	uint8_t const sleep_type_b = uint8_t(request.sp);

	auto const cap_suspend = platform_specific().core_pd_sel() + 3;
	auto const result      = Nova::acpi_suspend(cap_suspend, sleep_type_a,
	                                            sleep_type_b);

	if (result == Nova::NOVA_OK)
		respond.trapno = 1 /* success, which means we resumed already */;

	return respond;
}


State System_control_component::system_control(State const &request)
{
	if (request.trapno == State::ACPI_SUSPEND_REQUEST)
		return acpi_suspend(request);

	if (request.trapno == State::MSR_ACCESS) {
		auto const  msr_cap = platform_specific().core_pd_sel() + 4;
		Nova::Utcb &utcb    = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
		return msr_access(request, utcb, msr_cap);
	}

	return State();
}
