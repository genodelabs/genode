/*
 * \brief   List of double connected items
 * \author  Martin Stein
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/double_list.h>

using namespace Kernel;


void Double_list::_connect_neighbors(Item * const i)
{
	i->_prev->_next = i->_next;
	i->_next->_prev = i->_prev;
}


void Double_list::_to_tail(Item * const i)
{
	if (i == _tail) { return; }
	_connect_neighbors(i);
	i->_prev = _tail;
	i->_next = 0;
	_tail->_next = i;
	_tail = i;
}


Double_list::Double_list(): _head(0), _tail(0) { }


void Double_list::to_tail(Item * const i)
{
	if (i == _head) { head_to_tail(); }
	else { _to_tail(i); }
}


void Double_list::insert_tail(Item * const i)
{
	if (_tail) { _tail->_next = i; }
	else { _head = i; }
	i->_prev = _tail;
	i->_next = 0;
	_tail = i;
}


void Double_list::insert_head(Item * const i)
{
	if (_head) { _head->_prev = i; }
	else { _tail = i; }
	i->_next = _head;
	i->_prev = 0;
	_head = i;
}


void Double_list::remove(Item * const i)
{
	if (i == _tail) { _tail = i->_prev; }
	else { i->_next->_prev = i->_prev; }
	if (i == _head) { _head = i->_next; }
	else { i->_prev->_next = i->_next; }
}


void Double_list::head_to_tail()
{
	if (!_head || _head == _tail) { return; }
	_head->_prev = _tail;
	_tail->_next = _head;
	_head = _head->_next;
	_head->_prev = 0;
	_tail = _tail->_next;
	_tail->_next = 0;
}
