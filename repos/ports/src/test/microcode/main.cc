/*
 * \brief  Testing microcode status
 * \author Alexander Boetcher
 * \date   2018-08-01
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>

#include <util/mmio.h>

using Genode::uint8_t;

struct Microcode : Genode::Mmio<36>
{
	struct Version  : Register< 0, 32> { };
	struct Revision : Register< 4, 32> { };
	struct Date     : Register< 8, 32> {
		struct Year  : Bitfield< 0, 16> { };
		struct Day   : Bitfield<16,  8> { };
		struct Month : Bitfield<24,  8> { };
	};
	struct Cpuid   : Register<12, 32> {
		struct Stepping   : Bitfield<  0, 4> { };
		struct Model      : Bitfield<  4, 4> { };
		struct Family     : Bitfield<  8, 4> { };
		struct Type       : Bitfield< 12, 2> { };
		struct Model_ext  : Bitfield< 16, 4> { };
		struct Family_ext : Bitfield< 20, 8> { };
	};
	struct Processor_flags : Register<24, 32> {
		struct Flags      : Bitfield<  0, 8> { };
	};
	struct Datasize  : Register<28, 32> { };
	struct Totalsize : Register<32, 32> { };

	using Mmio::Mmio;
};

static void read_micro_code_rom(Genode::Env &env, Genode::String<9> &rom_name,
                                unsigned const id, uint8_t const family,
                                uint8_t const model, uint8_t const stepping,
                                uint8_t const platform, unsigned const patch)
{
	using namespace Genode;

	try {
		Attached_rom_dataspace mc_rom(env, rom_name.string());
		Microcode mc_bits({mc_rom.local_addr<char>(), mc_rom.size()});

		unsigned total_size = mc_bits.read<Microcode::Totalsize>();
		unsigned data_size = mc_bits.read<Microcode::Datasize>();

		/* see Intel - 9.11 MICROCODE UPDATE FACILITIES */
		if (data_size == 0) data_size = 2000;
		if (total_size == 0) total_size = data_size + 48;

		if (total_size < data_size || total_size <= 48) {
			error(id, " ", rom_name, " - microcode sizes are bogus ", total_size, " ", data_size);
			return;
		}
		unsigned const extension_size = total_size - data_size - 48;
		if (extension_size)
			warning("microcode patch contains extension we don't support yet!");

		if (mc_bits.read<Microcode::Version>() != 1) {
			error(id, " ", rom_name, " - unsupported microcode version");
			return;
		}

		uint8_t  mc_family   = (mc_bits.read<Microcode::Cpuid::Family_ext>() << 4)
		                       | mc_bits.read<Microcode::Cpuid::Family>();
		uint8_t  mc_model    = (mc_bits.read<Microcode::Cpuid::Model_ext>() << 4)
		                       | mc_bits.read<Microcode::Cpuid::Model>();
		uint8_t  mc_stepping = mc_bits.read<Microcode::Cpuid::Stepping>();
		unsigned mc_patch    = mc_bits.read<Microcode::Revision>();
		uint8_t  mc_flags    = mc_bits.read<Microcode::Processor_flags::Flags>();

		bool const platform_match = (1U << platform) & mc_flags;
		bool const match = (mc_family == family) && (mc_model == model) &&
                           (mc_stepping == stepping) && platform_match;

		log(id,
		    " ",  Hex(family, Hex::OMIT_PREFIX, Hex::PAD),
		    ":",  Hex(model, Hex::OMIT_PREFIX, Hex::PAD),
		    ":",  Hex(stepping, Hex::OMIT_PREFIX, Hex::PAD),
		    " [", Hex(patch, Hex::OMIT_PREFIX), "]",
		    " - microcode: "
		    " ",  Hex(mc_family, Hex::OMIT_PREFIX, Hex::PAD),
		    ":",  Hex(mc_model, Hex::OMIT_PREFIX, Hex::PAD),
		    ":",  Hex(mc_stepping, Hex::OMIT_PREFIX, Hex::PAD),
		    " [", Hex(mc_patch, Hex::OMIT_PREFIX), "] from ",
		    Hex(mc_bits.read<Microcode::Date::Month>(), Hex::OMIT_PREFIX), "/",
		    Hex(mc_bits.read<Microcode::Date::Day>()  , Hex::OMIT_PREFIX), "/",
		    Hex(mc_bits.read<Microcode::Date::Year>() , Hex::OMIT_PREFIX),
		    match ? " matches" : " mismatches",
		    platform_match ? "" : ", platform mismatch");

		if (!match)
		   warning(id, " - microcode not applicable to CPU");
		else
			if (mc_patch > patch)
				warning(id, " - microcode of CPU is not on last patch level!");
	} catch (...) {
		warning(id, " ", rom_name, " - no microcode available");
	}
}

static void inline cpuid(unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
  asm volatile ("cpuid" : "+a" (*eax), "+d" (*edx), "+b" (*ebx), "+c"(*ecx) :: "memory");
}

static unsigned cpu_attribute_value(Genode::Xml_node const &cpu, char const *attribute)
{
	if (!cpu.has_attribute(attribute)) {
		Genode::error("missing cpu attribute ", attribute);
		throw Genode::Exception();
	}

	return cpu.attribute_value(attribute, 0U);
}

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	/* we support currently solely Intel CPUs */
	uint32_t eax = 0x0, ebx = 0, edx = 0, ecx = 0;
	cpuid(&eax, &ebx, &ecx, &edx);
	const char * intel = "GenuineIntel";

	if (memcmp(intel, &ebx, 4) || memcmp(intel + 4, &edx, 4) || memcmp(intel + 8, &ecx, 4)) {
		error("no Intel CPU detected");
		return;
	}

	Attached_rom_dataspace const platform_info { env, "platform_info" };

	log("CPU family:model:stepping [patch]");

	try {
		Xml_node const cpus = platform_info.xml().sub_node("hardware").sub_node("cpus");
		cpus.for_each_sub_node("cpu", [&] (Xml_node cpu) {

			unsigned const id       = cpu_attribute_value(cpu, "id");
			uint8_t  const family   = cpu_attribute_value(cpu, "family");
			uint8_t  const model    = cpu_attribute_value(cpu, "model");
			uint8_t  const stepping = cpu_attribute_value(cpu, "stepping");
			uint8_t  const platform = cpu_attribute_value(cpu, "platform");
			unsigned const patch    = cpu_attribute_value(cpu, "patch");

			String<9> name(Hex(family, Hex::OMIT_PREFIX, Hex::PAD), "-",
			               Hex(model, Hex::OMIT_PREFIX, Hex::PAD), "-",
			               Hex(stepping, Hex::OMIT_PREFIX, Hex::PAD));

			read_micro_code_rom(env, name, id, family, model, stepping,
			                    platform, patch);
		});
	} catch (...) {
		error("could not parse CPU data from platform_info");
	}
	log("microcode check done");
}
