/*
 * \brief  Platform-independent part of signal framework
 * \author Norman Feske
 * \author Christian Prochaska
 * \author Martin Stein
 * \date   2013-02-21
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/signal.h>

using namespace Genode;


/************
 ** Signal **
 ************/

Signal::Signal(Signal const &other)
{
	_data.context = other._data.context;
	_data.num     = other._data.num;

	_inc_ref();
}


Signal & Signal::operator=(Signal const &other)
{
	if ((_data.context == other._data.context) &&
	    (_data.num     == other._data.num))
		return *this;

	_dec_ref_and_unlock();

	_data.context = other._data.context;
	_data.num     = other._data.num;

	_inc_ref();

	return *this;
}


Signal::~Signal() { _dec_ref_and_unlock(); }


/********************
 ** Signal_context **
 ********************/

Signal_context::~Signal_context()
{
	/*
	 * Detect bug in an application where a signal context is destroyed prior
	 * dissolving it from the signal receiver.
	 */
	if (_receiver)
		PERR("Destructing undissolved signal context");
}


/************************
 ** Signal_transmitter **
 ************************/

Signal_transmitter::Signal_transmitter(Signal_context_capability context)
: _context(context) { }


void Signal_transmitter::context(Signal_context_capability context) {
	_context = context; }


/*********************
 ** Signal_receiver **
 *********************/

Signal_receiver::~Signal_receiver()
{
	Lock::Guard list_lock_guard(_contexts_lock);

	/* disassociate contexts from the receiver */
	for (List_element<Signal_context> *le; (le = _contexts.first()); )
		_unsynchronized_dissolve(le->object());

	_platform_destructor();
}
