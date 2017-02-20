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

namespace Terminal {

	struct Character;
	struct Boundary;
	struct Offset;
	struct Position;
	struct Character_array;
	template <unsigned, unsigned> class Static_character_array;
}


/*
 * The character definition is wrapped in a compound data structure to make
 * the implementation easily changeable to unicode later.
 */
struct Terminal::Character
{
	unsigned char c;

	Character() : c(0) { }
	Character(unsigned char c) : c(c) { }

	bool valid() const { return c != 0; }

	unsigned char ascii() const { return c; }
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

	/**
	 * Return true if position lies within the specified boundaries
	 */
	bool lies_within(Boundary const &boundary) const
	{
		return x >= 0 && x < boundary.width
		    && y >= 0 && y < boundary.height;
	}
};


struct Terminal::Character_array
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
