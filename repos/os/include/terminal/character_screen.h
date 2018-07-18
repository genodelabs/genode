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
	 * Cursor Character Absolute - 8.3.9
	 */
	virtual void cha(int pn = 1) = 0;

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
	virtual void cub(int pn = 1) = 0;

	/**
	 * Cursor right - 8.3.20
	 */
	virtual void cuf(int pn = 1) = 0;

	/**
	 * Move cursor to row #1 column #2
	 */
	virtual void cup(int, int) = 0;

	/**
	 * Cursor Down - 8.3.19
	 */
	virtual void cud(int pn = 1) = 0;

	/**
	 * Cursor Up - 8.3.22
	 */
	virtual void cuu(int pn = 1) = 0;

	/**
	 * Device Attributes - 8.3.24
	 */
	virtual void da(int ps = 0) = 0;

	/**
	 * Delete Character - 8.3.26
	 */
	virtual void dch(int pn = 1) = 0;

	/**
	 * Delete line - 8.3.32
	 */
	virtual void dl(int pn = 1) = 0;


	/**
	 * Erase Character - 8.3.38
	 */
	virtual void ech(int pn = 1) = 0;

	/**
	 * Erase in page - 8.3.39
	 */
	virtual void ed(int ps = 0) = 0;

	/**
	 * Erase in line - 8.3.41
	 */
	virtual void el(int ps = 0) = 0;

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
	 * Set a tab in every row, current column - 8.3.62
	 */
	virtual void hts() = 0;

	/**
	 * Insert character - 8.3.64
	 */
	virtual void ich(int pn = 1) = 0;

	/**
	 * Insert line - 8.3.67
	 */
	virtual void il(int pn = 1) = 0;

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
	 * Reset Mode - 8.3.106
	 */
	virtual void rm(int) = 0;

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
	 * Scroll Down - 8.3.113
	 */
	virtual void sd(int pn = 1) = 0;

	/**
	 * Set background color to #1, using ANSI escape
	 */
	virtual void setab(int) = 0;

	/**
	 * Set foreground color to #1, using ANSI escape
	 */
	virtual void setaf(int) = 0;

	/**
	 * Select Graphic Rendition - 8.3.117
	 */
	virtual void sgr(int ps = 0) = 0;

	/**
	 * Set mode 8.3.125
	 */
	virtual void sm(int) = 0;

	/**
	 * Scroll Up - 8.3.147
	 */
	virtual void su(int pn = 1) = 0;

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

	/**
	 * Tabulation Stop Remove - 8.3.156
	 */
	virtual void tsr(int) = 0;

	/**
	 * Line position absolute - 8.3.158
	 */
	virtual void vpa(int pn = 1) = 0;

	/**
	 * Line position backward - 8.3.159
	 */
	virtual void vpb(int pn = 1) = 0;

	/*****************
	 ** DEC private **
	 *****************/

	/**
	 * Save Cursor
	 */
	virtual void decsc() = 0;

	/**
	 * Restore Cursor
	 */
	virtual void decrc() = 0;

	/**
	 * Set mode
	 */
	virtual void decsm(int p1, int p2 = 0) = 0;

	/**
	 * Reset mode
	 */
	virtual void decrm(int p1, int p2 = 0) = 0;


	/**************************
	 ** Select Character Set **
	 **************************/

	virtual void scs_g0(int) = 0;
	virtual void scs_g1(int) = 0;

	/*************
	 ** Unknown **
	 *************/

	virtual void reverse_index() = 0;
};

#endif /* _TERMINAL__CHARACTER_SCREEN_H_ */
