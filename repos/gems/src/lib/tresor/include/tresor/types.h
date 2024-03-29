/*
 * \brief  Basic types, functions, enums used throughout the Tresor ecosystem
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__TYPES_H_
#define _TRESOR__TYPES_H_

/* base includes */
#include <base/output.h>
#include <util/string.h>

/* tresor includes */
#include <tresor/verbosity.h>
#include <tresor/math.h>
#include <tresor/assertion.h>

namespace Tresor {

	using namespace Genode;

	using Physical_block_address = uint64_t;
	using Virtual_block_address  = uint64_t;
	using Generation             = uint64_t;
	using Generation_string      = String<21>;
	using Number_of_leaves       = uint64_t;
	using Number_of_blocks       = uint64_t;
	using Tree_level_index       = uint32_t;
	using Tree_node_index        = uint64_t;
	using Tree_degree            = uint32_t;
	using Tree_degree_log_2      = uint32_t;
	using Key_id                 = uint32_t;
	using Snapshot_id            = uint32_t;
	using Snapshot_index         = uint32_t;
	using Superblock_index       = uint8_t;
	using On_disc_bool           = uint8_t;

	enum { BLOCK_SIZE = 4096 };
	enum { INVALID_KEY_ID = 0 };
	enum { INVALID_GENERATION = 0 };
	enum { INITIAL_GENERATION = 0 };
	enum { MAX_PBA = 0xffff'ffff'ffff'ffff };
	enum { INVALID_PBA = MAX_PBA };
	enum { INVALID_NODE_INDEX = 0xff };
	enum { MAX_GENERATION = 0xffff'ffff'ffff'ffff };
	enum { MAX_SNAP_ID = 0xffff'ffff };
	enum { HASH_SIZE = 32 };
	enum { T1_NODE_STORAGE_SIZE = 64 };
	enum { T2_NODE_STORAGE_SIZE = 64 };
	enum { NR_OF_T2_NODES_PER_BLK = (size_t)BLOCK_SIZE / (size_t)T2_NODE_STORAGE_SIZE };
	enum { NR_OF_T1_NODES_PER_BLK = (size_t)BLOCK_SIZE / (size_t)T1_NODE_STORAGE_SIZE };
	enum { TREE_MAX_DEGREE_LOG_2 = 6 };
	enum { TREE_MAX_DEGREE = 1 << TREE_MAX_DEGREE_LOG_2 };
	enum { TREE_MAX_LEVEL = 6 };
	enum { TREE_MAX_NR_OF_LEVELS = TREE_MAX_LEVEL + 1 };
	enum { T2_NODE_LVL = 1 };
	enum { VBD_LOWEST_T1_LVL = 1 };
	enum { FT_LOWEST_T1_LVL = 2 };
	enum { MT_LOWEST_T1_LVL = 2 };
	enum { KEY_SIZE = 32 };
	enum { MAX_NR_OF_SNAPSHOTS = 48 };
	enum { MAX_SNAP_IDX = MAX_NR_OF_SNAPSHOTS - 1 };
	enum { INVALID_SNAP_IDX = MAX_NR_OF_SNAPSHOTS };
	enum { SNAPSHOT_STORAGE_SIZE = 72 };
	enum { NR_OF_SUPERBLOCK_SLOTS = 8 };
	enum { MAX_SUPERBLOCK_INDEX = NR_OF_SUPERBLOCK_SLOTS - 1 };
	enum { FREE_TREE_MIN_MAX_LEVEL = 2 };
	enum { TREE_MAX_NR_OF_LEAVES = to_the_power_of<Number_of_leaves>(TREE_MAX_DEGREE, (TREE_MAX_LEVEL - 1)) };
	enum { INVALID_VBA = to_the_power_of<Virtual_block_address>(TREE_MAX_DEGREE, TREE_MAX_LEVEL - 1) };
	enum { TREE_MIN_DEGREE = 1 };

	struct Byte_range;
	struct Key_value;
	struct Key;
	struct Hash;
	struct Block;
	struct Block_scanner;
	struct Block_generator;
	struct Superblock;
	struct Superblock_info;
	struct Snapshot;
	struct Snapshot_generations;
	struct Snapshots;
	struct Type_1_node;
	struct Type_1_node_block;
	struct Type_1_node_walk;
	struct Type_2_node;
	struct Type_2_node_block;
	struct Tree_walk_pbas;
	struct Level_indent;

