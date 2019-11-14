/*
 * \brief  Generic and simple virtio device
 * \author Sebastian Sumpf
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

#include <util/mmio.h>
#include <util/reconstructible.h>

#include <gic.h>
#include <ram.h>
#include <mmio.h>

namespace Vmm {
	class  Virtio_device;
	class  Virtio_queue;
	struct Virtio_queue_data;
	class  Virtio_descriptor;
	class  Virtio_avail;
	class  Virtio_used;
}

using uint16_t = Genode::uint16_t;
using uint32_t = Genode::uint32_t;
using uint64_t = Genode::uint64_t;
using addr_t   = Genode::addr_t;
using size_t   = Genode::size_t;

struct Vmm::Virtio_queue_data
{
	uint32_t descr_low     { 0 };
	uint32_t descr_high    { 0 };
	uint32_t driver_low    { 0 };
	uint32_t driver_high   { 0 };
	uint32_t device_low    { 0 };
	uint32_t device_high   { 0 };
	uint32_t num           { 0 };
	uint32_t ready         { 0 };
	bool     tx            { false };

	uint64_t descr()  const {
		return ((uint64_t)descr_high  << 32) | descr_low; }
	uint64_t driver() const {
		return ((uint64_t)driver_high << 32) | driver_low; }
	uint64_t device() const {
		return ((uint64_t)device_high << 32) | device_low; }

	enum { MAX_QUEUE_SIZE = 1 << 15 };
};


class Vmm::Virtio_descriptor : Genode::Mmio
{
	public:

		Virtio_descriptor(addr_t base)
		: Mmio(base) {  }


		struct Address : Register<0x0, 64> { };
		struct Length  : Register<0x8, 32> { };

		struct Flags : Register<0xc, 16>
		{
			struct Next     : Bitfield<0, 1> { };
			struct Write    : Bitfield<1, 1> { };
			struct Indirect : Bitfield<2, 1> { };
		};

		struct Next  : Register<0xe, 16> { };

		static constexpr size_t size() { return 16; }

		Virtio_descriptor index(unsigned idx)
		{
			return Virtio_descriptor(base() + (size() * idx));
		}

		uint64_t address() const { return read<Address>(); }
		size_t   length () const { return read<Length>(); }
		uint16_t flags()   const { return read<Flags>(); }
		uint16_t next()    const { return read<Next>(); }
};


class Vmm::Virtio_avail : public Genode::Mmio
{
	public:

		Virtio_avail(addr_t base)
		: Mmio(base) { };

		struct Flags : Register<0x0, 16> { };
		struct Idx   : Register<0x2, 16> { };
		struct Ring  : Register_array<0x4, 16, Virtio_queue_data::MAX_QUEUE_SIZE, 16> { };

		bool inject_irq() { return (read<Flags>() & 1) == 0; }
};


class Vmm::Virtio_used : public Genode::Mmio
{
	public:

		Virtio_used(addr_t base)
		: Mmio(base) { };

		struct Flags : Register<0x0, 16> { };
		struct Idx   : Register<0x2, 16> { };

		struct Elem : Register_array<0x4, 64, Virtio_queue_data::MAX_QUEUE_SIZE, 64>
		{
			struct Id     : Bitfield<0, 32> { };
			struct Length : Bitfield<32,32> { };
		};
};

/** 
 * Split queue implementation
 */
class Vmm::Virtio_queue
{
	private:

		Virtio_queue_data &_data;
		Ram               &_ram;

		Virtio_descriptor _descr { _ram.local_address(_data.descr(),
			Virtio_descriptor::size() * _data.num ) };
		Virtio_avail      _avail { _ram.local_address(_data.driver(), 6 + 2 * _data.num) };
		Virtio_used       _used  { _ram.local_address(_data.device(), 6 + 8 * _data.num) };

		uint16_t   _idx     { 0 };
		uint32_t   _length  { _data.num };
		bool       _tx      { _data.tx  };
		bool const _verbose { false };

	public:

		Virtio_queue(Virtio_queue_data &data, Ram &ram)
		: _data(data), _ram(ram) { }


	bool verbose() const { return _verbose; }

