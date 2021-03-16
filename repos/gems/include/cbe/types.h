/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_TYPES_H_
#define _CBE_TYPES_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>
#include <base/exception.h>
#include <util/string.h>

namespace Cbe {

	enum { INVALID_GENERATION = 0 };

	using namespace Genode;
	using Number_of_primitives   = size_t;
	using Physical_block_address = uint64_t;
	using Virtual_block_address  = uint64_t;
	using Generation             = uint64_t;
	using Generation_string      = String<21>;
	using Height                 = uint32_t;
	using Number_of_leaves       = uint64_t;
	using Number_of_leafs        = uint64_t;
	using Number_of_blocks       = uint64_t;
	using Degree                 = uint32_t;

	static constexpr uint32_t BLOCK_SIZE = 4096;
	static constexpr uint32_t NR_OF_SNAPSHOTS = 48;


	class Request
	{
		public:

			enum class Operation : uint32_t {
				INVALID = 0,
				READ = 1,
				WRITE = 2,
				SYNC = 3,
				CREATE_SNAPSHOT = 4,
				DISCARD_SNAPSHOT = 5,
				REKEY = 6,
				EXTEND_VBD = 7,
				EXTEND_FT = 8,
				RESUME_REKEYING = 10,
				DEINITIALIZE = 11,
				INITIALIZE = 12,
			};

		private:

			Operation        _operation;
			bool             _success;
			uint64_t         _block_number;
			uint64_t         _offset;
			Number_of_blocks _count;
			uint32_t         _key_id;
			uint32_t         _tag;

		public:

			Request(Operation        operation,
			        bool             success,
			        uint64_t         block_number,
			        uint64_t         offset,
			        Number_of_blocks count,
			        uint32_t         key_id,
			        uint32_t         tag)
			:
				_operation    { operation    },
				_success      { success      },
				_block_number { block_number },
				_offset       { offset       },
				_count        { count        },
				_key_id       { key_id       },
				_tag          { tag          }
			{ }

			Request()
			:
				_operation    { Operation::INVALID },
				_success      { false },
				_block_number { 0 },
				_offset       { 0 },
				_count        { 0 },
				_key_id       { 0 },
				_tag          { 0 }
			{ }

			bool valid() const
			{
				return _operation != Operation::INVALID;
			}

			void print(Genode::Output &out) const;


			/***************
			 ** Accessors **
			 ***************/

			bool read()             const { return _operation == Operation::READ; }
			bool write()            const { return _operation == Operation::WRITE; }
			bool sync()             const { return _operation == Operation::SYNC; }
			bool create_snapshot()  const { return _operation == Operation::CREATE_SNAPSHOT; }
			bool discard_snapshot() const { return _operation == Operation::DISCARD_SNAPSHOT; }
			bool rekey()            const { return _operation == Operation::REKEY; }
			bool extend_vbd()       const { return _operation == Operation::EXTEND_VBD; }
			bool extend_ft()        const { return _operation == Operation::EXTEND_FT; }
			bool resume_rekeying()  const { return _operation == Operation::RESUME_REKEYING; }
			bool deinitialize()     const { return _operation == Operation::DEINITIALIZE; }
			bool initialize()       const { return _operation == Operation::INITIALIZE; }

			Operation        operation()    const { return _operation; }
			bool             success()      const { return _success; }
			uint64_t         block_number() const { return _block_number; }
			uint64_t         offset()       const { return _offset; }
			Number_of_blocks count()        const { return _count; }
			uint32_t         key_id()       const { return _key_id; }
			uint32_t         tag()          const { return _tag; }

			void success(bool arg) { _success = arg; }
			void tag(uint32_t arg)    { _tag = arg; }

	} __attribute__((packed));

	class Trust_anchor_request
	{
		public:

			enum class Operation : uint32_t {
				INVALID           = 0,
				CREATE_KEY        = 1,
				SECURE_SUPERBLOCK = 2,
				ENCRYPT_KEY       = 3,
				DECRYPT_KEY       = 4,
				LAST_SB_HASH      = 5,
				INITIALIZE        = 6,
			};