	constexpr Virtual_block_address tree_max_max_vba(Tree_degree      degree,
	                                                 Tree_level_index max_lvl)
	{
		return to_the_power_of<Virtual_block_address>(degree, max_lvl) - 1;
	}

	inline Physical_block_address
	alloc_pba_from_resizing_contingent(Physical_block_address &first_pba,
	                                   Number_of_blocks       &nr_of_pbas)
	{
		if (nr_of_pbas == 0) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		Physical_block_address const allocated_pba { first_pba };
		first_pba  = first_pba  + 1;
		nr_of_pbas = nr_of_pbas - 1;
		return allocated_pba;
	}

	inline Tree_node_index
	t1_child_idx_for_vba_typed(Virtual_block_address vba,
	                           Tree_level_index      lvl,
	                           Tree_degree           degr)
	{
		uint64_t const degr_log_2 { log2(degr) };
		uint64_t const degr_mask  { ((uint64_t)1 << degr_log_2) - 1 };
		uint64_t const vba_rshift { degr_log_2 * ((uint64_t)lvl - 1) };
		return (Tree_node_index)(degr_mask & (vba >> vba_rshift));
	}

	template <typename T1, typename T2, typename T3>
	inline Tree_node_index t1_child_idx_for_vba(T1 vba,
	                                       T2 lvl,
	                                       T3 degr)
	{
		return t1_child_idx_for_vba_typed((Virtual_block_address)vba,
		                                  (Tree_level_index)lvl,
		                                  (Tree_degree)degr);
	}

	inline Tree_node_index t2_child_idx_for_vba(Virtual_block_address vba,
	                                       Tree_degree           degr)
	{
		uint64_t const degr_log_2 { log2(degr) };
		uint64_t const degr_mask  { ((uint64_t)1 << degr_log_2) - 1 };
		return (Tree_node_index)((uint64_t)vba & degr_mask);
	}
}


struct Tresor::Byte_range
{
	uint8_t const *ptr;
	size_t         size;

	void print(Output &out) const
	{
		using Genode::print;

		enum { MAX_BYTES_PER_LINE = 64 };
		enum { MAX_BYTES_PER_WORD = 4 };

		if (size > 0xffff) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		if (size > MAX_BYTES_PER_LINE) {

			for (size_t idx { 0 }; idx < size; idx++) {

				if (idx % MAX_BYTES_PER_LINE == 0)
					print(out, "\n  ",
					      Hex((uint16_t)idx, Hex::PREFIX, Hex::PAD), ": ");

				else if (idx % MAX_BYTES_PER_WORD == 0)
					print(out, " ");

				print(out, Hex(ptr[idx], Hex::OMIT_PREFIX, Hex::PAD));
			}

		} else {

			for (size_t idx { 0 }; idx < size; idx++) {

				if (idx % MAX_BYTES_PER_WORD == 0 && idx != 0)
					print(out, " ");

				print(out, Hex(ptr[idx], Hex::OMIT_PREFIX, Hex::PAD));
			}
		}
	}
};


struct Tresor::Superblock_info
{
	bool valid         { false };
	bool rekeying      { false };
	bool extending_vbd { false };
	bool extending_ft  { false };
};


struct Tresor::Key_value
{
	uint8_t bytes[KEY_SIZE];

	void print(Output &out) const
	{
		Genode::print(out, Byte_range { bytes, KEY_SIZE });
	}
};


struct Tresor::Hash
{
	uint8_t bytes[HASH_SIZE] { 0 };

	void print(Output &out) const
	{
		Genode::print(out, Byte_range { bytes, 4 }, "…");
	}

	bool operator == (Hash const &other) const
	{
		return !memcmp(bytes, other.bytes, sizeof(bytes));
	}

	bool operator != (Hash const &other) const
	{
		return !(*this == other);
	}
};


struct Tresor::Block
{
	uint8_t bytes[BLOCK_SIZE] { 0 };

	void print(Output &out) const
	{
		Genode::print(out, Byte_range { bytes, 16 }, "…");
	}
};


class Tresor::Block_scanner
{
	private:

		Block const &_blk;
		size_t       _offset { 0 };

		void const *_current_position() const
		{
			return (void const *)((addr_t)&_blk + _offset);
		}

		void _advance_position(size_t num_bytes)
		{
			ASSERT(_offset <= sizeof(_blk) - num_bytes);
			_offset += num_bytes;
		}

