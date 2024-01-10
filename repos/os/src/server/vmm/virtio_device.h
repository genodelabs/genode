/*
 * \brief  Generic and simple virtio device
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2019-10-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIRTIO_DEVICE_H_
#define _VIRTIO_DEVICE_H_

#include <cpu/memory_barrier.h>
#include <util/mmio.h>
#include <util/reconstructible.h>

#include <cpu.h>
#include <gic.h>
#include <ram.h>
#include <mmio.h>

namespace Vmm {
	class  Virtio_split_queue;
	struct Virtio_device_base;
	template <typename QUEUE, unsigned NUM> class Virtio_device;

	using Virtio_device_list = List<Virtio_device_base>;

	using namespace Genode;
}


/**
 * Split queue implementation
 */
class Vmm::Virtio_split_queue
{
	public:

		static constexpr unsigned MAX_SIZE_LOG2 = 9;
		static constexpr unsigned MAX_SIZE      = 1 << MAX_SIZE_LOG2;

	protected:

		template <uint16_t LOG2>
		class Index
		{
			private:

				uint16_t _idx;

				static_assert((sizeof(uint16_t)*8) >= LOG2);

			public:

				Index(uint16_t idx = 0) : _idx(idx % (1 << LOG2)) {}

				void inc() {
					_idx = ((_idx + 1U) % (1U << LOG2)); }

				uint16_t idx() const { return _idx; }

				bool operator != (Index const & o) const {
					return _idx != o._idx; }
		};

		using Ring_index       = Index<16>;
		using Descriptor_index = Index<MAX_SIZE_LOG2>;


		template <size_t SIZE>
		struct Queue_base : Mmio<SIZE>
		{
			using Base = Mmio<SIZE>;

			uint16_t const max;

			Queue_base(Byte_range_ptr const &range, uint16_t max)
			: Base(range), max(max) {}

			struct Flags : Base::template Register<0x0, 16> { };
			struct Idx   : Base::template Register<0x2, 16> { };

			Ring_index current() { return Base::template read<Idx>(); }
		};


		struct Avail_queue : Queue_base<0x4 + MAX_SIZE * 2>
		{
			using Queue_base::Queue_base;

			struct Ring : Register_array<0x4, 16, MAX_SIZE, 16> { };

			bool inject_irq() { return (read<Flags>() & 1) == 0; }

			Descriptor_index get(Ring_index id)
			{
				uint16_t v = read<Ring>(id.idx() % max);
				if (v >= max) {
					throw Exception("Descriptor_index out of bounds"); }
				return Descriptor_index(v);
			}
		} _avail;


		struct Used_queue : Queue_base<0x4 + MAX_SIZE * 8>
		{
			using Queue_base::Queue_base;

			struct Ring : Register_array<0x4, 64, MAX_SIZE, 64>
			{
				struct Id     : Bitfield<0, 32> { };
				struct Length : Bitfield<32,32> { };
			};

			void add(Ring_index ri, Descriptor_index di, size_t size)
			{
				if (di.idx() >= max) {
					throw Exception("Descriptor_index out of bounds"); }

				write<Flags>(0);
				Ring::access_t elem = 0;
				Ring::Id::set(elem,  di.idx());
				Ring::Length::set(elem, size);
				write<Ring>(elem, ri.idx() % max);
			}
		} _used;


		struct Descriptor : Mmio<0x10>
		{
			using Mmio::Mmio;

			struct Address : Register<0x0, 64> { };
			struct Length  : Register<0x8, 32> { };

			struct Flags : Register<0xc, 16>
			{
				struct Next     : Bitfield<0, 1> { };
				struct Write    : Bitfield<1, 1> { };
				struct Indirect : Bitfield<2, 1> { };
			};

			struct Next  : Register<0xe, 16> { };

			uint64_t address() const { return read<Address>(); }
			size_t   length () const { return read<Length>();  }
			uint16_t flags()   const { return read<Flags>();   }
			uint16_t next()    const { return read<Next>();    }
		};


		struct Descriptor_array
		{
			size_t         const elem_size { 16 };
			unsigned       const max;
			Byte_range_ptr const guest_range;
			Byte_range_ptr const local_range;

			Descriptor_array(Ram & ram, addr_t base, unsigned const max)
			:
				max(max),
				guest_range((char *)base, max * elem_size),
				local_range(
					ram.to_local_range(guest_range).start,
					ram.to_local_range(guest_range).num_bytes) {}

			Descriptor get(Descriptor_index idx)
			{
				if (idx.idx() >= max) error("Descriptor_index out of bounds");
				off_t offset = elem_size * idx.idx();
				return Descriptor({local_range.start + offset, local_range.num_bytes - offset});
			}
		} _descriptors;


		Ram      & _ram;
		Ring_index _cur_idx {};

