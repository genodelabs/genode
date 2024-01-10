/*
 * \brief  Virtio Input implementation
 * \author Stefan Kalkowski
 * \date   2022-12-06
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIRTIO_INPUT_H_
#define _VIRTIO_INPUT_H_

#include <input/event.h>
#include <input_session/client.h>
#include <virtio_device.h>

namespace Vmm {
	class Virtio_input_device;
	using namespace Genode;
}


namespace Linux_evdev
{
	enum Type {
		EV_SYNC = 0x0,
		EV_KEY  = 0x1,
		EV_REL  = 0x2,
		EV_ABS  = 0x3,
		EV_REP  = 0x14
	};

	enum Relative {
		REL_WHEEL       = 8,
		EV_REL_FEATURES = 1U << REL_WHEEL
	};

	enum Absolute {
		ABS_X           = 0,
		ABS_Y           = 1,
		EV_ABS_FEATURES = (1U << ABS_X) | (1U << ABS_Y)
	};
};


class Vmm::Virtio_input_device : public Virtio_device<Virtio_split_queue, 2>
{
	private:

		Env                      & _env;
		Heap                     & _heap;
		Input::Session_client    & _input;
		Attached_dataspace         _input_ds { _env.rm(), _input.dataspace() };
		Input::Event const * const _events   {
			_input_ds.local_addr<Input::Event>() };

		enum State { READY, IN_MOTION, SYNC };

		State    _state      { READY };
		unsigned _num_events { 0U    };
		unsigned _idx_events { 0U    };
		int      _motion_y   { -1    };

		Cpu::Signal_handler<Virtio_input_device> _handler;

		struct Virtio_input_event : Mmio<8>
		{
			struct Type  : Register<0, 16> {};
			struct Code  : Register<2, 16> {};
			struct Value : Register<4, 32> {};

			using Mmio::Mmio;
		};

		struct Configuration_area : Mmio_register
		{
			Virtio_input_device & dev;

			struct Abs_info
			{
				enum { SIZE = 20 };

				uint32_t min;
				uint32_t max;
				uint32_t fuzz;
				uint32_t flat;
			};

			enum Offsets
			{
				SELECT      = 0,
				SUB_SELECT  = 1,
				SIZE        = 2,
				DATA        = 8,
				DATA_MAX    = DATA + 128,
			};

			enum Select {
				UNSET      = 0x00,
				ID_NAME    = 0x01,
				ID_SERIAL  = 0x02,
				ID_DEVIDS  = 0x03,
				PROP_BITS  = 0x10,
				EV_BITS    = 0x11,
				ABS_INFO   = 0x12,
			};

			uint8_t _select     { 0 };
			uint8_t _sub_select { 0 };

			String<16> _name   { "vinput0" };
			String<16> _serial { "serial0" };
			String<16> _dev_id { "0"       };

			uint8_t _size()
			{
				using namespace Linux_evdev;

				switch (_select) {
				case ID_NAME:    return (uint8_t)(_name.length()   - 1);
				case ID_SERIAL:  return (uint8_t)(_serial.length() - 1);
				case ID_DEVIDS:  return (uint8_t)(_dev_id.length() - 1);
				case PROP_BITS:  return 0; /* Unsupported */
				case EV_BITS:
					switch (_sub_select) {
					case EV_KEY: return 36;
					case EV_REL: return 2;
					case EV_ABS: return 1;
					case EV_REP: return 1;
					default:     return 0; /* Unsupported */
					};
				case ABS_INFO:   return 20;
				default:         break;
				};

				error("Unknown size for ", _select, " ", _sub_select);
				return 0;
			}

			Register _data(Register off)
			{
				using namespace Linux_evdev;

				switch (_select) {
				case ID_NAME:
					return (off < (_name.length()-1)) ? _name.string()[off] : 0;
				case ID_SERIAL:
					return (off < (_serial.length()-1)) ? _serial.string()[off]
					                                    : 0;
				case ID_DEVIDS:
					return (off < (_dev_id.length()-1)) ? _dev_id.string()[off]
					                                    : 0;
				case EV_BITS:
					switch (_sub_select) {
					case EV_ABS: return EV_ABS_FEATURES;
					case EV_REL: return EV_REL_FEATURES;
					case EV_KEY: return 0xffffffff;
					default: return 0;
					};
				case ABS_INFO:
					switch (_sub_select) {
					case ABS_X: return (off == 4) ? 1920 : 0;
					case ABS_Y: return (off == 4) ? 1050 : 0;
					default:    return 0;
					};
				default: break;
				};

				error("Invalid data offset for selectors ",
				      _select, " ", _sub_select);
				return 0;
			}

			Register read(Address_range & range,  Cpu&) override
			{
				if (range.start() == SIZE)
					return _size();

				if (range.start() >= DATA && range.start() < DATA_MAX)
					return _data(range.start()-DATA);

				error("Reading from virtio input config space ",
				      "at offset ", range.start(), " is not allowed");
				return 0;
			}

			void write(Address_range & range,  Cpu&, Register v) override
			{
				switch (range.start()) {
				case SELECT:     _select     = (uint8_t)v; return;
				case SUB_SELECT: _sub_select = (uint8_t)v; return;
				default:
					error("Writing to virtio input config space ",
					      "at offset ", range.start(), " is not allowed");
				}
			}

			Configuration_area(Virtio_input_device & device)
			:
				Mmio_register("Input config area", Mmio_register::RO,
				              0x100, 0xa4, device.registers()),
				dev(device) { }
		} _config_area{ *this };

		void _handle_input()
		{
			if (!_queue[0].constructed())
				return;

			bool irq = _queue[0]->notify([&] (Byte_range_ptr const &data) {
				if (data.num_bytes < Virtio_input_event::SIZE) {
					warning("wrong virtioqueue packet size for input ", data.num_bytes);
					return 0UL;
				}

				Virtio_input_event vie(data);

				if (_state == IN_MOTION) {
					vie.write<Virtio_input_event::Type>(Linux_evdev::EV_ABS);
					vie.write<Virtio_input_event::Code>(Linux_evdev::ABS_Y);
					vie.write<Virtio_input_event::Value>(_motion_y);
					_state = SYNC;
					return data.num_bytes;
				}

				if (_state == SYNC) {
					vie.write<Virtio_input_event::Type>(Linux_evdev::EV_SYNC);
					vie.write<Virtio_input_event::Code>(0);
					vie.write<Virtio_input_event::Value>(0);
					_state = READY;
					return data.num_bytes;
				}

				if (_num_events == _idx_events) {
					_num_events = _input.flush();
					_idx_events = 0;
				}

				while (_idx_events < _num_events &&
				       !_events[_idx_events].valid())
					_idx_events++;

				if (_num_events == _idx_events)
					return 0UL;

				Input::Event const event = _events[_idx_events++];

				auto press = [&] (Input::Keycode key, bool press) {
					vie.write<Virtio_input_event::Type>(Linux_evdev::EV_KEY);
					vie.write<Virtio_input_event::Code>(key);
					vie.write<Virtio_input_event::Value>(press);
					_state = SYNC;
				};
				event.handle_press([&] (Input::Keycode key, Genode::Codepoint) {
					press(key, true); });
				event.handle_release([&] (Input::Keycode key) {
					press(key, false); });
				event.handle_absolute_motion([&] (int x, int y)
				{
					vie.write<Virtio_input_event::Type>(Linux_evdev::EV_ABS);
					vie.write<Virtio_input_event::Code>(Linux_evdev::ABS_X);
					vie.write<Virtio_input_event::Value>(x);
					_motion_y = y;
					_state = IN_MOTION;
				});
				return data.num_bytes;
			});

			if (irq) _buffer_notification();
		}

		void _notify(unsigned idx) override
		{
			if (idx) {
				error("VirtIO input queue for status event not implemented");
				return;
			}
			_handle_input();
		}

		enum Device_id { INPUT = 18 };

		/*
		 * Noncopyable
		 */
		Virtio_input_device(Virtio_input_device const &);
		Virtio_input_device &operator = (Virtio_input_device const &);

	public:

		Virtio_input_device(const char * const      name,
		                    const uint64_t          addr,
		                    const uint64_t          size,
		                    unsigned                irq,
		                    Cpu                   & cpu,
		                    Mmio_bus              & bus,
		                    Ram                   & ram,
		                    Virtio_device_list    & list,
		                    Env                   & env,
		                    Heap                  & heap,
		                    Input::Session_client & input)
		:
			Virtio_device<Virtio_split_queue, 2>(name, addr, size, irq,
			                                     cpu, bus, ram, list, INPUT),
			_env(env), _heap(heap), _input(input),
			_handler(cpu, env.ep(), *this, &Virtio_input_device::_handle_input)
		{
			_input.sigh(_handler);
		}
};

#endif /* _VIRTIO_INPUT_H_ */