		template<typename T>
		void _fetch_copy(T &dst)
		{
			void const *blk_pos { _current_position() };
			_advance_position(sizeof(dst));
			memcpy(&dst, blk_pos, sizeof(dst));
		}

		static bool _decode_bool(On_disc_bool val)
		{
			switch (val) {
			case 0: return false;
			case 1: return true;
			default: break;
			}
			ASSERT_NEVER_REACHED;
		}

	public:

		Block_scanner(Block const &blk)
		:
			_blk { blk }
		{ }

		template<typename T>
		void fetch(T &dst);

		template<typename T>
		T fetch_value()
		{
			T dst;
			fetch(dst);
			return dst;
		}

		void skip_bytes(size_t num_bytes)
		{
			_advance_position(num_bytes);
		}

		~Block_scanner()
		{
			ASSERT(_offset == sizeof(_blk));
		}
};


template <> inline void Tresor::Block_scanner::fetch<bool>(bool &dst) { dst = _decode_bool(fetch_value<On_disc_bool>()); }
template <> inline void Tresor::Block_scanner::fetch<Genode::uint8_t>(uint8_t &dst) { _fetch_copy(dst); }
template <> inline void Tresor::Block_scanner::fetch<Genode::uint16_t>(uint16_t &dst) { _fetch_copy(dst); }
template <> inline void Tresor::Block_scanner::fetch<Genode::uint32_t>(uint32_t &dst) { _fetch_copy(dst); }
template <> inline void Tresor::Block_scanner::fetch<Genode::uint64_t>(uint64_t &dst) { _fetch_copy(dst); }
template <> inline void Tresor::Block_scanner::fetch<Tresor::Hash>(Hash &dst) { _fetch_copy(dst); }
template <> inline void Tresor::Block_scanner::fetch<Tresor::Key_value>(Key_value &dst) { _fetch_copy(dst); }


class Tresor::Block_generator
{
	private:

		Block  &_blk;
		size_t  _offset { 0 };

		void *_current_position() const
		{
			return (void *)((addr_t)&_blk + _offset);
		}

		void _advance_position(size_t num_bytes)
		{
			ASSERT(_offset <= sizeof(_blk) - num_bytes);
			_offset += num_bytes;
		}

		template<typename T>
		void _append_copy(T const &src)
		{
			void *blk_pos { _current_position() };
			_advance_position(sizeof(src));
			memcpy(blk_pos, &src, sizeof(src));
		}

		static On_disc_bool _encode_bool(bool val)
		{
			return val ? 1 : 0;
		}

	public:

		Block_generator(Block &blk)
		:
			_blk { blk }
		{ }

		template<typename T>
		void append(T const &src);

		template<typename T>
		void append_value(T src)
		{
			append(src);
		}

		void append_zero_bytes(size_t num_bytes)
		{
			void *blk_pos { _current_position() };
			_advance_position(num_bytes);
			memset(blk_pos, 0, num_bytes);
		}

		~Block_generator()
		{
			ASSERT(_offset == sizeof(_blk));
		}
};


template <> inline void Tresor::Block_generator::append<bool>(bool const &src) { append_value(_encode_bool(src)); }
template <> inline void Tresor::Block_generator::append<Genode::uint8_t>(uint8_t const &src) { _append_copy(src); }
template <> inline void Tresor::Block_generator::append<Genode::uint16_t>(uint16_t const &src) { _append_copy(src); }
template <> inline void Tresor::Block_generator::append<Genode::uint32_t>(uint32_t const &src) { _append_copy(src); }
template <> inline void Tresor::Block_generator::append<Genode::uint64_t>(uint64_t const &src) { _append_copy(src); }
template <> inline void Tresor::Block_generator::append<Tresor::Hash>(Hash const &src) { _append_copy(src); }
template <> inline void Tresor::Block_generator::append<Tresor::Key_value>(Key_value const &src) { _append_copy(src); }


struct Tresor::Key
{
	Key_value value { };
	Key_id    id    { INVALID_KEY_ID };

	void decode_from_blk(Block_scanner &scanner)
	{
		scanner.fetch(value);
		scanner.fetch(id);
	}

	void encode_to_blk(Block_generator &generator) const
	{
		generator.append(value);
		generator.append(id);
	}
};


struct Tresor::Type_1_node
{
	Physical_block_address pba  { 0 };
	Generation             gen  { 0 };
	Hash                   hash { };

