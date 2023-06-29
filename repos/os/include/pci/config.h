/*
 * \brief  PCI, PCI-x, PCI-Express configuration declarations
 * \author Stefan Kalkowski
 * \date   2021-12-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __INCLUDE__PCI__CONFIG_H__
#define __INCLUDE__PCI__CONFIG_H__

#include <base/log.h>
#include <pci/types.h>
#include <util/reconstructible.h>
#include <util/mmio.h>

namespace Pci {
	struct Config;
	struct Config_type0;
	struct Config_type1;

	enum {
		DEVICES_PER_BUS_MAX        = 32,
		FUNCTION_PER_DEVICE_MAX    = 8,
		FUNCTION_PER_BUS_MAX       = DEVICES_PER_BUS_MAX *
		                             FUNCTION_PER_DEVICE_MAX,
		FUNCTION_CONFIG_SPACE_SIZE = 4096,
	};
};

struct Pci::Config : Genode::Mmio
{
	struct Vendor : Register<0x0, 16>
	{
		enum { INVALID = 0xffff };
	};

	struct Device : Register<0x2, 16> {};

	struct Command : Register<0x4, 16>
	{
		struct Io_space_enable          : Bitfield<0,  1> {};
		struct Memory_space_enable      : Bitfield<1,  1> {};
		struct Bus_master_enable        : Bitfield<2,  1> {};
		struct Special_cycle_enable     : Bitfield<3,  1> {};
		struct Memory_write_invalidate  : Bitfield<4,  1> {};
		struct Vga_palette_snoop        : Bitfield<5,  1> {};
		struct Parity_error_response    : Bitfield<6,  1> {};
		struct Idsel                    : Bitfield<7,  1> {};
		struct Serror_enable            : Bitfield<8,  1> {};
		struct Fast_back_to_back_enable : Bitfield<9,  1> {};
		struct Interrupt_enable         : Bitfield<10, 1> {};
	};

	struct Status : Register<0x6, 16>
	{
		struct Interrupt    : Bitfield<3,1> {};
		struct Capabilities : Bitfield<4,1> {};
	};

	struct Class_code_rev_id : Register<0x8, 32>
	{
		struct Revision   : Bitfield<0, 8> {};
		struct Class_code : Bitfield<8, 24> {};
	};

	struct Iface_class_code : Register<0x9, 8> {};
	struct Sub_class_code   : Register<0xa, 8> {};
	struct Base_class_code  : Register<0xb, 8>
	{
		enum { BRIDGE = 6 };
	};

	struct Header_type : Register<0xe, 8>
	{
		struct Type           : Bitfield<0,7> {};
		struct Multi_function : Bitfield<7,1> {};
	};

	struct Base_address : Mmio
	{
		struct Bar_32bit : Register<0, 32>
		{
			struct Memory_space_indicator : Bitfield<0,1>
			{
				enum { MEMORY = 0, IO = 1 };
			};

			struct Memory_type : Bitfield<1,2>
			{
				enum { SIZE_32BIT = 0, SIZE_64BIT = 2 };
			};

			struct Memory_prefetchable : Bitfield<3,1> {};

			struct Io_base     : Bitfield<2, 30> {};
			struct Memory_base : Bitfield<7, 25> {};
		};

		struct Upper_bits : Register<0x4, 32> { };

		Bar_32bit::access_t _conf_value { 0 };

		template <typename REG>
		typename REG::access_t _get_and_set(typename REG::access_t value)
		{
			write<REG>(0xffffffff);
			typename REG::access_t ret = read<REG>();
			write<REG>(value);
			return ret;
		}

		Bar_32bit::access_t _conf()
		{
			/*
			 * Initialize _conf_value on demand only to prevent read-write
			 * operations on BARs of invalid devices at construction time.
			 */
			if (!_conf_value)
				_conf_value = _get_and_set<Bar_32bit>(read<Bar_32bit>());
			return _conf_value;
		}

		Base_address(Genode::addr_t base) : Mmio(base) { }

		bool valid() { return _conf() != 0; }

		bool memory() {
			return !Bar_32bit::Memory_space_indicator::get(_conf()); }

		bool bit64()
		{
			return Bar_32bit::Memory_type::get(_conf()) ==
			       Bar_32bit::Memory_type::SIZE_64BIT;
		}

		bool prefetchable() {
			return Bar_32bit::Memory_prefetchable::get(_conf()); }

		Genode::uint64_t size()
		{
			if (memory()) {
				Genode::uint64_t size = 1 + ~Bar_32bit::Memory_base::masked(_conf());
				if (bit64())
					size += ((Genode::uint64_t)~_get_and_set<Upper_bits>(read<Upper_bits>()))<<32;

				return size;
			}
			else
				return 1 + ~Bar_32bit::Io_base::masked(_conf());
		}

		Genode::uint64_t addr()
		{
			if (memory())
				return (bit64()
					? ((Genode::uint64_t)read<Upper_bits>()<<32) : 0UL)
					| Bar_32bit::Memory_base::masked(read<Bar_32bit>());
			else
				return Bar_32bit::Io_base::masked(read<Bar_32bit>());
		}

		void set(Genode::uint64_t v)
		{
			if (!valid() || v == addr())
				return;

			if (memory()) {
				if (bit64())
					_get_and_set<Upper_bits>((Upper_bits::access_t)(v >> 32));
				_get_and_set<Bar_32bit>(Bar_32bit::Memory_base::masked(v & ~0U));
			} else
				_get_and_set<Bar_32bit>(Bar_32bit::Io_base::masked(v & ~0U));
		}
	};

	enum Base_addresses {
		BASE_ADDRESS_0            = 0x10,
		BASE_ADDRESS_COUNT_TYPE_0 = 6,
		BASE_ADDRESS_COUNT_TYPE_1 = 2,
	};

	struct Capability_pointer : Register<0x34, 8> {};

	struct Irq_line : Register<0x3c, 8>
	{
		enum { UNKNOWN = 0xff };
	};

	struct Irq_pin  : Register<0x3d, 8>
	{
		enum { NO_INT = 0, INTA, INTB, INTC, INTD };
	};


	/**********************
	 ** PCI Capabilities **
	 **********************/

	struct Pci_capability : Genode::Mmio
	{
		struct Id : Register<0,8>
		{
			enum {
				POWER_MANAGEMENT = 0x1,
				AGP              = 0x2,
				VITAL_PRODUCT    = 0x3,
				MSI              = 0x5,
				VENDOR           = 0x9,
				DEBUG            = 0xa,
				BRIDGE_SUB       = 0xd,
				PCI_E            = 0x10,
				MSI_X            = 0x11,
				SATA             = 0x12,
				ADVANCED         = 0x13,
			};
		};

		struct Pointer : Register<1,8> {};

		using Genode::Mmio::Mmio;
	};


	struct Power_management_capability : Pci_capability
	{
		struct Capabilities : Register<0x2, 16> {};

		struct Control_status : Register<0x4, 16>
		{
			struct Power_state : Bitfield<0, 2>
			{
				enum { D0, D1, D2, D3 };
			};

			struct No_soft_reset : Bitfield<3, 1> {};
			struct Pme_status    : Bitfield<15,1> {};
		};

		struct Data : Register<0x7, 8> {};

		using Pci_capability::Pci_capability;

		bool power_on(Delayer & delayer)
		{
			using Reg = Control_status::Power_state;
			if (read<Reg>() == Reg::D0)
				return false;

			write<Reg>(Reg::D0);

			/*
			 * PCI Express 4.3 - 5.3.1.4. D3 State
			 *
			 * "Unless Readiness Notifications mechanisms are used ..."
			 * "a minimum recovery time following a D3 hot → D0 transition of"
			 * "at least 10 ms ..."
			 */
			delayer.usleep(10'000);
			return true;
		}

		void power_off()
		{
			using Reg = Control_status::Power_state;
			if (read<Reg>() != Reg::D3) write<Reg>(Reg::D3);
		}


		bool soft_reset()
		{
			return !read<Control_status::No_soft_reset>();
		}
	};


	struct Msi_capability : Pci_capability
	{
		struct Control : Register<0x2, 16>
		{
			struct Enable                : Bitfield<0,1> {};
			struct Multi_message_capable : Bitfield<1,3> {};
			struct Multi_message_enable  : Bitfield<4,3> {};
			struct Large_address_capable : Bitfield<7,1> {};
		};

		struct Address_32 : Register<0x4, 32> {};
		struct Data_32    : Register<0x8, 16> {};

		struct Address_64_lower : Register<0x4, 32> {};
		struct Address_64_upper : Register<0x8, 32> {};
		struct Data_64          : Register<0xc, 16> {};

		using Pci_capability::Pci_capability;

		void enable(Genode::addr_t   address,
		            Genode::uint16_t data)
		{
			if (read<Control::Large_address_capable>()) {
				Genode::uint64_t addr = address;
				write<Address_64_upper>((Genode::uint32_t)(addr >> 32));
				write<Address_64_lower>((Genode::uint32_t)addr);
				write<Data_64>(data);
			} else {
				write<Address_32>((Genode::uint32_t)address);
				write<Data_32>(data);
			}
			write<Control::Enable>(1);
		};
	};


	struct Msi_x_capability : Pci_capability
	{
		struct Control : Register<0x2, 16>
		{
			struct Slots         : Bitfield<0,  10> {};
			struct Size          : Bitfield<0,  11> {};
			struct Function_mask : Bitfield<14, 1> {};
			struct Enable        : Bitfield<15, 1> {};
		};

		struct Table : Register<0x4, 32>
		{
			struct Bar_index : Bitfield<0, 3>  {};
			struct Offset    : Bitfield<3, 29> {};
		};

		struct Pending_bit_array : Register<0x8, 32>
		{
			struct Bar_index : Bitfield<0, 3>  {};
			struct Offset    : Bitfield<3, 29> {};
		};

		struct Table_entry : Genode::Mmio
		{
			enum { SIZE = 16 };

			struct Address_64_lower : Register<0x0, 32> { };
			struct Address_64_upper : Register<0x4, 32> { };
			struct Data             : Register<0x8, 32> { };
			struct Vector_control   : Register<0xc, 32>
			{
				struct Mask : Bitfield <0, 1> { };
			};

			using Genode::Mmio::Mmio;
		};

		using Pci_capability::Pci_capability;

		Genode::uint8_t bar() {
			return (Genode::uint8_t) read<Table::Bar_index>(); }

		Genode::size_t table_offset() {
			return read<Table::Offset>() << 3; }

		unsigned slots() { return read<Control::Slots>(); }

		void enable()
		{
			Control::access_t ctrl = read<Control>();
			Control::Function_mask::set(ctrl, 0);
			Control::Enable::set(ctrl, 1);
			write<Control>(ctrl);
		}
	};


	struct Pci_express_capability : Pci_capability
	{
		struct Capabilities : Register<0x2, 16> {};

		struct Device_capabilities : Register<0x4,  32>
		{
			struct Function_level_reset : Bitfield<28,1> {};
		};

		struct Device_control : Register<0x8,  16>
		{
			struct Function_level_reset : Bitfield<15,1> {};
		};

		struct Device_status : Register<0xa,  16>
		{
			struct Correctable_error    : Bitfield<0, 1> {};
			struct Non_fatal_error      : Bitfield<1, 1> {};
			struct Fatal_error          : Bitfield<2, 1> {};
			struct Unsupported_request  : Bitfield<3, 1> {};
			struct Aux_power            : Bitfield<4, 1> {};
			struct Transactions_pending : Bitfield<5, 1> {};
		};

		struct Link_capabilities : Register<0xc,  32>
		{
			struct Max_link_speed : Bitfield<0, 4> {};
		};

		struct Link_control : Register<0x10, 16>
		{
			struct Lbm_irq_enable : Bitfield<10,1> {};
		};

		struct Link_status : Register<0x12, 16>
		{
			struct Lbm_status : Bitfield<10,1> {};
		};

		struct Slot_capabilities   : Register<0x14, 32> {};
		struct Slot_control        : Register<0x18, 16> {};
		struct Slot_status         : Register<0x1a, 16> {};

		struct Root_control : Register<0x1c, 16>
		{
			struct Pme_irq_enable : Bitfield<3,1> {};
		};

		struct Root_status : Register<0x20, 32>
		{
			struct Pme : Bitfield<16,1> {};
		};

		struct Device_capabilities_2 : Register<0x24, 32> {};
		struct Device_control_2      : Register<0x28, 16> {};
		struct Device_status_2       : Register<0x2a, 16> {};
		struct Link_capabilities_2   : Register<0x2c, 32> {};

		struct Link_control_2 : Register<0x30, 16>
		{
			struct Link_speed : Bitfield<0, 4> {};
		};

		struct Link_status_2         : Register<0x32, 16> {};
		struct Slot_capabilities_2   : Register<0x34, 32> {};
		struct Slot_control_2        : Register<0x38, 16> {};
		struct Slot_status_2         : Register<0x3a, 16> {};

		using Pci_capability::Pci_capability;

		void power_management_event_enable()
		{
			write<Root_status::Pme>(1);
			write<Root_control::Pme_irq_enable>(1);
		};

		void clear_dev_errors()
		{
			Device_status::access_t v = read<Device_status>();
			Device_status::Correctable_error::set(v,1);
			Device_status::Non_fatal_error::set(v,1);
			Device_status::Fatal_error::set(v,1);
			Device_status::Unsupported_request::set(v,1);
			Device_status::Aux_power::set(v,1);
			write<Device_status>(v);
		}

		void link_bandwidth_management_enable()
		{
			write<Link_status::Lbm_status>(1);
			write<Link_control::Lbm_irq_enable>(1);
		}

		void reset(Delayer & delayer)
		{
			if (!read<Device_capabilities::Function_level_reset>())
				return;
			write<Device_control::Function_level_reset>(1);
			try {
				wait_for(Attempts(100), Microseconds(10000), delayer,
				         Device_status::Transactions_pending::Equal(0));
			} catch(Polling_timeout) { }
		}
	};


	/*********************************
	 ** PCI-E extended capabilities **
	 *********************************/

	enum { PCI_E_EXTENDED_CAPS_OFFSET = 0x100U };

	struct Pci_express_extended_capability : Genode::Mmio
	{
		struct Id : Register<0x0, 16>
		{
			enum {
				INVALID                  = 0x0,
				ADVANCED_ERROR_REPORTING = 0x1,
				VIRTUAL_CHANNEL          = 0x2,
				DEVICE_SERIAL_NUMBER     = 0x3,
				POWER_BUDGETING          = 0x4,
				VENDOR                   = 0xb,
				MULTI_ROOT_IO_VIRT       = 0x11,
			};
		};

		struct Next_and_version : Register<0x2, 16>
		{
			struct Offset : Bitfield<4, 12> {};
		};

		using Genode::Mmio::Mmio;
	};


	struct Advanced_error_reporting_capability : Pci_express_extended_capability
	{
		struct Uncorrectable_error_status : Register<0x4,  32> {};
		struct Correctable_error_status   : Register<0x10, 32> {};

		struct Root_error_command : Register<0x2c, 32>
		{
			struct Correctable_error_enable : Bitfield<0,1> {};
			struct Non_fatal_error_enable   : Bitfield<1,1> {};
			struct Fatal_error_enable       : Bitfield<2,1> {};
		};

		struct Root_error_status : Register<0x30, 32> {};

		using Pci_express_extended_capability::Pci_express_extended_capability;

		void enable()
		{
			Root_error_command::access_t v = 0;
			Root_error_command::Correctable_error_enable::set(v,1);
			Root_error_command::Non_fatal_error_enable::set(v,1);
			Root_error_command::Fatal_error_enable::set(v,1);
			write<Root_error_command>(v);
		};

		void clear()
		{
			write<Root_error_status>(read<Root_error_status>());
			write<Correctable_error_status>(read<Correctable_error_status>());
			write<Uncorrectable_error_status>(read<Uncorrectable_error_status>());
		};
	};

	Genode::Constructible<Power_management_capability>         power_cap   {};
	Genode::Constructible<Msi_capability>                      msi_cap     {};
	Genode::Constructible<Msi_x_capability>                    msi_x_cap   {};
	Genode::Constructible<Pci_express_capability>              pci_e_cap   {};
	Genode::Constructible<Advanced_error_reporting_capability> adv_err_cap {};

	Base_address bar0 { base() + BASE_ADDRESS_0       };
	Base_address bar1 { base() + BASE_ADDRESS_0 + 0x4 };

	void clear_errors() {
		if (adv_err_cap.constructed()) adv_err_cap->clear(); }

	void scan()
	{
		using namespace Genode;

		if (!read<Status::Capabilities>())
			return;

		uint16_t off = read<Capability_pointer>();
		while (off) {
			Pci_capability cap(base() + off);
			switch(cap.read<Pci_capability::Id>()) {
			case Pci_capability::Id::POWER_MANAGEMENT:
				power_cap.construct(base()+off); break;
			case Pci_capability::Id::MSI:
				msi_cap.construct(base()+off);   break;
			case Pci_capability::Id::MSI_X:
				msi_x_cap.construct(base()+off); break;
			case Pci_capability::Id::PCI_E:
				pci_e_cap.construct(base()+off); break;

			default:
				/* ignore unhandled capability */ ;
			}
			off = cap.read<Pci_capability::Pointer>();
		}

		if (!pci_e_cap.constructed())
			return;

		off = PCI_E_EXTENDED_CAPS_OFFSET;
		while (off) {
			Pci_express_extended_capability cap(base() + off);
			switch (cap.read<Pci_express_extended_capability::Id>()) {
			case Pci_express_extended_capability::Id::INVALID:
				return;
			case Pci_express_extended_capability::Id::ADVANCED_ERROR_REPORTING:
				adv_err_cap.construct(base() + off); break;

			default:
				/* ignore unhandled extended capability */ ;
			}
			off = cap.read<Pci_express_extended_capability::Next_and_version::Offset>();
		}
	}

	using Genode::Mmio::Mmio;

	bool valid() {
		return read<Vendor>() != Vendor::INVALID; }

	bool bridge()
	{
		return read<Header_type::Type>() == 1 ||
		       read<Base_class_code>() == Base_class_code::BRIDGE;
	}

	template <typename MEM_FN, typename IO_FN>
	void for_each_bar(MEM_FN const & memory, IO_FN const & io)
	{
		Genode::addr_t const reg_addr = base() + BASE_ADDRESS_0;
		Genode::size_t const reg_cnt  =
			(read<Header_type::Type>()) ? BASE_ADDRESS_COUNT_TYPE_1
			                            : BASE_ADDRESS_COUNT_TYPE_0;

		for (unsigned i = 0; i < reg_cnt; i++) {
			Base_address reg0(reg_addr + i*0x4);
			if (!reg0.valid())
				continue;
			if (reg0.memory()) {
				memory(reg0.addr(), reg0.size(), i, reg0.prefetchable());
				if (reg0.bit64()) i++;
			} else
				io(reg0.addr(), reg0.size(), i);
		}
	};

	void set_bar_address(unsigned idx, Genode::uint64_t addr)
	{
		if (idx > 5 || (idx > 1 && bridge()))
			return;

		Base_address bar { base() + BASE_ADDRESS_0 + idx*0x4 };
		bar.set(addr);
	}

	void power_on(Delayer & delayer)
	{
		if (!power_cap.constructed() || !power_cap->power_on(delayer))
			return;

		if (power_cap->soft_reset() && pci_e_cap.constructed())
			pci_e_cap->reset(delayer);
	}

	void power_off()
	{
		if (power_cap.constructed()) power_cap->power_off();
	}
};


