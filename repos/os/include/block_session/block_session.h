/*
 * \brief  Block session interface.
 * \author Stefan Kalkowski
 * \date   2010-07-06
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK_SESSION__BLOCK_SESSION_H_
#define _INCLUDE__BLOCK_SESSION__BLOCK_SESSION_H_

#include <os/packet_stream.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <session/session.h>

namespace Block {

	/**
	 * Sector type for block session
	 */
	typedef Genode::uint64_t sector_t;

	class Packet_descriptor;
	struct Session;
}


/**
 * Representation of a block-operation request
 *
 * The data associated with the 'Packet_descriptor' is either
 * the data read from or written to the block indicated by
 * its number.
 */
class Block::Packet_descriptor : public Genode::Packet_descriptor
{
	public:

		enum Opcode { READ, WRITE, SYNC, TRIM, END };

		/*
		 * Alignment used when allocating a packet directly via the 'tx'
		 * packet stream. This is not recommended because it does not
		 * apply the server's alignment constraints. Instead, the
		 * 'Block::Session_client::alloc_packet' should be used for
		 * allocating properly aligned block-request packets.
		 */
		enum Alignment { PACKET_ALIGNMENT = 11 };

		/**
		 * Client-defined value for correlating acknowledgements with requests
		 */
		struct Tag { unsigned long value; };

	private:

		Opcode          _op;           /* requested operation */
		Tag             _tag;          /* client-defined request identifier */
		sector_t        _block_number; /* requested block number */
		Genode::size_t  _block_count;  /* number of blocks of operation */
		bool            _success;      /* indicates success of operation */

	public:

		/**
		 * Constructor
		 */
		Packet_descriptor(Genode::off_t offset=0, Genode::size_t size = 0)
		:
			Genode::Packet_descriptor(offset, size),
			_op(READ), _tag(), _block_number(0), _block_count(0), _success(false)
		{ }

		/**
		 * Constructor
		 */
		Packet_descriptor(Packet_descriptor p, Opcode op,
		                  sector_t block_number, Genode::size_t block_count = 1,
		                  Tag tag = { ~0U })
		:
			Genode::Packet_descriptor(p.offset(), p.size()),
			_op(op), _tag(tag), _block_number(block_number),
			_block_count(block_count), _success(false)
		{ }

		Opcode         operation()    const { return _op;           }
		sector_t       block_number() const { return _block_number; }
		Genode::size_t block_count()  const { return _block_count;  }
		bool           succeeded()    const { return _success;      }
		Tag            tag()          const { return _tag;          }

		void succeeded(bool b) { _success = b; }
};


/**
 * Block session interface
 *
 * A block session corresponds to a block device that can be used to read
 * or store data. Payload is communicated over the packet-stream interface
 * set up between 'Session_client' and 'Session_server'.
 *
 * Even though the methods 'tx' and 'tx_channel' are specific for the client
 * side of the block session interface, they are part of the abstract 'Session'
 * class to enable the client-side use of the block interface via a pointer to
 * the abstract 'Session' class. This way, we can transparently co-locate the
 * packet-stream server with the client in same program.
 */
struct Block::Session : public Genode::Session
{
	enum { TX_QUEUE_SIZE = 256 };

	typedef Genode::Packet_stream_policy<Block::Packet_descriptor,
	                                     TX_QUEUE_SIZE, TX_QUEUE_SIZE,
	                                     char> Tx_policy;

	typedef Packet_stream_tx::Channel<Tx_policy> Tx;

	typedef Packet_descriptor::Tag Tag;

	struct Info
	{
		Genode::size_t block_size;   /* size of one block in bytes */
		sector_t       block_count;  /* number of blocks */
		Genode::size_t align_log2;   /* packet alignment within payload buffer */
		bool           writeable;
	};

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Block"; }

	enum { CAP_QUOTA = 5 };

	virtual ~Session() { }

	/**
	 * Request information about the metrics of the block device
	 */
	virtual Info info() const = 0;

	/**
	 * Request packet-transmission channel
	 */
	virtual Tx *tx_channel() { return 0; }

	/**
	 * Request client-side packet-stream interface of tx channel
	 */
	virtual Tx::Source *tx() { return 0; }

	/**
	 * Return capability for packet-transmission channel
	 */
	virtual Genode::Capability<Tx> tx_cap() = 0;

	/**
	 * Return packet descriptor for syncing the entire block session
	 */
	static Packet_descriptor sync_all_packet_descriptor(Info const &info, Tag tag)
	{
		return Packet_descriptor(Packet_descriptor(0UL, 0UL),
		                         Packet_descriptor::SYNC,
		                         0UL, info.block_count, tag);
	}


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_info, Info, info);
	GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, tx_cap);
	GENODE_RPC_INTERFACE(Rpc_info, Rpc_tx_cap);
};

#endif /* _INCLUDE__BLOCK_SESSION__BLOCK_SESSION_H_ */