	void decode_from_blk(Block_scanner &scanner)
	{
		scanner.fetch(pba);
		scanner.fetch(gen);
		scanner.fetch(hash);
		scanner.skip_bytes(16);
	}

	void encode_to_blk(Block_generator &generator) const
	{
		generator.append(pba);
		generator.append(gen);
		generator.append(hash);
		generator.append_zero_bytes(16);
	}

	bool valid() const
	{
		Type_1_node const node { };
		return
			pba  != node.pba ||
			gen  != node.gen ||
			hash != node.hash;
	}

	void print(Output &out) const
	{
		Genode::print(out, "pba ", pba, " gen ", gen, " hash ", hash);
	}
};


struct Tresor::Type_1_node_block
{
	Type_1_node nodes[NR_OF_T1_NODES_PER_BLK] { };

	void decode_from_blk(Block const &blk)
	{
		Block_scanner scanner { blk };
		for (Type_1_node &node : nodes)
			node.decode_from_blk(scanner);
	}

	void encode_to_blk(Block &blk) const
	{
		Block_generator generator { blk };
		for (Type_1_node const &node : nodes)
			node.encode_to_blk(generator);
	}
};


struct Tresor::Type_2_node
{
	Physical_block_address pba         { 0 };
	Virtual_block_address  last_vba    { 0 };
	Generation             alloc_gen   { 0 };
	Generation             free_gen    { 0 };
	Key_id                 last_key_id { 0 };
	bool                   reserved    { false };

	void decode_from_blk(Block_scanner &scanner)
	{
		scanner.fetch(pba);
		scanner.fetch(last_vba);
		scanner.fetch(alloc_gen);
		scanner.fetch(free_gen);
		scanner.fetch(last_key_id);
		scanner.fetch(reserved);
		scanner.skip_bytes(27);
	}

	void encode_to_blk(Block_generator &generator) const
	{
		generator.append(pba);
		generator.append(last_vba);
		generator.append(alloc_gen);
		generator.append(free_gen);
		generator.append(last_key_id);
		generator.append(reserved);
		generator.append_zero_bytes(27);
	}

	bool valid() const
	{
		Type_2_node const node { };
		return
			pba         != node.pba ||
			last_vba    != node.last_vba ||
			alloc_gen   != node.alloc_gen ||
			free_gen    != node.free_gen ||
			last_key_id != node.last_key_id ||
			reserved    != node.reserved;
	}

	void print(Output &out) const
	{
		Genode::print(
			out, "pba ", pba, " last_vba ", last_vba, " alloc_gen ",
			alloc_gen, " free_gen ", free_gen, " last_key ", last_key_id);
	}
};


struct Tresor::Type_2_node_block
{
	Type_2_node nodes[NR_OF_T2_NODES_PER_BLK] { };

	void decode_from_blk(Block const &blk)
	{
		Block_scanner scanner { blk };
		for (Type_2_node &node : nodes)
			node.decode_from_blk(scanner);
	}

	void encode_to_blk(Block &blk) const
	{
		Block_generator generator { blk };
		for (Type_2_node const &node : nodes)
			node.encode_to_blk(generator);
	}
};


struct Tresor::Snapshot
{
	Hash                   hash         { };
	Physical_block_address pba          { INVALID_PBA };
	Generation             gen          { MAX_GENERATION };
	Number_of_leaves       nr_of_leaves { TREE_MAX_NR_OF_LEAVES };
	Tree_level_index       max_level    { TREE_MAX_LEVEL };
	bool                   valid        { false };
	Snapshot_id            id           { MAX_SNAP_ID };
	bool                   keep         { false };

	void decode_from_blk(Block_scanner &scanner)
	{
		scanner.fetch(hash);
		scanner.fetch(pba);
		scanner.fetch(gen);
		scanner.fetch(nr_of_leaves);
		scanner.fetch(max_level);
		scanner.fetch(valid);
		scanner.fetch(id);
		scanner.fetch(keep);
		scanner.skip_bytes(6);
	}

	void encode_to_blk(Block_generator &generator) const
	{
		generator.append(hash);
		generator.append(pba);
		generator.append(gen);
		generator.append(nr_of_leaves);
		generator.append(max_level);
		generator.append(valid);
		generator.append(id);
		generator.append(keep);
		generator.append_zero_bytes(6);
	}

