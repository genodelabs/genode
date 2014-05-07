/*
 * \brief  Dummy implementation of the IPC API
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>

using namespace Genode;


/*****************
 ** Ipc_ostream **
 *****************/

Ipc_ostream::Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg)
:
	Ipc_marshaller(&snd_msg->buf[0], snd_msg->size()),
	_snd_msg(snd_msg), _dst(dst)
{ }


/*****************
 ** Ipc_istream **
 *****************/

void Ipc_istream::_wait()
{ }


Ipc_istream::Ipc_istream(Msgbuf_base *rcv_msg) :
	Ipc_unmarshaller(&rcv_msg->buf[0], rcv_msg->size()),
	_rcv_msg(rcv_msg)
{ }


Ipc_istream::~Ipc_istream() { }


/****************
 ** Ipc_client **
 ****************/

void Ipc_client::_call() { }


Ipc_client::Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg, unsigned short)
: Ipc_istream(rcv_msg), Ipc_ostream(srv, snd_msg), _result(0)
{ }


/****************
 ** Ipc_server **
 ****************/

void Ipc_server::_wait() { }


void Ipc_server::_reply() { }


void Ipc_server::_reply_wait() { }


Ipc_server::Ipc_server(Msgbuf_base *snd_msg,
                       Msgbuf_base *rcv_msg)
: Ipc_istream(rcv_msg), Ipc_ostream(Native_capability(), snd_msg)
{ }