		private:

			Operation _operation;
			bool      _success;
			uint32_t  _tag;

		public:

			Trust_anchor_request()
			:
				_operation { Operation::INVALID },
				_success   { false },
				_tag       { 0 }
			{ }

			Trust_anchor_request(Operation operation,
			                     bool      success,
			                     uint32_t  tag)
			:
				_operation { operation },
				_success   { success },
				_tag       { tag }
			{ }

			void print(Genode::Output &out) const;

			bool valid()             const { return _operation != Operation::INVALID; }
			bool create_key()        const { return _operation == Operation::CREATE_KEY; }
			bool secure_superblock() const { return _operation == Operation::SECURE_SUPERBLOCK; }
			bool encrypt_key()       const { return _operation == Operation::ENCRYPT_KEY; }
			bool decrypt_key()       const { return _operation == Operation::DECRYPT_KEY; }
			bool last_sb_hash()      const { return _operation == Operation::LAST_SB_HASH; }
			bool initialize()        const { return _operation == Operation::INITIALIZE; }

			Operation operation() const { return _operation; }
			bool      success()   const { return _success; }
			uint32_t  tag()       const { return _tag; }

			void tag(uint32_t arg) { _tag = arg; }
			void success(bool arg) { _success = arg; }

	} __attribute__((packed));


	struct Block_data
	{
		char values[BLOCK_SIZE];

		void print(Genode::Output &out) const
		{
			using namespace Genode;
			for (char const c : values) {
				Genode::print(out, Hex(c, Hex::OMIT_PREFIX, Hex::PAD), " ");
			}
			Genode::print(out, "\n");
		}
	} __attribute__((packed));


	class Io_buffer
	{
		private:

			Block_data items[1];

		public:

			struct Bad_index : Genode::Exception { };

			struct Index
			{
				uint32_t value;

				explicit Index(uint32_t value) : value(value) { }

			} __attribute__((packed));

			Block_data &item(Index const idx)
			{
				if (idx.value >= sizeof(items) / sizeof(items[0])) {
					throw Bad_index();
				}
				return items[idx.value];
			}

	} __attribute__((packed));


	class Crypto_plain_buffer
	{
		private:

			Block_data items[1];

		public:

			struct Bad_index : Genode::Exception { };

			struct Index
			{
				uint32_t value;

				explicit Index(uint32_t value) : value(value) { }

			} __attribute__((packed));

			Block_data &item(Index const idx)
			{
				if (idx.value >= sizeof(items) / sizeof(items[0])) {
					throw Bad_index();
				}
				return items[idx.value];
			}

	} __attribute__((packed));


	class Crypto_cipher_buffer
	{
		private:

			Block_data items[1];

		public:

			struct Bad_index : Genode::Exception { };

			struct Index
			{
				uint32_t value;

				explicit Index(uint32_t value) : value(value) { }

			} __attribute__((packed));

			Block_data &item(Index const idx)
			{
				if (idx.value >= sizeof(items) / sizeof(items[0])) {
					throw Bad_index();
				}
				return items[idx.value];
			}
	} __attribute__((packed));


	/*
	 * The Hash contains the hash of a node.
	 */
	struct Hash
	{
		enum { MAX_LENGTH = 32, };
		char values[MAX_LENGTH];

		/* hash as hex value plus "0x" prefix and terminating null */
		using String = Genode::String<sizeof(values) * 2 + 3>;

		/* debug */
		void print(Genode::Output &out) const
		{
			using namespace Genode;
			Genode::print(out, "0x");
			bool leading_zero = true;
			for (char const c : values) {
				if (leading_zero) {
					if (c) {
						leading_zero = false;
						Genode::print(out, Hex(c, Hex::OMIT_PREFIX));
					}
				} else {
					Genode::print(out, Hex(c, Hex::OMIT_PREFIX, Hex::PAD));
				}
			}
			if (leading_zero) {
				Genode::print(out, "0");
			}
		}
	};

	struct Key_plaintext_value
	{
		enum { KEY_SIZE = 32 };
		char value[KEY_SIZE];
	};