	template <typename FUNC>
	bool notify(FUNC func)
	{
		asm volatile ("dmb ish" : : : "memory");
		uint16_t used_idx  = _used.read<Virtio_used::Idx>();
		uint16_t avail_idx = _avail.read<Virtio_avail::Idx>();

		uint16_t written = 0;

		if (_verbose)
			Genode::log(_length > 64 ? "net $ " : "console $ ",
			            "[", _tx ? "tx] " : "rx] ",
			            "idx: ", _idx, " avail_idx: ", avail_idx, " used_idx: ", used_idx,
			             " queue length: ", _length, " avail flags: ", _avail.read<Virtio_avail::Flags>());

		while (_idx != avail_idx && written < _length)  {

			uint16_t id = _avail.read<Virtio_avail::Ring>(_idx % _length);

			/* make sure id stays in ring */
			id %= _length;

			Virtio_descriptor descr = _descr.index(id);
			uint64_t        address = descr.address();
			size_t           length = descr.length();
			if (!address || !length) break;

			addr_t data = 0;
			try {
				data   = _ram.local_address(address, length);
				length = func(data, length);
			} catch (...) { break; }

			if (length == 0) break;

			_used.write<Virtio_used::Flags>(0);
			Virtio_used::Elem::access_t elem = 0;
			Virtio_used::Elem::Id::set(elem,  id);
			Virtio_used::Elem::Length::set(elem, length);
			_used.write<Virtio_used::Elem>(elem, _idx % _length);
			written++; _idx++;

			if (used_idx + written == avail_idx)  break;
		}

		if (written) {
			used_idx += written;
			_used.write<Virtio_used::Idx>(used_idx);
			asm volatile ("dmb ish" : : : "memory");
		}

		if (written && _verbose)
			Genode::log(_length > 64 ? "net $ " : "console $ ",
			            "[", _tx ? "tx] " : "rx] ", "updated used_idx: ", used_idx,
			            " written: ", written);

		return written > 0 && _avail.inject_irq();
	}
};


class Vmm::Virtio_device : public Vmm::Mmio_device
{
	protected:

		enum { RX = 0, TX = 1, NUM = 2 };
		Virtio_queue_data _data[NUM];
		uint32_t          _current { RX };

		Genode::Constructible<Virtio_queue> _queue[NUM];
		Gic::Irq                           &_irq;
		Ram                                &_ram;

		struct Dummy {
			Mmio_register regs[7];
		} _reg_container { .regs = {
			{ "MagicValue", Mmio_register::RO, 0x0, 4, 0x74726976            },
			{ "Version",    Mmio_register::RO, 0x4, 4, 0x2                   },
			{ "DeviceID",   Mmio_register::RO, 0x8, 4, 0x0                   },
			{ "VendorID",   Mmio_register::RO, 0xc, 4, 0x554d4551 /* QEMU */ },
			{ "DeviceFeatureSel", Mmio_register::RW, 0x14, 4, 0 },
			{ "DriverFeatureSel", Mmio_register::RW, 0x24, 4, 0 },
			{ "QueueNumMax", Mmio_register::RO, 0x34, 4, 8 }
		}};


		void _device_id(unsigned id)
		{
			_reg_container.regs[2].set(id);
		}

		void               _queue_select(uint32_t sel) { _current = sel; }
		Virtio_queue_data &_queue_data() { return _data[_current]; }

		void _queue_state(bool const construct)
		{
			if (construct && !_queue[_current].constructed() && _queue_data().num > 0) {
				_queue_data().tx = (_current == TX);
				_queue[_current].construct(_queue_data(), _ram);
			}

			if (!construct && _queue[_current].constructed())
				_queue[_current].destruct();
		}

		void _assert_irq()
		{
			_interrupt_status.set(0x1);
			_irq.assert();
		}

		void _deassert_irq()
		{
			_interrupt_status.set(0);
			_irq.deassert();
		}

		virtual void _notify(unsigned idx) = 0;
		virtual Register _device_specific_features() = 0;

		/***************
		 ** Registers **
		 ***************/

		struct DeviceFeatures : Mmio_register
		{
			enum {
				VIRTIO_F_VERSION_1 = 1,
			};

			Virtio_device &_device;
			Mmio_register &_selector;

			Register read(Address_range&,  Cpu&) override
			{
				/* lower 32 bit */
				if (_selector.value() == 0) return _device._device_specific_features();

				/* upper 32 bit */
				return VIRTIO_F_VERSION_1;
			}

			DeviceFeatures(Virtio_device &device, Mmio_register &selector)
			: Mmio_register("DeviceFeatures", Mmio_register::RO, 0x10, 4),
			  _device(device), _selector(selector)
			{ }
		} _device_features { *this, _reg_container.regs[4] };

		struct DriverFeatures : Mmio_register
		{
			Mmio_register    &_selector;
			uint32_t  _lower { 0 };
			uint32_t  _upper { 0 };

			void write(Address_range&, Cpu&, Register reg) override
			{
				if (_selector.value() == 0) {
					_lower = reg;
				}
				_upper = reg;
			}

