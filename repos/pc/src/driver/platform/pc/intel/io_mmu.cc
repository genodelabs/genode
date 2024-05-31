/*
 * \brief  Intel IOMMU implementation
 * \author Johannes Schlatow
 * \date   2023-08-15
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <intel/io_mmu.h>

using namespace Driver;

static void attribute_hex(Genode::Xml_generator & xml, char const * name,
                          unsigned long long value)
{
	xml.attribute(name, Genode::String<32>(Genode::Hex(value)));
}


template <typename TABLE>
void Intel::Io_mmu::Domain<TABLE>::enable_pci_device(Io_mem_dataspace_capability const,
                                                     Pci::Bdf const & bdf)
{
	Domain_id cur_domain =
		_intel_iommu.root_table().insert_context<TABLE::address_width()>(
			bdf, _translation_table_phys, _domain_id);

	/**
	 * We need to invalidate the context-cache entry for this device and
	 * IOTLB entries for the previously used domain id.
	 *
	 * If the IOMMU caches unresolved requests, we must invalidate those. In
	 * legacy translation mode, these are cached with domain id 0. This is
	 * currently implemented as global invalidation, though.
	 *
	 * Some older architectures also require explicit write-buffer flushing
	 * unless invalidation takes place.
	 */
	if (cur_domain.valid())
		_intel_iommu.invalidate_all(cur_domain, Pci::Bdf::rid(bdf));
	else if (_intel_iommu.caching_mode())
		_intel_iommu.invalidate_context(Domain_id(), Pci::Bdf::rid(bdf));
	else
		_intel_iommu.flush_write_buffer();
}


template <typename TABLE>
void Intel::Io_mmu::Domain<TABLE>::disable_pci_device(Pci::Bdf const & bdf)
{
	_intel_iommu.root_table().remove_context(bdf, _translation_table_phys);

	/* lookup default mappings and insert instead */
	_intel_iommu.apply_default_mappings(bdf);

	_intel_iommu.invalidate_all(_domain_id);
}


template <typename TABLE>
void Intel::Io_mmu::Domain<TABLE>::add_range(Range const & range,
                                             addr_t const paddr,
                                             Dataspace_capability const)
{
	addr_t const             vaddr   { range.start };
	size_t const             size    { range.size };

	Page_flags flags { RW, NO_EXEC, USER, NO_GLOBAL,
	                   RAM, Genode::CACHED };

	_translation_table.insert_translation(vaddr, paddr, size, flags,
	                                      _table_allocator,
	                                      !_intel_iommu.coherent_page_walk(),
	                                      _intel_iommu.supported_page_sizes());

	if (_skip_invalidation)
		return;

	/* only invalidate iotlb if failed requests are cached */
	if (_intel_iommu.caching_mode())
		_intel_iommu.invalidate_iotlb(_domain_id, vaddr, size);
	else
		_intel_iommu.flush_write_buffer();
}


template <typename TABLE>
void Intel::Io_mmu::Domain<TABLE>::remove_range(Range const & range)
{
	_translation_table.remove_translation(range.start, range.size,
	                                      _table_allocator,
	                                      !_intel_iommu.coherent_page_walk());

	if (!_skip_invalidation)
		_intel_iommu.invalidate_iotlb(_domain_id, range.start, range.size);
}


/* Flush write-buffer if required by hardware */
void Intel::Io_mmu::flush_write_buffer()
{
	if (!read<Capability::Rwbf>())
		return;

	Global_status::access_t  status = read<Global_status>();
	Global_command::access_t cmd    = status;

	/* keep status bits but clear one-shot bits */
	Global_command::Srtp::clear(cmd);
	Global_command::Sirtp::clear(cmd);

	Global_command::Wbf::set(cmd);
	write<Global_command>(cmd);

	/* wait until command completed */
	while (read<Global_status>() != status);
}


/**
 * Clear IOTLB.
 *
 * By default, we perform a global invalidation. When provided with a valid
 * Domain_id, a domain-specific invalidation is conducted. If provided with
 * a DMA address and size, a page-selective invalidation is performed.
 *
 * See Table 25 for required invalidation scopes.
 */