struct Pci::Config_type0 : Pci::Config
{
	struct Expansion_rom_base_addr : Register<0x30, 32> {};

	using Pci::Config::Config;

	Base_address bar2 { base() + BASE_ADDRESS_0 + 0x8  };
	Base_address bar3 { base() + BASE_ADDRESS_0 + 0xc  };
	Base_address bar4 { base() + BASE_ADDRESS_0 + 0x10 };
	Base_address bar5 { base() + BASE_ADDRESS_0 + 0x14 };

	struct Subsystem_vendor : Register<0x2c, 16> { };
	struct Subsystem_device : Register<0x2e, 16> { };
};


struct Pci::Config_type1 : Pci::Config
{
	struct Sec_lat_timer_bus : Register<0x18, 32>
	{
		struct Primary_bus   : Bitfield<0,  8> {};
		struct Secondary_bus : Bitfield<8,  8> {};
		struct Sub_bus       : Bitfield<16, 8> {};
	};

	struct Io_base_limit : Register<0x1c, 16> {};

	struct Memory_base  : Register<0x20, 16> {};
	struct Memory_limit : Register<0x22, 16> {};

	struct Prefetchable_memory_base : Register<0x24, 32> {};
	struct Prefetchable_memory_base_upper : Register<0x28, 32> {};
	struct Prefetchable_memory_limit_upper : Register<0x2c, 32> {};

	struct Io_base_limit_upper : Register<0x30, 32> {};

	struct Expansion_rom_base_addr : Register<0x38, 32> {};

	struct Bridge_control : Register<0x3e, 16>
	{
		struct Serror : Bitfield<1, 1> {};
	};

	using Pci::Config::Config;

	bus_t primary_bus_number() {
		return (bus_t) read<Sec_lat_timer_bus::Primary_bus>(); }

	bus_t secondary_bus_number() {
		return (bus_t) read<Sec_lat_timer_bus::Secondary_bus>(); }

	bus_t subordinate_bus_number() {
		return (bus_t) read<Sec_lat_timer_bus::Sub_bus>(); }
};

#endif /* __INCLUDE__PCI__CONFIG_H__ */
