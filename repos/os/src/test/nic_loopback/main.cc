/*
 * \brief  Test for the NIC loop-back service
 * \author Norman Feske
 * \date   2009-11-14
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/allocator_avl.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <timer_session/connection.h>

namespace Genode {

	static inline void print(Output &out, Packet_descriptor packet)
	{
		Genode::print(out, "offset=", packet.offset(), ", size=", packet.size());
	}
}


using namespace Genode;


static bool single_packet_roundtrip(Nic::Session  *nic,
                                    unsigned char  content_pattern,
                                    size_t         packet_size)
{
	Packet_descriptor tx_packet;

	log("single_packet_roundtrip(content='", Char(content_pattern), "', "
	    "packet_size=", packet_size, ")");

	/* allocate transmit packet */
	try {
		tx_packet = nic->tx()->alloc_packet(packet_size);
	} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
		error(__func__, ": tx packet alloc failed");
		return false;
	}

	log("allocated tx packet ", tx_packet);

	/* fill packet with pattern */
	char *tx_content = nic->tx()->packet_content(tx_packet);
	for (unsigned i = 0; i < packet_size; i++)
		tx_content[i] = content_pattern;

	nic->tx()->submit_packet(tx_packet);

	/* wait for acknowledgement */
	Packet_descriptor ack_tx_packet = nic->tx()->get_acked_packet();

	if (ack_tx_packet.size()   != tx_packet.size()
	 || ack_tx_packet.offset() != tx_packet.offset()) {
		error("unexpected acked packet");
		return false;
	}

	/*
	 * Because we sent the packet to a loop-back device, we expect
	 * the same packet to be available at the rx channel of the NIC
	 * session.
	 */
	Packet_descriptor rx_packet = nic->rx()->get_packet();
	log("received rx packet ", rx_packet);

	if (rx_packet.size() != tx_packet.size()) {
		error("sent and echoed packets differ in size");
		return false;
	}

	/* compare original and echoed packets */
	char *rx_content = nic->rx()->packet_content(rx_packet);
	for (unsigned i = 0; i < packet_size; i++)
		if (rx_content[i] != tx_content[i]) {
			error("sent and echoed packets have differnt content");
			return false;
		}

	/* acknowledge received packet */
	nic->rx()->acknowledge_packet(rx_packet);

	/* release sent packet to free the space in the tx communication buffer */
	nic->tx()->release_packet(tx_packet);

	return true;
}


static bool batch_packets(Nic::Session *nic, unsigned num_packets)
{
	unsigned tx_cnt = 0, acked_cnt = 0, rx_cnt = 0, batch_cnt = 0;

	Genode::Signal_context tx_ready_to_submit, tx_ack_avail,
	                       rx_ready_to_ack, rx_packet_avail;
	Genode::Signal_receiver signal_receiver;

	nic->tx_channel()->sigh_ready_to_submit (signal_receiver.manage(&tx_ready_to_submit));
	nic->tx_channel()->sigh_ack_avail       (signal_receiver.manage(&tx_ack_avail));
	nic->rx_channel()->sigh_ready_to_ack    (signal_receiver.manage(&rx_ready_to_ack));
	nic->rx_channel()->sigh_packet_avail    (signal_receiver.manage(&rx_packet_avail));

	enum { PACKET_SIZE = 100 };

	while (acked_cnt != num_packets
	    || tx_cnt    != num_packets
	    || rx_cnt    != num_packets) {

		if (tx_cnt > rx_cnt || tx_cnt > acked_cnt)
			signal_receiver.wait_for_signal();

		/* produce as many packets as possible as one batch */
		unsigned max_outstanding_requests = Nic::Session::QUEUE_SIZE - 1;
		while (nic->tx()->ready_to_submit()
		    && tx_cnt < num_packets
		    && tx_cnt - rx_cnt < max_outstanding_requests) {

			try {
				Packet_descriptor tx_packet = nic->tx()->alloc_packet(PACKET_SIZE);
				nic->tx()->submit_packet(tx_packet);
				tx_cnt++;
			} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
				break;
			}
		}

		unsigned batch_rx_cnt = 0, batch_acked_cnt = 0;

		/* check for acknowledgements */
		while (nic->tx()->ack_avail()) {
			Packet_descriptor acked_packet = nic->tx()->get_acked_packet();
			nic->tx()->release_packet(acked_packet);
			acked_cnt++;
			batch_acked_cnt++;
		}

		/* check for available rx packets */
		while (nic->rx()->packet_avail() && nic->rx()->ready_to_ack()) {
			Packet_descriptor rx_packet = nic->rx()->get_packet();

			if (!nic->rx()->ready_to_ack())
				warning("not ready for ack, going to blocK");

			nic->rx()->acknowledge_packet(rx_packet);
			rx_cnt++;
			batch_rx_cnt++;
		}

		log("acked ", batch_acked_cnt, " packets, "
		    "received ", batch_rx_cnt, " packets "
		    "-> tx: ", tx_cnt, ", acked: ", acked_cnt, ", rx: ", rx_cnt);

		batch_cnt++;
	}

	log("test used ", batch_cnt, " batches");
	return true;
}


int main(int, char **)
{
	log("--- NIC loop-back test ---");

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	bool config_test_roundtrip = true;
	bool config_test_batch     = true;

	if (config_test_roundtrip) {
		log("-- test roundtrip two times (packet offsets should be the same) --");
		Allocator_avl tx_block_alloc(env()->heap());
		Nic::Connection nic(&tx_block_alloc, BUF_SIZE, BUF_SIZE);
		single_packet_roundtrip(&nic, 'a', 100);
		single_packet_roundtrip(&nic, 'b', 100);
	}

	if (config_test_batch) {
		log("-- test submitting and receiving batches of packets --");
		Allocator_avl tx_block_alloc(env()->heap());
		Nic::Connection nic(&tx_block_alloc, BUF_SIZE, BUF_SIZE);
		enum { NUM_PACKETS = 1000 };
		batch_packets(&nic, NUM_PACKETS);
	}

	log("--- finished NIC loop-back test ---");
	return 0;
}