	void print(Output &out) const
	{
		if (valid)
			Genode::print(
				out, "pba ", (Physical_block_address)pba, " gen ",
				(Generation)gen, " hash ", hash, " leaves ", nr_of_leaves,
				" max_lvl ", max_level);
		else
			Genode::print(out, "<invalid>");
	}

	bool contains_vba(Virtual_block_address vba) const
	{
		return vba <= nr_of_leaves - 1;
	}
};


struct Tresor::Snapshots
{
	Snapshot items[MAX_NR_OF_SNAPSHOTS];

	void decode_from_blk(Block_scanner &scanner)
	{
		for (Snapshot &snap : items)
			snap.decode_from_blk(scanner);
	}

	void encode_to_blk(Block_generator &generator) const
	{
		for (Snapshot const &snap : items)
			snap.encode_to_blk(generator);
	}

	void discard_disposable_snapshots(Generation curr_gen,
	                                  Generation last_secured_gen)
	{
		for (Snapshot &snap : items) {

			if (snap.valid &&
			    !snap.keep &&
			    snap.gen != curr_gen &&
			    snap.gen != last_secured_gen)

				snap.valid = false;
		}
	}

	Snapshot_index newest_snapshot_idx() const
	{
		Snapshot_index result { INVALID_SNAP_IDX };
		for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx ++) {

			Snapshot const &snap { items[idx] };
			if (!snap.valid)
				continue;

			if (result != INVALID_SNAP_IDX &&
			    snap.gen <= items[result].gen)
				continue;

			result = idx;
		}
		if (result != INVALID_SNAP_IDX)
			return result;

		class Exception_1 { };
		throw Exception_1 { };
	}

	Snapshot_index
	idx_of_invalid_or_lowest_gen_evictable_snap(Generation curr_gen,
	                                            Generation last_secured_gen) const
	{
		Snapshot_index result { INVALID_SNAP_IDX };
		for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx ++) {

			Snapshot const &snap { items[idx] };
			if (!snap.valid)
				return idx;

			if (snap.keep ||
			    snap.gen == curr_gen ||
			    snap.gen == last_secured_gen)
				continue;

			if (result != INVALID_SNAP_IDX &&
			    snap.gen >= items[result].gen)
				continue;

			result = idx;
		}
		if (result != INVALID_SNAP_IDX)
			return result;

		class Exception_1 { };
		throw Exception_1 { };
	}
};


struct Tresor::Superblock
{
	using On_disc_state = uint8_t;

	enum State {
		INVALID, NORMAL, REKEYING, EXTENDING_VBD, EXTENDING_FT };

	State                  state                   { INVALID };         // offset 0
	Virtual_block_address  rekeying_vba            { 0 };               // offset 1
	Number_of_blocks       resizing_nr_of_pbas     { 0 };               // offset 9
	Number_of_leaves       resizing_nr_of_leaves   { 0 };               // offset 17
	Key                    previous_key            { };                 // offset 25
	Key                    current_key             { };                 // offset 61
	Snapshots              snapshots               { };                 // offset 97
	Generation             last_secured_generation { };                 // offset 3553
	Snapshot_index         curr_snap               { };                 // offset 3561
	Tree_degree            degree                  { TREE_MIN_DEGREE }; // offset 3565
	Physical_block_address first_pba               { 0 };               // offset 3569
	Number_of_blocks       nr_of_pbas              { 0 };               // offset 3577
	Generation             free_gen                { 0 };               // offset 3585
	Physical_block_address free_number             { 0 };               // offset 3593
	Hash                   free_hash               { 0 };               // offset 3601
	Tree_level_index       free_max_level          { 0 };               // offset 3633
	Tree_degree            free_degree             { TREE_MIN_DEGREE }; // offset 3637
	Number_of_leaves       free_leaves             { 0 };               // offset 3641
	Generation             meta_gen                { 0 };               // offset 3649
	Physical_block_address meta_number             { 0 };               // offset 3657
	Hash                   meta_hash               { 0 };               // offset 3665
	Tree_level_index       meta_max_level          { 0 };               // offset 3697
	Tree_degree            meta_degree             { TREE_MIN_DEGREE }; // offset 3701
	Number_of_leaves       meta_leaves             { 0 };               // offset 3705
	                                                                    // offset 3713

