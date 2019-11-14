/*
 * \brief  VMM ARM Generic Interrupt Controller device model
 * \author Stefan Kalkowski
 * \date   2019-08-05
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__GIC_H_
#define _SRC__SERVER__VMM__GIC_H_

#include <mmio.h>

#include <base/env.h>
#include <drivers/defs/arm_v7.h>
#include <vm_session/connection.h>
#include <util/list.h>
#include <util/register.h>
#include <util/reconstructible.h>

namespace Vmm {
	class Cpu_base;
	class Gic;
}

class Vmm::Gic : public Vmm::Mmio_device
{
	public:

		enum Irqs {
			MAX_SGI  = 16,
			MAX_PPI  = 16,
			MAX_SPI  = 992,
			MAX_IRQ  = 1020,
			SPURIOUS = 1023,
		};

		class Irq : public Genode::List<Irq>::Element
		{
			public:

				struct List : Genode::List<Irq>
				{
					void insert(Irq & irq);
					Irq * highest_enabled();
				};

				struct Irq_handler {
					virtual void eoi()   {};
					virtual void enabled() {};
					virtual void disabled() {};
				};

				enum Type   { SGI, PPI, SPI };
				enum State  { INACTIVE, ACTIVE, PENDING, ACTIVE_PENDING };
				enum Config { LEVEL, EDGE };

				Irq(unsigned num, Type t, List &pending_list);

				bool     enabled() const;
				bool     active()  const;
				bool     pending() const;
				bool     level()   const;
				unsigned number()  const;
				Genode::uint8_t priority() const;
				Genode::uint8_t target() const;

				void enable();
				void disable();
				void activate();
				void deactivate();
				void assert();
				void deassert();
				void level(bool l);
				void priority(Genode::uint8_t p);
				void target(Genode::uint8_t t);
				void handler(Irq_handler & handler);

			private:

				Irq(Irq const &);
				Irq &operator = (Irq const &);

				bool            _enabled { false    };
				Type            _type    { SPI      };
				State           _state   { INACTIVE };
				Config          _config  { LEVEL    };
				unsigned        _num     { 0 };
				Genode::uint8_t _prio    { 0 };
				Genode::uint8_t _target  { 1 };
				List          & _pending_list;
				Irq_handler   * _handler { nullptr };
		};

		struct Irq_reg: Mmio_register
		{
			unsigned const irq_count;

			virtual Register read(Irq & irq)  { return 0; }
			virtual void     write(Irq & irq, Register reg) { }

			Register read(Address_range  & access, Cpu&) override;
			void     write(Address_range & access, Cpu&, Register value) override;

			Irq_reg(Mmio_register::Name name,
			        Mmio_register::Type type,
			        Genode::uint64_t    start,
			        unsigned            bits_per_irq,
			        unsigned            irq_count)
			: Mmio_register(name, type, start, bits_per_irq*irq_count/8),
			  irq_count(irq_count) {}
		};

		class Gicd_banked
		{
			public:

				Irq & irq(unsigned num);
				void handle_irq();
				bool pending_irq();

				Gicd_banked(Cpu_base & cpu, Gic & gic, Mmio_bus & bus);

			private:

				Cpu_base                 & _cpu;
				Gic                      & _gic;
				Genode::Constructible<Irq> _sgi[MAX_SGI];
				Genode::Constructible<Irq> _ppi[MAX_PPI];
				Irq::List                  _pending_list;

				struct Redistributor : Mmio_device
				{
					unsigned      cpu_id;
					bool          last;
					Mmio_register gicr_ctlr     { "GICR_CTLR", Mmio_register::RO,
					                              0x0, 4, 0b10010 };
					Mmio_register gicr_typer    { "GICR_TYPER", Mmio_register::RO,
					                              0x8, 8,
					                              (Genode::uint64_t)cpu_id<<32 |
					                              cpu_id<<8 | (last ? 1<<4 : 0) };
					Mmio_register gicr_waker    { "GICR_WAKER", Mmio_register::RO,
					                              0x14, 4, 0 };
					Mmio_register gicr_pidr2    { "GICR_PIDR2", Mmio_register::RO,
					                              0xffe8, 4, (3<<4) };
					Mmio_register gicr_igroupr0 { "GICR_IGROUPR0", Mmio_register::RO,
					                              0x10080, 4, 0 };

					struct Gicr_isenabler0 : Irq_reg
					{
						Register read(Irq & irq)              { return irq.enabled(); }
						void     write(Irq & irq, Register v) { if (v) irq.enable();  }

						Gicr_isenabler0()
						: Irq_reg("GICR_ISENABLER0", Mmio_register::RW, 0x10100, 1, 32) {}
					} gicr_isenabler0;

					struct Gicr_icenabler0 : Irq_reg
					{
						Register read(Irq & irq)              { return irq.enabled(); }
						void     write(Irq & irq, Register v) { if (v) irq.disable();  }

						Gicr_icenabler0()
						: Irq_reg("GICR_ICENABLER0", Mmio_register::RW, 0x10180, 1, 32) {}
					} gicr_icenabler0;

					struct Gicr_ispendr0 : Irq_reg
					{
						Register read(Irq & irq)              { return irq.pending(); }
						void     write(Irq & irq, Register v) { if (v) irq.assert();  }

						Gicr_ispendr0()
						: Irq_reg("GICR_ISPENDR0", Mmio_register::RW, 0x10200, 1, 32) {}
					} gicr_ispendr0;

					struct Gicr_icpendr0 : Irq_reg
					{
						Register read(Irq & irq)              { return irq.pending();  }
						void     write(Irq & irq, Register v) { if (v) irq.deassert(); }

						Gicr_icpendr0()
						: Irq_reg("GICR_ICPENDR0", Mmio_register::RW, 0x10280, 1, 32) {}
					} gicr_icpendr0;

					struct Gicr_isactiver0 : Irq_reg
					{
						Register read(Irq & irq)              { return irq.active();   }
						void     write(Irq & irq, Register v) { if (v) irq.activate(); }

						Gicr_isactiver0()
						: Irq_reg("GICR_ISACTIVER0", Mmio_register::RW, 0x10300, 1, 32) {}
					} gicr_isactiver0;

					struct Gicr_icactiver0 : Irq_reg
					{
						Register read(Irq & irq)              { return irq.active();     }
						void     write(Irq & irq, Register v) { if (v) irq.deactivate(); }

						Gicr_icactiver0()
						: Irq_reg("GICR_ICACTIVER0", Mmio_register::RW, 0x10380, 1, 32) {}
					} gicr_icactiver0;

					struct Gicr_ipriorityr : Irq_reg
					{
						Register read(Irq & irq)              { return irq.priority(); }
						void     write(Irq & irq, Register v) { irq.priority(v);       }

						Gicr_ipriorityr()
						: Irq_reg("GICR_IPRIORITYR", Mmio_register::RW, 0x10400, 8, 32) {}
					} gicr_ipriorityr;

					struct Gicr_icfgr : Irq_reg
					{
						Register read(Irq & irq)              { return irq.level() ? 0 : 1; }
						void     write(Irq & irq, Register v) { irq.level(!v);              }

						Gicr_icfgr()
						: Irq_reg("GICR_ICFGR", Mmio_register::RW, 0x10c00, 8, 32) {}
					} gicr_icfgr;

					Redistributor(const Genode::uint64_t addr,
					              const Genode::uint64_t size,
					              unsigned               cpu_id,
					              bool                   last)
					: Mmio_device("GICR", addr, size), cpu_id(cpu_id), last(last)
					{
						add(gicr_ctlr);
						add(gicr_typer);
						add(gicr_waker);
						add(gicr_pidr2);
						add(gicr_igroupr0);
						add(gicr_isenabler0);
						add(gicr_icenabler0);
						add(gicr_ispendr0);
						add(gicr_icpendr0);
						add(gicr_isactiver0);
						add(gicr_icactiver0);
						add(gicr_ipriorityr);
						add(gicr_icfgr);
					}
				};

				Genode::Constructible<Redistributor> _rdist;
		};

		unsigned version();

		Gic(const char * const       name,
		    const Genode::uint64_t   addr,
		    const Genode::uint64_t   size,
		    unsigned                 cpus,
		    unsigned                 version,
		    Genode::Vm_connection  & vm,
		    Mmio_bus               & bus,
		    Genode::Env            & env);

	private:

		friend struct Gicd_banked;

		Genode::Constructible<Irq> _spi[MAX_SPI];
		Irq::List                  _pending_list;
		unsigned                   _cpu_cnt;
		unsigned                   _version;

		struct Gicd_ctlr : Genode::Register<32>, Mmio_register
		{
			struct Enable  : Bitfield<0, 1> {};
			struct Disable : Bitfield<6, 1> {};

			void write(Address_range & access, Cpu & cpu,
			           Mmio_register::Register value) override
			{
				access_t v = value;
				Disable::set(v, 0);
				Mmio_register::write(access, cpu, v);
			}

			Gicd_ctlr()
			: Mmio_register("GICD_CTLR", Mmio_register::RW, 0, 4, 0) {}
		} _ctrl;

		struct Gicd_typer : Genode::Register<32>, Mmio_register
		{
			struct It_lines_number : Bitfield<0,  5> {};
			struct Cpu_number      : Bitfield<5,  3> {};
			struct Id_bits         : Bitfield<19, 5> {};

			Gicd_typer(unsigned cpus)
			:  Mmio_register("GICD_TYPER", Mmio_register::RO, 0x4, 4,
			                 It_lines_number::bits(31) |
			                 Cpu_number::bits(cpus-1) | Id_bits::bits(9)) {}
		} _typer { _cpu_cnt };

		struct Gicd_iidr : Genode::Register<32>, Mmio_register
		{
			struct Implementer : Bitfield<0, 12> {};
			struct Revision    : Bitfield<12, 4> {};
			struct Variant     : Bitfield<16, 4> {};
			struct Product_id  : Bitfield<24, 8> {};

			Gicd_iidr()
			: Mmio_register("GICD_IIDR", Mmio_register::RO, 0x8, 4, 0x123) {}
		} _iidr;

		struct Gicd_igroupr : Irq_reg
		{
			Gicd_igroupr()
			: Irq_reg("GICD_IGROUPR", Mmio_register::RW, 0x80, 1, 1024) {}
		} _igroupr;

		struct Gicd_isenabler : Irq_reg
		{
			Register read(Irq & irq)              { return irq.enabled(); }
			void     write(Irq & irq, Register v) { if (v) irq.enable();  }

			Gicd_isenabler()
			: Irq_reg("GICD_ISENABLER", Mmio_register::RW, 0x100, 1, 1024) {}
		} _isenabler;

		struct Gicd_icenabler : Irq_reg
		{
			Register read(Irq & irq)              { return irq.enabled(); }
			void     write(Irq & irq, Register v) { if (v) irq.disable(); }

			Gicd_icenabler()
			: Irq_reg("GICD_ICENABLER", Mmio_register::RW, 0x180, 1, 1024) {}
		} _csenabler;

		struct Gicd_ispendr : Irq_reg
		{
			Register read(Irq & irq)              { return irq.pending(); }
			void     write(Irq & irq, Register v) { if (v) irq.assert();  }

			Gicd_ispendr()
			: Irq_reg("GICD_ISPENDR", Mmio_register::RW, 0x200, 1, 1024) {}
		} _ispendr;

		struct Gicd_icpendr : Irq_reg
		{
			Register read(Irq & irq)              { return irq.pending();  }
			void     write(Irq & irq, Register v) { if (v) irq.deassert(); }

			Gicd_icpendr()
			: Irq_reg("GICD_ICPENDR", Mmio_register::RW, 0x280, 1, 1024) {}
		} _icpendr;

		struct Gicd_isactiver : Irq_reg
		{
			Register read(Irq & irq)              { return irq.active();   }
			void     write(Irq & irq, Register v) { if (v) irq.activate(); }

			Gicd_isactiver()
			: Irq_reg("GICD_ISACTIVER", Mmio_register::RW, 0x300, 1, 1024) {}
		} _isactiver;

		struct Gicd_icactiver : Irq_reg
		{
			Register read(Irq & irq)              { return irq.active();     }
			void     write(Irq & irq, Register v) { if (v) irq.deactivate(); }

			Gicd_icactiver()
			: Irq_reg("GICD_ICACTIVER", Mmio_register::RW, 0x380, 1, 1024) {}
		} _icactiver;

		struct Gicd_ipriorityr : Irq_reg
		{
			Register read(Irq & irq)              { return irq.priority(); }
			void     write(Irq & irq, Register v) { irq.priority(v);       }

			Gicd_ipriorityr()
			: Irq_reg("GICD_IPRIORITYR", Mmio_register::RW, 0x400, 8, 1024) {}
		} _ipriorityr;

		struct Gicd_itargetr : Irq_reg
		{
			Register read(Irq & irq)              { return irq.target(); }
			void     write(Irq & irq, Register v) { irq.target(v);       }

			Gicd_itargetr()
			: Irq_reg("GICD_ITARGETSR", Mmio_register::RW, 0x800, 8, 1024) {}
		} _itargetr;


		struct Gicd_icfgr : Irq_reg
		{
			Register read(Irq & irq)              { return irq.level() ? 0 : 2; }
			void     write(Irq & irq, Register v) { irq.level(!v);              }

			Gicd_icfgr()
			: Irq_reg("GICD_ICFGR", Mmio_register::RW, 0xc00, 2, 1024) {}
		} _icfgr;

		struct Gicd_sgir : Genode::Register<32>, Mmio_register
		{
			struct Enable  : Bitfield<0, 1> {};
			struct Disable : Bitfield<6, 1> {};

			void write(Address_range & access, Cpu & cpu,
			           Mmio_register::Register value) override
			{
				Genode::error("SGIR WRITE ", value);
			}

			Gicd_sgir()
			: Mmio_register("GICD_SGIR", Mmio_register::WO, 0xf00, 4, 0) {}
		} _sgir;


		struct Gicd_irouter : Irq_reg
		{
			Register read(Irq &)            { return 0x0; } // FIXME smp
			void     write(Irq &, Register) { }

			Gicd_irouter()
			: Irq_reg("GICD_IROUTER", Mmio_register::RW, 0x6100, 64, 1024) {}
		} _irouter;

		/**
		 * FIXME: missing registers:
		 * GICD_IGROUP      = 0x80,
		 * GICD_NSACR       = 0xe00,
		 * GICD_SGIR        = 0xf00,
		 * GICD_CPENDSGIR0  = 0xf10,
		 * GICD_SPENDSGIR0  = 0xf20,
         * GICD identification registers 0xfd0...
		 */

		/**
		 * Dummy container for holding array of noncopyable objects
		 * Workaround for gcc bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70395
		 */
		struct Dummy {
			Mmio_register regs[8];
		} _reg_container { .regs = {
			{ "GICD_PIDR4",    Mmio_register::RO, 0xffd0, 4, 0x0                 },
			{ "GICD_PIDR5",    Mmio_register::RO, 0xffd4, 4, 0x0                 },
			{ "GICD_PIDR6",    Mmio_register::RO, 0xffd8, 4, 0x0                 },
			{ "GICD_PIDR7",    Mmio_register::RO, 0xffdc, 4, 0x0                 },
			{ "GICD_PIDR0",    Mmio_register::RO, 0xffe0, 4, 0x492               },
			{ "GICD_PIDR1",    Mmio_register::RO, 0xffe4, 4, 0xb0                },
			{ "GICD_PIDR2",    Mmio_register::RO, 0xffe8, 4, (_version<<4) | 0xb },
			{ "GICD_PIDR3",    Mmio_register::RO, 0xffec, 4, 0x44                }
		}};
};

#endif /* _SRC__SERVER__VMM__GIC_H_ */