void Intel::Io_mmu::invalidate_iotlb(Domain_id domain_id, addr_t, size_t)
{
	unsigned requested_scope = Context_command::Cirg::GLOBAL;
	if (domain_id.valid())
		requested_scope = Context_command::Cirg::DOMAIN;

	/* wait for ongoing invalidation request to be completed */
	while (Iotlb::Invalidate::get(read_iotlb_reg()));

	/* invalidate IOTLB */
	write_iotlb_reg(Iotlb::Invalidate::bits(1) |
	                Iotlb::Iirg::bits(requested_scope) |
	                Iotlb::Dr::bits(1) | Iotlb::Dw::bits(1) |
	                Iotlb::Did::bits(domain_id.value));

	/* wait for completion */
	while (Iotlb::Invalidate::get(read_iotlb_reg()));

	/* check for errors */
	unsigned actual_scope = Iotlb::Iaig::get(read_iotlb_reg());
	if (!actual_scope)
		error("IOTLB invalidation failed (scope=", requested_scope, ")");
	else if (_verbose && actual_scope < requested_scope)
		warning("Performed IOTLB invalidation with different granularity ",
		        "(requested=", requested_scope, ", actual=", actual_scope, ")");

	/* XXX implement page-selective-within-domain IOTLB invalidation */
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
void Intel::Io_mmu::invalidate_context(Domain_id domain_id, Pci::rid_t rid)
{
	/**
	 * We are using the register-based invalidation interface for the
	 * moment. This is only supported in legacy mode and for major
	 * architecture version 5 and lower (cf. 6.5).
	 */

	if (read<Version::Major>() > 5) {
		error("Unable to invalidate caches: Register-based invalidation only ",
		      "supported in architecture versions 5 and lower");
		return;
	}

	/* make sure that there is no context invalidation ongoing */
	while (read<Context_command::Invalidate>());

	unsigned requested_scope = Context_command::Cirg::GLOBAL;
	if (domain_id.valid())
		requested_scope = Context_command::Cirg::DOMAIN;

	if (rid != 0)
		requested_scope = Context_command::Cirg::DEVICE;

	/* clear context cache */
	write<Context_command>(Context_command::Invalidate::bits(1) |
	                       Context_command::Cirg::bits(requested_scope) |
	                       Context_command::Sid::bits(rid) |
	                       Context_command::Did::bits(domain_id.value));


	/* wait for completion */
	while (read<Context_command::Invalidate>());

	/* check for errors */
	unsigned actual_scope = read<Context_command::Caig>();
	if (!actual_scope)
		error("Context-cache invalidation failed (scope=", requested_scope, ")");
	else if (_verbose && actual_scope < requested_scope)
		warning("Performed context-cache invalidation with different granularity ",
		        "(requested=", requested_scope, ", actual=", actual_scope, ")");
}


void Intel::Io_mmu::invalidate_all(Domain_id domain_id, Pci::rid_t rid)
{
	invalidate_context(domain_id, rid);

	/* XXX clear PASID cache if we ever switch from legacy mode translation */

	invalidate_iotlb(domain_id, 0, 0);
}


void Intel::Io_mmu::_handle_faults()
{
	if (_fault_irq.constructed())
		_fault_irq->ack_irq();

	if (read<Fault_status::Pending>()) {
		if (read<Fault_status::Overflow>())
			error("Fault recording overflow");

		if (read<Fault_status::Iqe>())
			error("Invalidation queue error");

		/* acknowledge all faults */
		write<Fault_status>(0x7d);

		error("Faults records for ", name());
		unsigned num_registers = read<Capability::Nfr>() + 1;
		for (unsigned i = read<Fault_status::Fri>(); ; i = (i + 1) % num_registers) {
			Fault_record_hi::access_t hi = read_fault_record<Fault_record_hi>(i);

			if (!Fault_record_hi::Fault::get(hi))
				break;

			Fault_record_hi::access_t lo = read_fault_record<Fault_record_lo>(i);

			error("Fault: hi=", Hex(hi),
			      ", reason=", Hex(Fault_record_hi::Reason::get(hi)),
			      ", type=",   Hex(Fault_record_hi::Type::get(hi)),
			      ", AT=",     Hex(Fault_record_hi::At::get(hi)),
			      ", EXE=",    Hex(Fault_record_hi::Exe::get(hi)),
			      ", PRIV=",   Hex(Fault_record_hi::Priv::get(hi)),
			      ", PP=",     Hex(Fault_record_hi::Pp::get(hi)),
			      ", Source=", Hex(Fault_record_hi::Source::get(hi)),
			      ", info=",   Hex(Fault_record_lo::Info::get(lo)));


			clear_fault_record(i);
		}
	}
}


void Intel::Io_mmu::generate(Xml_generator & xml)
{
	xml.node("intel", [&] () {
		xml.attribute("name", name());

		const bool enabled = (bool)read<Global_status::Enabled>();
		const bool rtps    = (bool)read<Global_status::Rtps>();
		const bool ires    = (bool)read<Global_status::Ires>();
		const bool irtps   = (bool)read<Global_status::Irtps>();
		const bool cfis    = (bool)read<Global_status::Cfis>();

		xml.attribute("dma_remapping", enabled && rtps);
		xml.attribute("msi_remapping", ires && irtps);
		xml.attribute("irq_remapping", ires && irtps && !cfis);

		/* dump registers */
		xml.attribute("version", String<16>(read<Version::Major>(), ".",
		                                    read<Version::Minor>()));

		xml.node("register", [&] () {
			xml.attribute("name", "Capability");
			attribute_hex(xml, "value", read<Capability>());
			xml.attribute("esrtps", (bool)read<Capability::Esrtps>());
			xml.attribute("esirtps", (bool)read<Capability::Esirtps>());
			xml.attribute("rwbf",   (bool)read<Capability::Rwbf>());
			xml.attribute("nfr",     read<Capability::Nfr>());
			xml.attribute("domains", read<Capability::Domains>());
			xml.attribute("caching", (bool)read<Capability::Caching_mode>());
		});

		xml.node("register", [&] () {
			xml.attribute("name", "Extended Capability");
			attribute_hex(xml, "value", read<Extended_capability>());
			xml.attribute("interrupt_remapping",
			              (bool)read<Extended_capability::Ir>());
			xml.attribute("page_walk_coherency",
			              (bool)read<Extended_capability::Page_walk_coherency>());
		});

		xml.node("register", [&] () {
			xml.attribute("name", "Global Status");
			attribute_hex(xml, "value", read<Global_status>());
			xml.attribute("qies",    (bool)read<Global_status::Qies>());
			xml.attribute("ires",    (bool)read<Global_status::Ires>());
			xml.attribute("rtps",    (bool)read<Global_status::Rtps>());
			xml.attribute("irtps",   (bool)read<Global_status::Irtps>());
			xml.attribute("cfis",    (bool)read<Global_status::Cfis>());
			xml.attribute("enabled", (bool)read<Global_status::Enabled>());
		});

		if (!_verbose)
			return;

		xml.node("register", [&] () {
			xml.attribute("name", "Fault Status");
			attribute_hex(xml, "value", read<Fault_status>());
			attribute_hex(xml, "fri",   read<Fault_status::Fri>());
			xml.attribute("iqe", (bool)read<Fault_status::Iqe>());
			xml.attribute("ppf", (bool)read<Fault_status::Pending>());
			xml.attribute("pfo", (bool)read<Fault_status::Overflow>());
		});

		xml.node("register", [&] () {
			xml.attribute("name", "Fault Event Control");
			attribute_hex(xml, "value", read<Fault_event_control>());
			xml.attribute("mask", (bool)read<Fault_event_control::Mask>());
		});

		if (!read<Global_status::Rtps>())
			return;

		addr_t rt_addr = Root_table_address::Address::masked(read<Root_table_address>());

		xml.node("register", [&] () {
			xml.attribute("name", "Root Table Address");
			attribute_hex(xml, "value", rt_addr);
		});

		if (read<Root_table_address::Mode>() != Root_table_address::Mode::LEGACY) {
			error("Only supporting legacy translation mode");
			return;
		}

		/* dump root table, context table, and page tables */
		_report_helper.with_table<Root_table>(rt_addr,
			[&] (Root_table & root_table) {
				root_table.generate(xml, _env, _report_helper);
			});
	});
}


void Intel::Io_mmu::add_default_range(Range const & range, addr_t paddr)
{
	addr_t const             vaddr   { range.start };
	size_t const             size    { range.size };

	Page_flags flags { RW, NO_EXEC, USER, NO_GLOBAL,
	                   RAM, Genode::CACHED };

	try {
		_default_mappings.insert_translation(vaddr, paddr, size, flags,
		                                     supported_page_sizes());
	} catch (...) { /* catch any double insertions */ }
}


void Intel::Io_mmu::default_mappings_complete()
{
	Root_table_address::access_t rtp =
		Root_table_address::Address::masked(_managed_root_table.phys_addr());

	/* skip if already set */
	if (read<Root_table_address>() == rtp)
		return;

	/* insert contexts into managed root table */
	_default_mappings.copy_stage2(_managed_root_table);

	/* set root table address */
	write<Root_table_address>(rtp);

	/* issue set root table pointer command */
	_global_command<Global_command::Srtp>(1);

	/* caches must be cleared if Esrtps is not set (see 6.6) */
	if (!read<Capability::Esrtps>())
		invalidate_all();

	/* enable IOMMU */
	if (!read<Global_status::Enabled>())
		_global_command<Global_command::Enable>(1);

	log("enabled IOMMU ", name(), " with default mappings");
}


void Intel::Io_mmu::suspend()
{
	_s3_fec    = read<Fault_event_control>();
	_s3_fedata = read<Fault_event_data>();
	_s3_feaddr = read<Fault_event_address>();
	_s3_rta    = read<Root_table_address>();
}


void Intel::Io_mmu::resume()
{
	/* disable queued invalidation interface if it was re-enabled by kernel */
	if (read<Global_status::Enabled>() && read<Global_status::Qies>())
		_global_command<Global_command::Qie>(false);

	/* restore fault events only if kernel did not enable IRQ remapping */
	if (!read<Global_status::Ires>()) {
		write<Fault_event_control>(_s3_fec);
		write<Fault_event_data>(_s3_fedata);
		write<Fault_event_address>(_s3_feaddr);
	}

	/* issue set root table pointer command */
	write<Root_table_address>(_s3_rta);
	_global_command<Global_command::Srtp>(1);

	if (!read<Capability::Esrtps>())
		invalidate_all();

	/* enable IOMMU */
	if (!read<Global_status::Enabled>())
		_global_command<Global_command::Enable>(1);
}


Intel::Io_mmu::Io_mmu(Env                      & env,
                      Io_mmu_devices           & io_mmu_devices,
                      Device::Name       const & name,
                      Device::Io_mem::Range      range,
                      Context_table_allocator  & table_allocator,
                      unsigned                   irq_number)
: Attached_mmio(env, {(char *)range.start, range.size}),
  Driver::Io_mmu(io_mmu_devices, name),
  _env(env),
  _managed_root_table(_env, table_allocator, *this, !coherent_page_walk()),
  _default_mappings(_env, table_allocator, *this, !coherent_page_walk(),
                    _sagaw_to_levels()),
  _domain_allocator(_max_domains()-1)
{
	if (_broken_device()) {
		error(name, " reports invalid capability registers. Please disable VT-d/IOMMU.");
		return;
	}

	if (!read<Capability::Sagaw_4_level>() && !read<Capability::Sagaw_3_level>()) {
		error("IOMMU does not support 3- or 4-level page tables");
		return;
	}

	if (read<Global_status::Enabled>()) {
		log("IOMMU has been enabled during boot");

		/* disable queued invalidation interface */
		if (read<Global_status::Qies>())
			_global_command<Global_command::Qie>(false);
	}

	/* enable fault event interrupts (if not already enabled by kernel) */
	if (irq_number && !read<Global_status::Ires>()) {
		_fault_irq.construct(_env, irq_number, 0, Irq_session::TYPE_MSI);

		_fault_irq->sigh(_fault_handler);
		_fault_irq->ack_irq();

		Irq_session::Info info = _fault_irq->info();
		if (info.type == Irq_session::Info::INVALID)
			error("Unable to enable fault event interrupts for ", name);
		else {
			write<Fault_event_address>((Fault_event_address::access_t)info.address);
			write<Fault_event_data>((Fault_event_data::access_t)info.value);
			write<Fault_event_control::Mask>(0);
		}
	}
}
