/*
 * \brief  Platform driver for ARM root component
 * \author Stefan Kalkowski
 * \date   2020-04-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <root.h>

using Version = Driver::Session_component::Policy_version;

void Driver::Root::update_policy()
{
	_sessions.for_each([&] (Session_component & sc)
	{
		with_matching_policy(sc._label, _config.node(),
			[&] (Node const &policy) {
				sc.update_policy(policy.attribute_value("info", false),
				                 policy.attribute_value("version", Version())); },
			[&] {
				error("No matching policy for '", sc._label.string(),
				      "' anymore, will close the session!");
				close(sc.cap());
			});
	});
}


Driver::Root::Create_result Driver::Root::_create_session(const char *args)
{
	return with_matching_policy(label_from_args(args), _config.node(),

		[&] (Node const &policy) {
			return _alloc_obj(_env, _config, _devices, _sessions, _io_mmu_devices,
			                  _irq_controller_registry,
			                  label_from_args(args),
			                  session_resources_from_args(args),
			                  session_diag_from_args(args),
			                  policy.attribute_value("info", false),
			                  policy.attribute_value("version", Version()),
			                  _io_mmu_present || _kernel_iommu, _kernel_iommu);
		},
		[&] () -> Create_result {
			error("Invalid session request, no matching policy for ",
			      "'", label_from_args(args), "'");
			return Create_error::DENIED;
		});
}


void Driver::Root::_upgrade_session(Session_component &sc, const char * args)
{
	sc.upgrade(ram_quota_from_args(args));
	sc.upgrade(cap_quota_from_args(args));
}


void Driver::Root::add_range(Device const & dev, Range const & range)
{
	_sessions.for_each([&] (Session_component & sc) {
		if (!sc.matches(dev)) return;
		sc._dma_allocator.reserve(range.start, range.size);
	});

	/* add default mapping and enable for corresponding pci device */
	_io_mmu_devices.for_each([&] (Io_mmu & io_mmu_dev) {
		dev.with_optional_io_mmu(io_mmu_dev.name(), [&] () {

			io_mmu_dev.add_default_range(range, range.start);
			dev.for_pci_config([&] (Device::Pci_config const & cfg) {
				io_mmu_dev.enable_default_mappings(
					{cfg.bus_num, cfg.dev_num, cfg.func_num});
			});

		});
	});
}


void Driver::Root::remove_range(Device const & dev, Range const & range)
{
	_sessions.for_each([&] (Session_component & sc) {
		if (!sc.matches(dev)) return;
		sc._dma_allocator.unreserve(range.start, range.size);
	});

	/*
	 * remark: There is no need to remove default mappings since once known
	 *         default mappings should be preserved. Double-insertion in case
	 *         mapping are re-added at a later point in time is taken care of.
	 */
}


Driver::Root::Root(Env                          & env,
                   Sliced_heap                  & sliced_heap,
                   Attached_rom_dataspace const & config,
                   Device_model                 & devices,
                   Io_mmu_devices               & io_mmu_devices,
                   Registry<Irq_controller>     & irq_controller_registry,
                   bool const                     kernel_iommu)
: Root_component<Session_component>(env.ep(), sliced_heap),
  _env(env), _config(config), _devices(devices),
  _io_mmu_devices(io_mmu_devices),
  _irq_controller_registry(irq_controller_registry),
  _kernel_iommu(kernel_iommu)
{ }
