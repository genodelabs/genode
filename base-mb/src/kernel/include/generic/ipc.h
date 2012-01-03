/*
 * \brief  IPC Framework inside kernel
 * \author Martin Stein
 * \date   2011-02-22
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__IPC_H_ 
#define _KERNEL__INCLUDE__GENERIC__IPC_H_

#include <generic/verbose.h>


namespace Kernel {

	enum { IPC__VERBOSE = 0 };

	namespace Ipc {

		class Participates_dialog;
		typedef Kernel::Queue<Participates_dialog> Participant_queue;

		class Participates_dialog :
			public Participant_queue::Item
		{
			typedef Participates_dialog Participant;

			public:

				typedef Kernel::Utcb Utcb;

				inline unsigned message_size() { return _message_size; }

				inline byte_t message(unsigned i) { return _message[i]; }

				inline void print_message();

				byte_t *_message;

			private:

				Participant_queue _announced_clients;
				Participant*      _current_client;
				Utcb*             _utcb;
				unsigned          _message_size;
				bool              _waiting_for_reply;
				bool              _recieved_reply;

				inline void _recieve_message(Participant* sender);

			protected:

				inline void _send_message(Participant* server, 
                                          void* message, 
                                          unsigned size);

				inline Participates_dialog(Utcb* utcb); 

				inline Utcb* utcb() { return _utcb; }

				inline void recieve_reply(Participant* server);

				inline void announce_client(Participant* client);

				virtual ~Participates_dialog() { }

				virtual void ipc_sleep() = 0;

				virtual void ipc_wake() = 0;

			public:

				inline bool can_get_reply(Participant     *server,
				                          unsigned  request_size,
				                          unsigned *reply_size);

				inline bool can_reply_and_get_next_request(unsigned reply_size,
				                                           unsigned* request_size);

			protected:

				inline void _can_get_reply__error__invalid_server();

				inline void _recieve_message__error__invalid_message_size();

				inline void _can_reply_and_get_request__verbose__replied_to_request();

				inline void _can_reply_and_get_request__verbose__recieved_request();

				inline void _can_reply_and_get_request__verbose__waiting_for_request();

				inline void _send_message__verbose__success(Participant* server);

				inline void _can_get_reply__verbose__waiting_for_reply(Participant* server);

				inline void _can_get_reply__verbose__recieved_reply(Participant* server);
		};
	}
}


Kernel::Ipc::Participates_dialog::Participates_dialog(Utcb* utcb) :
	_current_client(0),
	_utcb(utcb),
	_waiting_for_reply(false),
	_recieved_reply(false)
{ }


void Kernel::Ipc::Participates_dialog::print_message()
{
	printf("  _message=0x%p\n", _message);

	for (unsigned current_byte=0;;){
		printf("  offset 0x%2X: 0x%p -> 0x",
			   current_byte, &_message[current_byte]);

		printf("%2X", _message[current_byte]);
		if (++current_byte>=_message_size) break;

		printf("%2X", _message[current_byte]);
		if (++current_byte>=_message_size) break;

		printf("%2X", _message[current_byte]);
		if (++current_byte>=_message_size) break;

		printf("%2X", _message[current_byte]);
		printf("\n");
		if (++current_byte>=_message_size) break;
	}
}


void Kernel::Ipc::Participates_dialog::_can_get_reply__error__invalid_server()
{
	printf("Error in Kernel::Ipc::Participates_dialog::can_get_reply, "
		   "invalid server, halt\n");
	halt();
}


void Kernel::Ipc::Participates_dialog::_recieve_message__error__invalid_message_size()
{
	printf("Error in Kernel::Ipc::Participates_dialog::recieve_message, "
		   "invalid message size, halt");
	Kernel::halt();
}


void Kernel::Ipc::Participates_dialog::_can_reply_and_get_request__verbose__replied_to_request()
{
	if (!IPC__VERBOSE) return;


	printf("Kernel::Ipc::Participates_dialog::can_reply_and_get_request, "
		   "replied to request, this=0x%p, _current_client=0x%p, "
		   "_message_size=%i\n",
		   this, _current_client, _message_size);
}


void Kernel::Ipc::Participates_dialog::_can_reply_and_get_request__verbose__recieved_request()
{
	if (!IPC__VERBOSE) return;

	printf("Kernel::Ipc::Participates_dialog::can_reply_and_get_request, "
		   "recieved request, this=0x%p, _current_client=0x%p, "
		   "_message_size=%i\n",
		   this, _current_client, _message_size);

	if (IPC__VERBOSE >= 2)
		print_message();
}


void Kernel::Ipc::Participates_dialog::_can_reply_and_get_request__verbose__waiting_for_request()
{
	if (!IPC__VERBOSE) return;

	printf("Kernel::Ipc::Participates_dialog::can_reply_and_get_request, "
		   "waiting for request, this=0x%p\n",
		   this);
}


void Kernel::Ipc::Participates_dialog::_send_message__verbose__success(Participant* server)
{
	if (!IPC__VERBOSE) return;

	printf("Kernel::Ipc::Participates_dialog::send_message, "
		   "this=0x%p, server=0x%p, _message_size=%i, print message\n",
		   this, server, _message_size);

	if (IPC__VERBOSE >= 2)
		print_message();
}


void Kernel::Ipc::Participates_dialog::_can_get_reply__verbose__waiting_for_reply(Participant* server)
{
	if (!IPC__VERBOSE) return;

	printf("Kernel::Ipc::Participates_dialog::can_get_reply, waiting for reply, "
		   "this=0x%p, server=0x%p, _message_size=%i\n",
		   this, server, _message_size);
}


void Kernel::Ipc::Participates_dialog::_can_get_reply__verbose__recieved_reply(Participant* server)
{
	if (!IPC__VERBOSE) return;

	printf("Kernel::Ipc::Participates_dialog::can_get_reply, recieved reply, "
		   "this=0x%p, server=0x%p, _message_size=%i\n",
		   this, server, _message_size);
}


void Kernel::Ipc::Participates_dialog::_send_message(Participant* server, 
                                                     void* message, 
                                                     unsigned size)
{
	_message_size = size;
	_message      = (byte_t*)message;

	server->announce_client(this);
	_send_message__verbose__success(server);
}


bool Kernel::Ipc::Participates_dialog::can_reply_and_get_next_request(unsigned reply_size,
                                                                      unsigned* request_size)
{
	if (_current_client) {
		_message_size = reply_size;
		_message      = (byte_t*)&_utcb->byte[0];

		_can_reply_and_get_request__verbose__replied_to_request();

		_current_client->recieve_reply(this);
		_current_client = 0;
	}

	_current_client=_announced_clients.dequeue();
	if (!_current_client) {
		_can_reply_and_get_request__verbose__waiting_for_request();
		return false;
	} else{
		_recieve_message(_current_client);
		*request_size = _message_size;
		_can_reply_and_get_request__verbose__recieved_request();
		return true;
	}
}


void Kernel::Ipc::Participates_dialog::_recieve_message(Participant* sender)
{
	if (sender->message_size() > sizeof(Utcb))
		_recieve_message__error__invalid_message_size();

	_message_size = sender->message_size();
	_message      = (byte_t*)&_utcb->byte[0];

	for (unsigned current_byte = 0; current_byte < _message_size; current_byte++)
		_message[current_byte] =
			sender->message(current_byte);
}


void Kernel::Ipc::Participates_dialog::announce_client(Participant* client)
{
	_announced_clients.enqueue(client);
}


void Kernel::Ipc::Participates_dialog::recieve_reply(Participant* server)
{
	if (!_waiting_for_reply || _recieved_reply)
		return;

	_recieve_message(server);
	_recieved_reply = true;
}


bool Kernel::Ipc::Participates_dialog::can_get_reply(Participant * server,
                                                     unsigned      request_size,
                                                     unsigned *    reply_size)
{
	if (!_waiting_for_reply) {

		if (!server)
			_can_get_reply__error__invalid_server();

		_message_size      = request_size;
		_message           = (byte_t*)&_utcb->byte[0];
		_recieved_reply    = false;
		_waiting_for_reply = true;

		server->announce_client(this);
	}

	if (!_recieved_reply) {
		_can_get_reply__verbose__waiting_for_reply(server);
		return false;
	} else {
		_can_get_reply__verbose__recieved_reply(server);

		_waiting_for_reply = false;
		*reply_size        = _message_size;
		return true;
	}
}


#endif /* _KERNEL__INCLUDE__GENERIC__IPC_H_ */
