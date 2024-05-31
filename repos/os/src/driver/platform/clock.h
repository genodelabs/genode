/*
 * \brief  Clock interface for platform driver
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2020-06-12
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CLOCK_H_
#define _CLOCK_H_

#include <types.h>
#include <named_registry.h>

namespace Driver {

	using namespace Genode;

	class Clock;
	class Fixed_clock;
	class Fixed_divider;

	using Clocks = Named_registry<Clock>;
}


class Driver::Clock : Clocks::Element, Interface
{
	private:

		/* friendships needed to make 'Clocks::Element' private */
		friend class Clocks::Element;
		friend class Genode::Avl_node<Clock>;
		friend class Genode::Avl_tree<Clock>;

		Switch<Clock> _switch { *this, &Clock::_enable, &Clock::_disable };

	protected:

		virtual void _enable()  { }
		virtual void _disable() { }

	public:

		using Name = Clocks::Element::Name;
		using Clocks::Element::name;
		using Clocks::Element::Element;

		struct Rate { unsigned long value; };

		virtual void rate(Rate) { }
		virtual Rate rate() const { return Rate { }; }
		virtual void parent(Name) { }

		void enable()  { _switch.use();   }
		void disable() { _switch.unuse(); }

		struct Guard : Genode::Noncopyable
		{
			Clock &_clock;
			Guard(Clock &clock) : _clock(clock) { _clock.enable(); }
			~Guard() { _clock.disable(); }
		};
};


class Driver::Fixed_clock : public Driver::Clock
{
	private:

		Rate const _rate;

	public:

		Fixed_clock(Clocks &clocks, Name const &name, Rate rate)
		:
			Clock(clocks, name), _rate(rate)
		{ }

		Rate rate() const override { return _rate; }
};


class Driver::Fixed_divider : public Driver::Clock
{
	private:

		Clock &_parent;

		unsigned const _divider;

	public:

		Fixed_divider(Clocks     &clocks,
		              Name const &name,
		              Clock      &parent,
		              unsigned    divider)
		:
			Clock(clocks, name), _parent(parent), _divider(divider)
		{ }

		Rate rate() const override
		{
			return Rate { _parent.rate().value / _divider };
		}
};

#endif /* _CLOCK_H_ */
