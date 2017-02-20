/*
 * \brief  Tracer of the operations performed by the input-stream decoder
 * \author Norman Feske
 * \date   2011-07-05
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__CHARACTER_SCREEN_TRACER_H_
#define _TERMINAL__CHARACTER_SCREEN_TRACER_H_

#include <terminal/character_screen.h>

/**
 * Shortcut for character-screen operations with no, one, or two arguments
 */
#define CS_TRACE_OP(name) void name() { printf(#name "\n"); }
#define CS_TRACE_OP_1(name) void name(int p1) { printf(#name "(%d)\n", p1); }
#define CS_TRACE_OP_2(name) void name(int p1, int p2) { printf(#name "(%d,%d)\n", p1, p2); }

namespace Terminal { struct Character_screen_tracer };


/**
 * Character screen implementation that prints a trace of operations
 */
struct Terminal::Character_screen_tracer : Character_screen
{
	void output(char c) { printf("output('%c')\n", c); }

	CS_TRACE_OP(civis);
	CS_TRACE_OP(cnorm);
	CS_TRACE_OP(cvvis);
	CS_TRACE_OP(cpr);
	CS_TRACE_OP_2(csr);
	CS_TRACE_OP_1(cub);
	CS_TRACE_OP(cuf1);
	CS_TRACE_OP_2(cup);
	CS_TRACE_OP(cuu1);
	CS_TRACE_OP_1(dch);
	CS_TRACE_OP_1(dl);
	CS_TRACE_OP_1(ech);
	CS_TRACE_OP(ed);
	CS_TRACE_OP(el);
	CS_TRACE_OP(el1);
	CS_TRACE_OP(home);
	CS_TRACE_OP_1(hpa);
	CS_TRACE_OP(hts);
	CS_TRACE_OP_1(ich);
	CS_TRACE_OP_1(il);
	CS_TRACE_OP(oc);
	CS_TRACE_OP(op);
	CS_TRACE_OP(rc);
	CS_TRACE_OP(ri);
	CS_TRACE_OP(ris);
	CS_TRACE_OP(rmam);
	CS_TRACE_OP(rmir);
	CS_TRACE_OP_1(setab);
	CS_TRACE_OP_1(setaf);
	CS_TRACE_OP_1(sgr);
	CS_TRACE_OP(sc);
	CS_TRACE_OP(smam);
	CS_TRACE_OP(smir);
	CS_TRACE_OP(tbc);
	CS_TRACE_OP_2(u6);
	CS_TRACE_OP(u7);
	CS_TRACE_OP(u8);
	CS_TRACE_OP(u9);
	CS_TRACE_OP_1(vpa);
};

#undef CS_TRACE_OP
#undef CS_TRACE_OP_1
#undef CS_TRACE_OP_2

#endif /* _TERMINAL__CHARACTER_SCREEN_TRACER_H_ */