	static State decode_state(On_disc_state val)
	{
		switch (val) {
		case 0: return INVALID;
		case 1: return NORMAL;
		case 2: return REKEYING;
		case 3: return EXTENDING_VBD;
		case 4: return EXTENDING_FT;
		default: break;
		}
		ASSERT_NEVER_REACHED;
	}

	static On_disc_state encode_state(State val)
	{
		switch (val) {
		case INVALID      : return 0;
		case NORMAL       : return 1;
		case REKEYING     : return 2;
		case EXTENDING_VBD: return 3;
		case EXTENDING_FT : return 4;
		default: break;
		}
		ASSERT_NEVER_REACHED;
	}

	void decode_from_blk(Block const &blk)
	{
		Block_scanner scanner { blk };
		state = decode_state(scanner.fetch_value<On_disc_state>());
		scanner.fetch(rekeying_vba);
		scanner.fetch(resizing_nr_of_pbas);
		scanner.fetch(resizing_nr_of_leaves);
		previous_key.decode_from_blk(scanner);
		current_key.decode_from_blk(scanner);
		snapshots.decode_from_blk(scanner);
		scanner.fetch(last_secured_generation);
		scanner.fetch(curr_snap);
		scanner.fetch(degree);
		scanner.fetch(first_pba);
		scanner.fetch(nr_of_pbas);
		scanner.fetch(free_gen);
		scanner.fetch(free_number);
		scanner.fetch(free_hash);
		scanner.fetch(free_max_level);
		scanner.fetch(free_degree);
		scanner.fetch(free_leaves);
		scanner.fetch(meta_gen);
		scanner.fetch(meta_number);
		scanner.fetch(meta_hash);
		scanner.fetch(meta_max_level);
		scanner.fetch(meta_degree);
		scanner.fetch(meta_leaves);
		scanner.skip_bytes(383);
	}

	void encode_to_blk(Block &blk) const
	{
		Block_generator generator { blk };
		generator.append_value(encode_state(state));
		generator.append(rekeying_vba);
		generator.append(resizing_nr_of_pbas);
		generator.append(resizing_nr_of_leaves);
		previous_key.encode_to_blk(generator);
		current_key.encode_to_blk(generator);
		snapshots.encode_to_blk(generator);
		generator.append(last_secured_generation);
		generator.append(curr_snap);
		generator.append(degree);
		generator.append(first_pba);
		generator.append(nr_of_pbas);
		generator.append(free_gen);
		generator.append(free_number);
		generator.append(free_hash);
		generator.append(free_max_level);
		generator.append(free_degree);
		generator.append(free_leaves);
		generator.append(meta_gen);
		generator.append(meta_number);
		generator.append(meta_hash);
		generator.append(meta_max_level);
		generator.append(meta_degree);
		generator.append(meta_leaves);
		generator.append_zero_bytes(383);
	}

	bool valid() const { return state != INVALID; }

	char const *state_to_str(State state) const
	{
		switch (state) {
		case INVALID:       return "INVALID";
		case NORMAL:        return "NORMAL";
		case REKEYING:      return "REKEYING";
		case EXTENDING_VBD: return "EXTENDING_VBD";
		case EXTENDING_FT:  return "EXTENDING_FT"; }
	}

	void print(Output &out) const
	{
		Genode::print(
			out, "state ", state_to_str(state), " last_secured_gen ",
			last_secured_generation, " curr_snap ", curr_snap, " degr ",
			degree, " first_pba ", first_pba, " pbas ", nr_of_pbas,
			" snapshots");

		for (Snapshot const &snap : snapshots.items)
			if (snap.valid)
				Genode::print(out, " ", snap);
	}
};


struct Tresor::Type_1_node_walk
{
	Type_1_node nodes[TREE_MAX_NR_OF_LEVELS] { };
};


struct Tresor::Tree_walk_pbas
{
	Physical_block_address pbas[TREE_MAX_NR_OF_LEVELS] { 0 };
};


struct Tresor::Snapshot_generations
{
	Generation items[MAX_NR_OF_SNAPSHOTS] { 0 };
};


struct Tresor::Level_indent
{
	Tree_level_index lvl;
	Tree_level_index max_lvl;

	void print(Output &out) const
	{
		for (Tree_level_index i { 0 }; i < max_lvl + 1 - lvl; i++)
			Genode::print(out, "  ");
	}
};


#endif /* _TRESOR__TYPES_H_ */