	public:

		Virtio_split_queue(addr_t   const descriptor_area,
		                   addr_t   const device_area,
		                   addr_t   const driver_area,
		                   uint16_t const queue_num,
		                   Ram          & ram)
		:
			_avail(ram.to_local_range({(char *)driver_area, 6+2*(size_t)queue_num}), queue_num),
			_used(ram.to_local_range({(char *)device_area, 6+8*(size_t)queue_num}), queue_num),
			_descriptors(ram, descriptor_area, queue_num),
			_ram(ram) { }

		template <typename FUNC>
		bool notify(FUNC func)
		{
			memory_barrier();

			bool written = false;

			for (Ring_index avail_idx = _avail.current();
			     _cur_idx != avail_idx; _cur_idx.inc()) {

				Descriptor_index  id         = _avail.get(_cur_idx);
				Descriptor descriptor = _descriptors.get(id);
				uint64_t          address    = descriptor.address();
				size_t            size       = descriptor.length();

				if (!address || !size) { break; }

				try {
					size_t consumed = func(_ram.to_local_range({(char *)address, size}));
					if (!consumed) { break; }
					_used.add(_cur_idx, id, consumed);
					written = true;
				} catch (...) { break; }
			}

			if (written) {
				_used.write<Used_queue::Idx>(_cur_idx.idx());
				memory_barrier();
			}

			return written && _avail.inject_irq();
		}
};


struct Vmm::Virtio_device_base : public List<Virtio_device_base>::Element { };


template <typename QUEUE, unsigned NUM>
class Vmm::Virtio_device : public Vmm::Mmio_device, private Virtio_device_base
{
	protected:

		Gic::Irq                   & _irq;
		Ram                        & _ram;
		Genode::Mutex                _mutex {};
		Genode::Constructible<QUEUE> _queue[NUM] {};

		virtual void _notify(unsigned idx) = 0;


		/***************
		 ** Registers **
		 ***************/

		class Reg : public Mmio_register
		{
			private:

				Virtio_device & _dev;

			public:

				Reg(Virtio_device     & dev,
				    Mmio_register::Name name,
				    Mmio_register::Type type,
				    Genode::uint64_t    start,
				    uint32_t            value = 0)
				:
					Mmio_register(name, type, start, 4,
					              dev.registers(), value),
					_dev(dev) { }

				Virtio_device & device() { return _dev; }
		};

		class Set : public Reg
		{
			private:

				Reg                  & _selector;
				typename Reg::Register _regs[32] { 0 };

			public:

				Set(Virtio_device & device,
				    Reg           & selector,
				    Mmio_register::Name name,
				    Mmio_register::Type type,
				    Genode::uint64_t    start)
				: Reg(device, name, type, start),
				  _selector(selector) {}

				Register read(Address_range &, Cpu &) override {
					return _regs[_selector.value()]; }

				void write(Address_range &, Cpu &, Register reg) override {
					_regs[_selector.value()] = reg; }

				void set(Register value) override {
					_regs[_selector.value()] = value; }

				Register value() const override {
					return _regs[_selector.value()]; }
		};


		Reg _magic     { *this, "MagicValue", Reg::RO, 0x0, 0x74726976 };
		Reg _version   { *this, "Version",    Reg::RO, 0x4, 0x2        };
		Reg _dev_id    { *this, "DeviceID",   Reg::RO, 0x8             };
		Reg _vendor_id { *this, "VendorID",   Reg::RO, 0xc, 0x554d4551/*QEMU*/};

		Reg _dev_sel   { *this, "DeviceFeatureSel", Reg::WO, 0x14 };
		Reg _drv_sel   { *this, "DriverFeatureSel", Reg::WO, 0x24 };
		Reg _queue_sel { *this, "QueueSel",         Reg::WO, 0x30 };

		Set _dev_feature   { *this, _dev_sel, "DeviceFeatures", Reg::RO, 0x10 };
		Set _drv_features  { *this, _drv_sel, "DriverFeatures", Reg::WO, 0x20 };
		Reg _queue_num_max { *this, "QueueNumMax", Reg::RO, 0x34, QUEUE::MAX_SIZE };
		Set _queue_num     { *this, _queue_sel, "QueueNum", Reg::WO, 0x38 };
		Reg _irq_status    { *this, "InterruptStatus",  Reg::RO, 0x60 };
		Reg _status        { *this, "Status",           Reg::RW, 0x70 };

		Set _descr_low   { *this, _queue_sel, "QueueDescrLow",   Reg::WO, 0x80 };
		Set _descr_high  { *this, _queue_sel, "QueueDescrHigh",  Reg::WO, 0x84 };
		Set _driver_low  { *this, _queue_sel, "QueueDriverLow",  Reg::WO, 0x90 };
		Set _driver_high { *this, _queue_sel, "QueueDriverHigh", Reg::WO, 0x94 };
		Set _device_low  { *this, _queue_sel, "QueueDeviceLow",  Reg::WO, 0xa0 };
		Set _device_high { *this, _queue_sel, "QueueDeviceHigh", Reg::WO, 0xa4 };