	struct Key_ciphertext_value
	{
		enum { KEY_SIZE = 32 };
		char value[KEY_SIZE];
	};

	/*
	 * The Key contains the key-material that is used to
	 * process cipher-blocks.
	 *
	 * (For now it is not used but the ID field is already referenced
	 *  by type 2 nodes.)
	 */
	struct Key
	{
		enum { KEY_SIZE = 32 };
		char value[KEY_SIZE];

		struct Id { uint32_t value; };
		Id id;

		using String = Genode::String<sizeof(value) * 2 + 3>;

		void print(Genode::Output &out) const
		{
			using namespace Genode;
			Genode::print(out, "[", id.value, ", ");
			for (uint32_t i = 0; i < 4; i++) {
				Genode::print(out, Hex(value[i], Hex::OMIT_PREFIX, Hex::PAD));
			}
			Genode::print(out, "...]");
		}
	} __attribute__((packed));


	struct Active_snapshot_ids
	{
		uint64_t values[NR_OF_SNAPSHOTS];
	} __attribute__((packed));


	struct Info
	{
		bool valid;
		bool rekeying;
		bool extending_vbd;
		bool extending_ft;
	} __attribute__((packed));
}


inline char const *to_string(Cbe::Request::Operation op)
{
	struct Unknown_operation_type : Genode::Exception { };
	switch (op) {
	case Cbe::Request::Operation::INVALID: return "invalid";
	case Cbe::Request::Operation::READ: return "read";
	case Cbe::Request::Operation::WRITE: return "write";
	case Cbe::Request::Operation::SYNC: return "sync";
	case Cbe::Request::Operation::CREATE_SNAPSHOT: return "create_snapshot";
	case Cbe::Request::Operation::DISCARD_SNAPSHOT: return "discard_snapshot";
	case Cbe::Request::Operation::REKEY: return "rekey";
	case Cbe::Request::Operation::EXTEND_VBD: return "extend_vbd";
	case Cbe::Request::Operation::EXTEND_FT: return "extend_ft";
	case Cbe::Request::Operation::RESUME_REKEYING: return "resume_rekeying";
	case Cbe::Request::Operation::DEINITIALIZE: return "deinitialize";
	case Cbe::Request::Operation::INITIALIZE: return "initialize";
	}
	throw Unknown_operation_type();
}


inline char const *to_string(Cbe::Trust_anchor_request::Operation op)
{
	struct Unknown_operation_type : Genode::Exception { };
	switch (op) {
	case Cbe::Trust_anchor_request::Operation::INVALID: return "invalid";
	case Cbe::Trust_anchor_request::Operation::CREATE_KEY: return "create_key";
	case Cbe::Trust_anchor_request::Operation::INITIALIZE: return "initialize";
	case Cbe::Trust_anchor_request::Operation::SECURE_SUPERBLOCK: return "secure_superblock";
	case Cbe::Trust_anchor_request::Operation::ENCRYPT_KEY: return "encrypt_key";
	case Cbe::Trust_anchor_request::Operation::DECRYPT_KEY: return "decrypt_key";
	case Cbe::Trust_anchor_request::Operation::LAST_SB_HASH: return "last_sb_hash";
	}
	throw Unknown_operation_type();
}


inline void Cbe::Request::print(Genode::Output &out) const
{
	if (!valid()) {
		Genode::print(out, "<invalid>");
		return;
	}
	Genode::print(out, "op=", to_string (_operation));
	Genode::print(out, " vba=", _block_number);
	Genode::print(out, " cnt=", _count);
	Genode::print(out, " tag=", _tag);
	Genode::print(out, " key=", _key_id);
	Genode::print(out, " off=", _offset);
	Genode::print(out, " succ=", _success);
}

inline void Cbe::Trust_anchor_request::print(Genode::Output &out) const
{
	if (!valid()) {
		Genode::print(out, "<invalid>");
		return;
	}
	Genode::print(out, "op=", to_string (_operation));
	Genode::print(out, " tag=", _tag);
	Genode::print(out, " succ=", _success);
}
#endif /* _CBE_TYPES_H_ */
