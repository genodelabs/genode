/*
 * \brief  Character-screen interface
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__CHARACTER_SCREEN_H_
#define _TERMINAL__CHARACTER_SCREEN_H_

#include <terminal/types.h>

namespace Terminal { struct Character_screen; }


/**
 * Character-screen interface called by input-stream decoder
 */
struct Terminal::Character_screen : Genode::Interface
{
	virtual void output(Character c) = 0;


	/*******************
	 ** VT Operations **
	 *******************/

	/*
	 * The VT operations are named according to the command names used by
	 * their respective terminfo definitions. See 'man 5 terminfo' for a
	 * thorough description of these commands.
	 */

	/**
	 * Back tab
	 */
	virtual void cbt() = 0;

	/**
	 * Make cursor invisible
	 */
	virtual void civis() = 0;

	/**
	 * Make cursor normal
	 */
	virtual void cnorm() = 0;

	/**
	 * Make cursor very visible
	 */
	virtual void cvvis() = 0;

	/**
	 * Change region to line #1 ... line #2
	 */
	virtual void csr(int, int) = 0;

	/**
	 * Move cursor backwards
	 */
	virtual void cub(int) = 0;

	/**
	 * Non-destructive space - move right #1 spaces
	 */
	virtual void cuf(int) = 0;

	/**
	 * Move cursor to row #1 column #2
	 */
	virtual void cup(int, int) = 0;

	/**
	 * Down #1 lines
	 */
	virtual void cud(int) = 0;

	/**
	 * Move cursor up one line
	 */
	virtual void cuu1() = 0;

	/**
	 * Up #1 lines
	 */
	virtual void cuu(int) = 0;

	/**
	 * Delete #1 characters
	 */
	virtual void dch(int) = 0;

	/**
	 * Delete #1 lines
	 */
	virtual void dl(int) = 0;

	/**
	 * Clear to end of screen
	 */
	virtual void ed() = 0;

	/**
	 * Clear to end of line
	 */
	virtual void el() = 0;

	/**
	 * Clear to beginning of line
	 */
	virtual void el1() = 0;

	/**
	 * Enable alternative character set
	 */
	virtual void enacs() = 0;

	/**
	 * Visible bell
	 */
	virtual void flash() = 0;

	/**
	 * Home cursor
	 */
	virtual void home() = 0;

	/**
	 * Set a tab in every row, current column
	 */
	virtual void hts() = 0;

	/**
	 * Insert #1 characters
	 */
	virtual void ich(int) = 0;

	/**
	 * Insert #1 lines
	 */
	virtual void il(int) = 0;

	/**
	 * Initialization string
	 */
	virtual void is2() = 0;

	/**
	 * Newline
	 */
	virtual void nel() = 0;

	/**
	 * Set default pair to its original value
	 */
	virtual void op() = 0;

	/**
	 * Restore cursor to position of last save_cursor
	 */
	virtual void rc() = 0;

	/**
	 * Reset string
	 */
	virtual void rs2() = 0;

	/**
	 * Leave cup mode
	 */
	virtual void rmcup() = 0;

	/**
	 * Exit insert mode
	 */
	virtual void rmir() = 0;

	/**
	 * Exit keyboard transmission mode
	 */
	virtual void rmkx() = 0;

	/**
	 * Set background color to #1, using ANSI escape
	 */
	virtual void setab(int) = 0;

	/**
	 * Set foreground color to #1, using ANSI escape
	 */
	virtual void setaf(int) = 0;

	/**
	 * Set attribute
	 */
	virtual void sgr(int) = 0;

	/**
	 * Turn of all attributes
	 */
	virtual void sgr0() = 0;

	/**
	 * Save current cursor position
	 */
	virtual void sc() = 0;

	/**
	 * Enter cup mode
	 */
	virtual void smcup() = 0;

	/**
	 * Enter insert mode
	 */
	virtual void smir() = 0;

	/**
	 * Enter keyboard transmission mode
	 */
	virtual void smkx() = 0;

	/**
	 * Clear all tab stops
	 */
	virtual void tbc() = 0;
};

#endif /* _TERMINAL__CHARACTER_SCREEN_H_ */
