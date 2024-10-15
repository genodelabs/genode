/*
 * \brief  Lx_emul backend for accessing GPIO pins
 * \author Norman Feske
 * \date   2021-11-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/registry.h>
#include <pin_control_session/connection.h>
#include <pin_state_session/connection.h>
#include <irq_session/connection.h>

#include <lx_emul/pin.h>
#include <lx_kit/env.h>

namespace {

	using namespace Genode;

	class Global_irq_controller : Lx_kit::Device, Noncopyable
	{
		private:

			Lx_kit::Env &_env;

			int _pending_irq { -1 };

		public:

			struct Number { unsigned value; };

			Global_irq_controller(Lx_kit::Env &env)
			:
				Lx_kit::Device(env.platform, "pin_irq"),
				_env(env)
			{
				_env.devices.insert(this);
			}

			void trigger_irq(Number number)
			{
				_pending_irq = number.value;

				_env.scheduler.unblock_irq_handler();
				_env.scheduler.schedule();
			}

			int pending_irq() override
			{
				int irq      = _pending_irq;
				_pending_irq = -1;

				return irq;
			}
	};

	using Pin_name       = Session_label;
	using Gic_irq_number = Global_irq_controller::Number;

	struct Pin_irq_number { unsigned value; };

	struct Irq_info
	{
		Gic_irq_number gic_irq_number;
		Pin_irq_number pin_irq_number;
	};

	struct Pin_irq_handler : Interface
	{
		virtual void handle_pin_irq(Irq_info) = 0;
	};

	struct Pin : Interface
	{
		using Name = Session_label;

		Env &_env;

		Pin_irq_handler &_pin_irq_handler;

		Irq_info _irq_info { };

		Name const name;

		Constructible<Pin_control::Connection> _control { };
		Constructible<Pin_state::Connection>   _state   { };
		Constructible<Irq_connection>          _irq     { };

		Io_signal_handler<Pin> _irq_handler { _env.ep(), *this, &Pin::_handle_irq };

		enum class Direction { IN, OUT } _direction { Direction::IN };

		void _handle_irq()
		{
			_pin_irq_handler.handle_pin_irq(_irq_info);
		}

		Pin(Env &env, Name const &name, Pin_irq_handler &pin_irq_handler)
		:
			_env(env), _pin_irq_handler(pin_irq_handler), name(name)
		{ }

		void control(bool enabled)
		{
			if (_irq.constructed()) {
				error("attempt to drive interrupt pin ", name, " as output");
				return;
			}

			if (!_control.constructed())
				_control.construct(_env, name.string());

			_control->state(enabled);
			_direction = Direction::OUT;
		}

		bool sense()
		{
			if (_irq.constructed()) {
				error("attempt to drive interrupt pin ", name, " as input");
				return false;
			}

			if (_control.constructed() && (_direction == Direction::OUT)) {
				_control->yield();
				_direction = Direction::IN;
			}

			if (!_state.constructed())
				_state.construct(_env, name.string());

			return _state->state();
		}

		void associate_with_gic_and_unmask_irq(Irq_info irq_info)
		{
			_control.destruct();

			if (!_irq.constructed()) {
				_irq_info = irq_info;
				_irq.construct(_env, _irq_info.pin_irq_number.value);
				_irq->sigh(_irq_handler);
				_irq->ack_irq();
			}
		}

		void ack_matching_irq(Pin_irq_number ack_pin)
		{
			if (ack_pin.value != _irq_info.pin_irq_number.value)
				return;

			if (_irq.constructed())
				_irq->ack_irq();
		}
	};

	struct Pins : private Pin_irq_handler
	{
		Env       &_env;
		Allocator &_alloc;

		Global_irq_controller &_gic;

		Registry<Registered<Pin> > _registry { };

		Pin_irq_number last_irq { };

		Pins(Env &env, Allocator &alloc, Global_irq_controller &gic)
		:
			_env(env), _alloc(alloc), _gic(gic)
		{ }

		template <typename FN>
		void with_pin(Pin::Name const &name, FN const &fn)
		{

			Pin_irq_handler &pin_irq_handler = *this;

			/*
			 * Construct 'Pin' object on demand, apply 'fn' if constructed
			 */
			for (unsigned i = 0; i < 2; i++) {

				bool done = false;
				_registry.for_each([&] (Pin &pin) {
					if (pin.name == name) {
						fn(pin);
						done = true;
					}
				});
				if (done)
					break;

				new (_alloc) Registered<Pin>(_registry, _env, name, pin_irq_handler);

				/* ... apply 'fn' in second iteration */
			}
		}

		void handle_pin_irq(Irq_info irq_info) override
		{
			last_irq = irq_info.pin_irq_number;

			_gic.trigger_irq(irq_info.gic_irq_number);
		}

		void irq_ack(Pin_irq_number ack_pin_number)
		{
			_registry.for_each([&] (Pin &pin) {
				pin.ack_matching_irq(ack_pin_number); });
		}
	};
};


static Pins &pins()
{
	static Global_irq_controller gic { Lx_kit::env() };

	static Pins pins { Lx_kit::env().env, Lx_kit::env().heap, gic };

	return pins;
}


extern "C" void lx_emul_pin_control(char const *pin_name, bool enabled)
{
	pins().with_pin(pin_name, [&] (Pin &pin) {
		pin.control(enabled); });
}


extern "C" int lx_emul_pin_sense(char const *pin_name)
{
	bool result = false;
	pins().with_pin(pin_name, [&] (Pin &pin) {
		result = pin.sense(); });
	return result;
}


extern "C" void lx_emul_pin_irq_unmask(unsigned gic_irq, unsigned pin_irq,
                                       char const *pin_name)
{
	/*
	 * Translate GIC IRQ number as known by the Linux kernel into the
	 * physical IRQ number expected by 'Lx_kit::Env::last_irq'.
	 */
	gic_irq += 32;

	pins().with_pin(pin_name, [&] (Pin &pin) {

		Irq_info const irq_info { .gic_irq_number = { gic_irq },
		                          .pin_irq_number = { pin_irq } };

		pin.associate_with_gic_and_unmask_irq(irq_info);
	});
}


extern "C" void lx_emul_pin_irq_ack(unsigned pin_irq)
{
	pins().irq_ack( Pin_irq_number { pin_irq } );
}


extern "C" unsigned lx_emul_pin_last_irq(void)
{
	return pins().last_irq.value;
}