		Reg _shm_id        { *this, "SHMSel",           Reg::WO, 0xac };
		Reg _shm_len_low   { *this, "SHMLenLow",        Reg::RO, 0xb0, 0xffffffff };
		Reg _shm_len_high  { *this, "SHMLenHigh",       Reg::RO, 0xb4, 0xffffffff };
		Reg _shm_base_low  { *this, "SHMBaseLow",       Reg::RO, 0xb8, 0xffffffff };
		Reg _shm_base_high { *this, "SHMBaseHigh",      Reg::RO, 0xbc, 0xffffffff };
		Reg _config_gen    { *this, "ConfigGeneration", Reg::RW, 0xfc, 0 };


		uint64_t _descriptor_area() const
		{
			return ((uint64_t)_descr_high.value()<<32) | _descr_low.value();
		}

		uint64_t _driver_area() const
		{
			return ((uint64_t)_driver_high.value()<<32) | _driver_low.value();
		}

		uint64_t _device_area() const
		{
			return ((uint64_t)_device_high.value()<<32) | _device_low.value();
		}

		enum Irq : uint64_t {
			NONE   = 0UL,
			BUFFER = 1UL << 0,
			CONFIG = 1UL << 1,
		};

		void _assert_irq(uint64_t irq)
		{
			_irq_status.set(_irq_status.value() | irq);
			_irq.assert();
		}

		void _deassert_irq(uint64_t irq)
		{
			_irq_status.set(_irq_status.value() & ~irq);
			_irq.deassert();
		}

		void _buffer_notification()
		{
			_assert_irq(BUFFER);
		}

		void _config_notification()
		{
			_config_gen.set(_config_gen.value() + 1);
			_assert_irq(CONFIG);
		}

		void _construct_queue()
		{
			Genode::Mutex::Guard guard(mutex());

			unsigned num = (unsigned)_queue_sel.value();

			if (num >= NUM || _queue[num].constructed())
				return;

			_queue[num].construct((addr_t)_descriptor_area(), (addr_t)_device_area(),
			                      (addr_t)_driver_area(), (uint16_t)_queue_num.value(),
			                      _ram);
		}

		struct Queue_ready : Reg
		{
			void write(Address_range&, Cpu&, Register reg) override {
				if (reg == 1) { Reg::device()._construct_queue(); } }

			Queue_ready(Virtio_device & device)
			: Reg(device, "QueueReady", Reg::RW, 0x44) {}
		} _queue_ready { *this };


		struct Queue_notify : Reg
		{
			void write(Address_range&, Cpu&, Register reg) override
			{
				Genode::Mutex::Guard guard(Reg::device().mutex());

				if (reg >= NUM) {
					error("Number of queues not supported by device!");
					return;
				}

				if (!Reg::device()._queue[reg].constructed()) {
					error("Queue is not constructed and cannot be notified!");
					return;
				}

				Reg::device()._notify((unsigned)reg);
			}

			Queue_notify(Virtio_device & device)
			: Reg(device, "QueueNotify", Mmio_register::WO, 0x50) { }
		} _queue_notify { *this };


		struct Interrupt_ack : Reg
		{
			void write(Address_range&, Cpu&, Register v) override
			{
				Genode::Mutex::Guard guard(Reg::device().mutex());
				Reg::device()._deassert_irq(v);
			}

			Interrupt_ack(Virtio_device &device)
			: Reg(device, "InterruptAck", Mmio_register::WO, 0x64) {}
		} _interrupt_ack { *this };

	public:

		Virtio_device(const char * const     name,
		              const Genode::uint64_t addr,
		              const Genode::uint64_t size,
		              unsigned               irq,
		              Cpu                  & cpu,
		              Space                & bus,
		              Ram                  & ram,
		              Virtio_device_list   & dev_list,
		              uint32_t               dev_id)
		:
			Mmio_device(name, addr, size, bus),
			_irq(cpu.gic().irq(irq)),
			_ram(ram)
		{
			/* set device id from specific, derived device */
			_dev_id.set(dev_id);

			/* prepare device features */
			enum { VIRTIO_F_VERSION_1 = 1 };
			_dev_sel.set(1); /* set to 32...63 feature bits */
			_dev_feature.set(VIRTIO_F_VERSION_1);
			_dev_sel.set(0); /* set to 0...31 feature bits */

			dev_list.insert(this);
		}

		Genode::Mutex & mutex() { return _mutex; }
};

#endif /* _VIRTIO_DEVICE_H_ */
