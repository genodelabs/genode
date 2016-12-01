
#ifndef _FS_REGISTRY_H_
#define _FS_REGISTRY_H_

/* Genode includes */
#include <util/fifo.h>
#include <file_system_session/connection.h>
#include <base/allocator_avl.h>

namespace File_system {

	struct Packet_callback : Genode::Fifo<Packet_callback>::Element
	{
		Session::Tx::Sink &sink;
		Packet_descriptor &sink_packet;
		Packet_descriptor &source_packet;

		Packet_callback(Session::Tx::Sink &sink, Packet_descriptor sink_packet, Packet_descriptor source_packet):
			 sink(sink), sink_packet(sink_packet), source_packet(source_packet){ }
	};

	class FS_reference : public Genode::List<FS_reference>::Element, public File_system::Connection
	{
	private:
		static Genode::List<FS_reference>    _filesystems;

		Genode::String<64>                   _label;
		Genode::Fifo<Packet_callback>        _callbacks;
		Genode::size_t                       _callback_count=0;
		Genode::Allocator                   &_alloc;

		Genode::Signal_handler<FS_reference> _process_packet_dispatcher;

		FS_reference(Genode::Env &env, Genode::Allocator_avl &avl, char *label):
			Connection(env, avl, label), _label(label), _alloc(avl), _process_packet_dispatcher(env.ep(), *this, &FS_reference::_process_packets)
		{
			sigh_ready_to_submit(_process_packet_dispatcher);
			sigh_ack_avail(_process_packet_dispatcher);
		}

		void _process_packets()
		{
			while (tx()->ack_avail()) {
				Packet_callback *cb = _callbacks.dequeue();
				/* ready_to_ack() returns true if it ISN'T ready to acknowledge */
				if (!cb || cb->sink.ready_to_ack()) {
					_callbacks.enqueue(cb);
					return;
				}
				Genode::memcpy(cb->sink.packet_content(cb->sink_packet), tx()->packet_content(cb->source_packet), cb->sink_packet.size());
				cb->sink.acknowledge_packet(cb->sink_packet);
			}
		}

	public:
		
		static void add_fs(Genode::Env &env, Genode::Allocator_avl avl, char *label)
		{
			_filesystems.insert(new (avl) FS_reference(env, avl, label));
		}

		static FS_reference *get_fs(char *label)
		{
			for (FS_reference *ref = _filesystems.first(); ref; ref = ref->next()) {
				if (ref->_label==label) return ref;
			}
			throw Lookup_failed();
		}

		void send_packet(Packet_descriptor packet, Session::Tx::Sink &sink, Packet_descriptor callback)
		{
// 			if (tx()->ready_to_submit() && _callback_count < MAX_PACKET_CALLBACKS) {
				tx()->submit_packet(packet);
				_callbacks.enqueue(new (_alloc) Packet_callback(sink, callback, packet));
// 			} else {
// 				return false;
// 			}
		}

	};
}

#endif 
