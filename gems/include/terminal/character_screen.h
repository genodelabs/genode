/*
 * \brief  Character-screen interface
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TERMINAL__CHARACTER_SCREEN_H_
#define _TERMINAL__CHARACTER_SCREEN_H_

#include <terminal/types.h>

namespace Terminal {

	/**
	 * Character-screen interface called by input-stream decoder
	 */
	struct Character_screen
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
		 * Reset string
		 */
		virtual void cpr() = 0;

		/**
		 * Change region to line #1 ... line #2
		 */
		virtual void csr(int, int) = 0;

		/**
		 * Non-destructive space - move right #1 spaces
		 */
		virtual void cuf(int) = 0;

		/**
		 * Move cursor to row #1 column #2
		 */
		virtual void cup(int, int) = 0;

		/**
		 * Move cursor up one line
		 */
		virtual void cuu1() = 0;

		/**
		 * Delete #1 characters
		 */
		virtual void dch(int) = 0;

		/**
		 * Delete #1 lines
		 */
		virtual void dl(int) = 0;

		/**
		 * Erase #1 characters
		 */
		virtual void ech(int) = 0;

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
		 * Home cursor
		 */
		virtual void home() = 0;

		/**
		 * Horizontal position #1 absolute
		 */
		virtual void hpa(int) = 0;

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
		 * Set all color pairs to the original ones
		 */
		virtual void oc() = 0;

		/**
		 * Set default pair to its original value
		 */
		virtual void op() = 0;

		/**
		 * Restore cursor to position of last save_cursor
		 */
		virtual void rc() = 0;

		/**
		 * Scroll text down
		 */
		virtual void ri() = 0;

		/**
		 * Reset string
		 */
		virtual void ris() = 0;

		/**
		 * Turn off automatic margins
		 */
		virtual void rmam() = 0;

		/**
		 * Exit insert mode
		 */
		virtual void rmir() = 0;

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
		 * Turn on automatic margins
		 */
		virtual void smam() = 0;

		/**
		 * Enter insert mode
		 */
		virtual void smir() = 0;

		/**
		 * Clear all tab stops
		 */
		virtual void tbc() = 0;

		/**
		 * User strings
		 */
		virtual void u6(int, int) = 0;
		virtual void u7() = 0;
		virtual void u8() = 0;
		virtual void u9() = 0;

		/**
		 * Vertical position #1 absolute)
		 */
		virtual void vpa(int) = 0;
	};
}

#endif /* _TERMINAL__CHARACTER_SCREEN_H_ */
