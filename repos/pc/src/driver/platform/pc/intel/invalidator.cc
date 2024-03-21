/*
 * \brief  Intel IOMMU invalidation interfaces
 * \author Johannes Schlatow
 * \date   2024-03-21
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <intel/invalidator.h>
#include <intel/io_mmu.h>

/**
 * Clear IOTLB.
 *
 * By default, we perform a global invalidation. When provided with a valid
 * Domain_id, a domain-specific invalidation is conducted.
 *
 * See Table 25 for required invalidation scopes.
 */
void Intel::Register_invalidator::invalidate_iotlb(Domain_id domain_id)
{
	using Context_command = Context_mmio::Context_command;
	using Iotlb           = Iotlb_mmio::Iotlb;

	unsigned requested_scope = Context_command::Cirg::GLOBAL;
	if (domain_id.valid())
		requested_scope = Context_command::Cirg::DOMAIN;

	/* wait for ongoing invalidation request to be completed */
	while (_iotlb_mmio.read<Iotlb::Invalidate>());

	/* invalidate IOTLB */
	_iotlb_mmio.write<Iotlb>(Iotlb::Invalidate::bits(1) |
	                         Iotlb::Iirg::bits(requested_scope) |
	                         Iotlb::Dr::bits(1) | Iotlb::Dw::bits(1) |
	                         Iotlb::Did::bits(domain_id.value));

	/* wait for completion */
	while (_iotlb_mmio.read<Iotlb::Invalidate>());

	/* check for errors */
	unsigned actual_scope = _iotlb_mmio.read<Iotlb::Iaig>();
	if (!actual_scope)
		error("IOTLB invalidation failed (scope=", requested_scope, ")");
	else if (_verbose && actual_scope < requested_scope)
		warning("Performed IOTLB invalidation with different granularity ",
		        "(requested=", requested_scope, ", actual=", actual_scope, ")");

	/*
	 * Note: At the moment we have no practical benefit from implementing
	 * page-selective invalidation, because
	 * a) When adding a DMA buffer range, invalidation is only required if
	 *    caching mode is set. This is not supposed to occur on real hardware but
	 *    only in emulators.
	 * b) Removal of DMA buffer ranges typically occurs only when a domain is
	 *    destructed. In this case, invalidation is not issued for individual
	 *    buffers but for the entire domain once all buffer ranges have been
	 *    removed.
	 * c) We do not use the register-based invalidation interface if queued
	 *    invalidation is available.
	 */
}


/**
 * Clear context cache
 *
 * By default, we perform a global invalidation. When provided with a valid
 * Domain_id, a domain-specific invalidation is conducted. When a rid is 
 * provided, a device-specific invalidation is done.
 *
 * See Table 25 for required invalidation scopes.
 */
void Intel::Register_invalidator::invalidate_context(Domain_id domain_id, Pci::rid_t rid)
{
	using Context_command = Context_mmio::Context_command;

	/* make sure that there is no context invalidation ongoing */
	while (_context_mmio.read<Context_command::Invalidate>());

	unsigned requested_scope = Context_command::Cirg::GLOBAL;
	if (domain_id.valid())
		requested_scope = Context_command::Cirg::DOMAIN;

	if (rid != 0)
		requested_scope = Context_command::Cirg::DEVICE;

	/* clear context cache */
	_context_mmio.write<Context_command>(Context_command::Invalidate::bits(1) |
	                                     Context_command::Cirg::bits(requested_scope) |
	                                     Context_command::Sid::bits(rid) |
	                                     Context_command::Did::bits(domain_id.value));

	/* wait for completion */
	while (_context_mmio.read<Context_command::Invalidate>());

	/* check for errors */
	unsigned actual_scope = _context_mmio.read<Context_command::Caig>();
	if (!actual_scope)
		error("Context-cache invalidation failed (scope=", requested_scope, ")");
	else if (_verbose && actual_scope < requested_scope)
		warning("Performed context-cache invalidation with different granularity ",
		        "(requested=", requested_scope, ", actual=", actual_scope, ")");
}


void Intel::Register_invalidator::invalidate_all(Domain_id domain_id, Pci::rid_t rid)
{
	invalidate_context(domain_id, rid);

	/* XXX clear PASID cache if we ever switch from legacy mode translation */

	invalidate_iotlb(domain_id);
}
