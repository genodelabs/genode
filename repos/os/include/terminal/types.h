/*
 * \brief  Types used by terminal interfaces
 * \author Norman Feske
 * \date   2011-07-05
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__TYPES_H_
#define _TERMINAL__TYPES_H_

/* Genode includes */
#include <util/utf8.h>
#include <util/interface.h>

namespace Terminal {
	using Genode::Codepoint;
	struct Character;
	struct Boundary;
	struct Offset;
	struct Position;
	struct Character_array;
	template <unsigned, unsigned> class Static_character_array;
}


struct Terminal::Character
{
	using value_t = Genode::uint16_t;

	value_t value = 0;

	Character() { }

	Character(Codepoint cp)
	: value((value_t)(cp.value < (1<<16) ? cp.value : 0)) { }

	bool valid() const { return value != 0; }
};


struct Terminal::Boundary
{
	int const width, height;
	Boundary(int width, int height) : width(width), height(height) { }
};


struct Terminal::Offset
{
	int const x, y;

	Offset(int x, int y) : x(x), y(y) { }
};


struct Terminal::Position
{
	int x, y;

	Position() : x(0), y(0) { }
	Position(int x, int y) : x(x), y(y) { }

	Position operator + (Offset const &offset) {
		return Position(x + offset.x, y + offset.y); }

	bool operator == (Position const &pos) const {
		return (pos.x == x) && (pos.y == y); }

	bool operator != (Position const &pos) const {
		return (pos.x != x) || (pos.y != y); }

	bool operator >= (Position const &other) const
	{
		if (y > other.y)
			return true;

		if (y == other.y && x >= other.x)
			return true;

		return false;
	}

	bool in_range(Position start, Position end) const
	{
		return (end >= start) ? *this >= start &&   end >= *this
		                      : *this >= end   && start >= *this;
	}

	/**
	 * Return true if position lies within the specified boundaries
	 */
	bool lies_within(Boundary const &boundary) const
	{
		return x >= 0 && x < boundary.width
		    && y >= 0 && y < boundary.height;
	}

	/**
	 * Make sure that position lies within specified boundaries
	 */
	void constrain(Boundary const &boundary)
	{
		using namespace Genode;
		x = max(0, min(boundary.width - 1, x));
		y = max(0, min(boundary.height - 1, y));
	}

	void print(Genode::Output &out) const {
		Genode::print(out, y, ",", x); }
};


struct Terminal::Character_array : Genode::Interface
{
	/**
	 * Assign character to specified position
	 */
	virtual void set(Position const &pos, Character c) = 0;

	/**
	 * Request character at specified position
	 */
	virtual Character get(Position const &pos) const = 0;

	/**
	 * Return array boundary
	 */
	virtual Boundary boundary() const = 0;
};


template <unsigned WIDTH, unsigned HEIGHT>
class Terminal::Static_character_array : public Character_array
{
	private:

		Character      _array[HEIGHT][WIDTH];
		Boundary const _boundary;

	public:

		Static_character_array() : _boundary(WIDTH, HEIGHT) { }

		void set(Position const &pos, Character c)
		{
			if (pos.lies_within(_boundary))
				_array[pos.y][pos.x] = c;
		}

		Character get(Position const &pos) const
		{
			if (pos.lies_within(_boundary))
				return _array[pos.y][pos.x];
			else
				return Character();
		}

		Boundary boundary() const { return _boundary; }
};

#endif /* _TERMINAL__TYPES_H_ */