			DriverFeatures(Mmio_register &selector)
			: Mmio_register("DriverFeatures", Mmio_register::WO, 0x20, 4),
			  _selector(selector)
			{ }
		} _driver_features { _reg_container.regs[5] };

		struct Status : Mmio_register
		{
			Register read(Address_range&,  Cpu&) override
			{
				return value();
			}

			void write(Address_range&, Cpu&, Register reg) override
			{
				set(reg);
			}

			Status()
			: Mmio_register("Status", Mmio_register::RW, 0x70, 4, 0)
			{ }
		} _status;

		struct QueueSel : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				if (reg >= device.NUM) return;
				device._queue_select(reg);
			}

			QueueSel(Virtio_device &device)
			: Mmio_register("QueueSel", Mmio_register::WO, 0x30, 4),
			  device(device) { }
		} _queue_sel { *this };

		struct QueueNum : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().num = Genode::min(reg,
					device._reg_container.regs[6].value());
			}

			QueueNum(Virtio_device &device)
			: Mmio_register("QueueNum", Mmio_register::WO, 0x38, 4),
			  device(device) { }
		} _queue_num { *this };

		struct QueueReady : Mmio_register
		{
			Virtio_device &device;

			Register read(Address_range&,  Cpu&) override
			{
				return device._queue_data().ready; 
			}

			void write(Address_range&, Cpu&, Register reg) override
			{
				bool construct = reg == 1 ? true : false;
				device._queue_data().ready = reg;
				device._queue_state(construct);
			}

			QueueReady(Virtio_device &device)
			: Mmio_register("QueueReady", Mmio_register::RW, 0x44, 4),
			  device(device) { }
		} _queue_ready { *this };

		struct QueueNotify : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				if (!device._queue[reg].constructed()) return;

				device._notify(reg);
			}

			QueueNotify(Virtio_device &device)
			: Mmio_register("QueueNotify", Mmio_register::WO, 0x50, 4),
			  device(device) { }
		} _queue_notify { *this };

		struct QueueDescrLow : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().descr_low = reg;
			}

			QueueDescrLow(Virtio_device &device)
			: Mmio_register("QueuDescrLow", Mmio_register::WO, 0x80, 4),
			  device(device) { }
		} _queue_descr_low { *this };

		struct QueueDescrHigh : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().descr_high = reg;
			}

			QueueDescrHigh(Virtio_device &device)
			: Mmio_register("QueuDescrHigh", Mmio_register::WO, 0x84, 4),
			  device(device) { }
		} _queue_descr_high { *this };

		struct QueueDriverLow : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().driver_low = reg;
			}

			QueueDriverLow(Virtio_device &device)
			: Mmio_register("QueuDriverLow", Mmio_register::WO, 0x90, 4),
			  device(device) { }
		} _queue_driver_low { *this };

		struct QueueDriverHigh : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().driver_high = reg;
			}

			QueueDriverHigh(Virtio_device &device)
			: Mmio_register("QueuDriverHigh", Mmio_register::WO, 0x94, 4),
			  device(device) { }
		} _queue_driver_high { *this };

		struct QueueDeviceLow : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().device_low = reg;
			}

			QueueDeviceLow(Virtio_device &device)
			: Mmio_register("QueuDeviceLow", Mmio_register::WO, 0xa0, 4),
			  device(device) { }
		} _queue_device_low { *this };

		struct QueueDeviceHigh : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._queue_data().device_high = reg;
			}

			QueueDeviceHigh(Virtio_device &device)
			: Mmio_register("QueuDeviceHigh", Mmio_register::WO, 0xa4, 4),
			  device(device) { }
		} _queue_device_high { *this };

		struct InterruptStatus : Mmio_register
		{
			Register read(Address_range&,  Cpu&) override
			{
				return value();
			}

			InterruptStatus()
			: Mmio_register("InterruptStatus", Mmio_register::RO, 0x60, 4)
			{ }
		} _interrupt_status;

		struct InterruptAck : Mmio_register
		{
			Virtio_device &device;

			void write(Address_range&, Cpu&, Register reg) override
			{
				device._deassert_irq();
			}

			InterruptAck(Virtio_device &device)
			: Mmio_register("InterruptAck", Mmio_register::WO, 0x64, 4),
			  device(device) { }
		} _interrupt_ack { *this };

	public:

		Virtio_device(const char * const name,
		              const uint64_t addr,
		              const uint64_t size,
		              unsigned irq,
		              Cpu      &cpu,
		              Mmio_bus &bus,
		              Ram      &ram,
		              unsigned queue_size = 8);
};

#endif /* _VIRTIO_DEVICE_H_ */
