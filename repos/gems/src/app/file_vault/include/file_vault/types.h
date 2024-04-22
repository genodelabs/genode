/*
 * \brief  Common types
 * \author Martin Stein
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_VAULT__TYPES_H_
#define _FILE_VAULT__TYPES_H_

/* Genode includes */
#include <tresor/types.h>

namespace File_vault {

	using namespace Tresor;
	using namespace Genode;

	static constexpr Tree_degree TRESOR_VBD_DEGREE = 64;
	static constexpr Tree_level_index TRESOR_VBD_MAX_LVL = 5;
	static constexpr Tree_degree TRESOR_FREE_TREE_DEGREE = 64;
	static constexpr Tree_level_index TRESOR_FREE_TREE_MAX_LVL = 5;
	static constexpr size_t MIN_CLIENT_FS_SIZE = 100 * 1024;
	static constexpr size_t MIN_PASSPHRASE_LENGTH = 8;
	static constexpr size_t MIN_CAPACITY = 100 * 1024;

	using Node_name = String<32>;
	using File_path = String<32>;
	using Child_name = String<128>;

	struct Number_of_clients { uint64_t value; };
	struct Operation_id { uint64_t value; };

	void gen_named_node(Xml_generator &xml, char const *type, auto name, auto const &fn)
	{
		xml.node(type, [&] {
			xml.attribute("name", name);
			fn(); });
	}

	inline size_t min_journal_buf(Number_of_bytes capacity)
	{
		size_t result { (size_t)capacity >> 8 };
		if (result < MIN_CAPACITY)
			result = MIN_CAPACITY;

		return result;
	}

	struct Ui_report
	{
		using State_string = String<32>;

		enum State {
			INVALID, UNINITIALIZED, INITIALIZING, LOCKED, UNLOCKING, UNLOCKED, LOCKING };

		static State_string state_to_string(State state)
		{
			switch (state) {
			case INVALID: return "invalid";
			case UNINITIALIZED: return "uninitialized";
			case INITIALIZING: return "initializing";
			case LOCKED: return "locked";
			case UNLOCKING: return "unlocking";
			case UNLOCKED: return "unlocked";
			case LOCKING: return "locking";
			}
			ASSERT_NEVER_REACHED;
		}

		static State string_to_state(State_string str)
		{
			if (str == "uninitialized") return UNINITIALIZED;
			if (str == "initializing") return INITIALIZING;
			if (str == "locked") return LOCKED;
			if (str == "unlocking") return UNLOCKING;
			if (str == "unlocked") return UNLOCKED;
			if (str == "locking") return LOCKING;
			return INVALID;
		}

		struct Rekey
		{
			Operation_id id;
			bool finished;

			Rekey(Xml_node const &node)
			: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

			Rekey(Operation_id id, bool finished) : id(id), finished(finished) { }

			void generate(Xml_generator &xml)
			{
				xml.attribute("id", id.value);
				xml.attribute("finished", finished);
			}
		};

		struct Extend
		{
			Operation_id id { };
			bool finished { };

			Extend(Xml_node const &node)
			: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

			Extend(Operation_id id, bool finished) : id(id), finished(finished) { }

			void generate(Xml_generator &xml)
			{
				xml.attribute("id", id.value);
				xml.attribute("finished", finished);
			}
		};

		State state { INVALID };
		Number_of_bytes image_size { };
		Number_of_bytes capacity { };
		Number_of_clients num_clients { };
		Constructible<Rekey> rekey { };
		Constructible<Extend> extend { };

		Ui_report() { }

		Ui_report(Xml_node const &node)
		:
			state(string_to_state(node.attribute_value("state", State_string()))),
			image_size(node.attribute_value("image_size", Number_of_bytes())),
			capacity(node.attribute_value("capacity", Number_of_bytes())),
			num_clients(node.attribute_value("num_clients", 0ULL))
		{
			node.with_optional_sub_node("rekey", [&] (Xml_node const &n) { rekey.construct(n); });
			node.with_optional_sub_node("extend", [&] (Xml_node const &n) { extend.construct(n); });
		}

		void generate(Xml_generator &xml)
		{
			xml.attribute("state", state_to_string(state));
			xml.attribute("image_size", image_size);
			xml.attribute("capacity", capacity);
			xml.attribute("num_clients", num_clients.value);
			if (rekey.constructed())
				xml.node("rekey", [&] { rekey->generate(xml); });
			if (extend.constructed())
				xml.node("extend", [&] { extend->generate(xml); });
		}
	};

	struct Ui_config
	{
		struct Extend
		{
			using Tree_string = String<4>;

			enum Tree { VIRTUAL_BLOCK_DEVICE, FREE_TREE };

			Operation_id id;
			Tree tree;
			Number_of_bytes num_bytes;

			static Tree string_to_tree(Tree_string const &str)
			{
				if (str == "vbd") return VIRTUAL_BLOCK_DEVICE;
				if (str == "ft") return FREE_TREE;
				ASSERT_NEVER_REACHED;
			}

			static Tree_string tree_to_string(Tree tree_arg)
			{
				switch (tree_arg) {
				case VIRTUAL_BLOCK_DEVICE: return "vbd";
				case FREE_TREE: return "ft"; }
				ASSERT_NEVER_REACHED;
			}

			Extend(Xml_node const &node)
			:
				id(node.attribute_value("id", 0ULL)),
				tree(string_to_tree(node.attribute_value("tree", Tree_string()))),
				num_bytes(node.attribute_value("num_bytes", 0UL))
			{ }

			Extend(Operation_id id, Tree tree, Number_of_bytes num_bytes) : id(id), tree(tree), num_bytes(num_bytes) { }

			void generate(Xml_generator &xml)
			{
				xml.attribute("id", id.value);
				xml.attribute("tree", tree_to_string(tree));
				xml.attribute("num_bytes", num_bytes);
			}
		};

		struct Rekey
		{
			Operation_id id;

			Rekey(Xml_node const &node) : id(node.attribute_value("id", 0ULL)) { }

			Rekey(Operation_id id) : id(id) { }

			void generate(Xml_generator &xml) { xml.attribute("id", id.value); }
		};

		Passphrase passphrase { };
		Number_of_bytes client_fs_size { };
		Number_of_bytes journaling_buf_size { };
		Constructible<Rekey> rekey { };
		Constructible<Extend> extend { };

		Ui_config(Xml_node const &node)
		:
			passphrase(node.attribute_value("passphrase", Passphrase())),
			client_fs_size(node.attribute_value("client_fs_size", Number_of_bytes())),
			journaling_buf_size(node.attribute_value("journaling_buf_size", Number_of_bytes()))
		{
			node.with_optional_sub_node("rekey", [&] (Xml_node const &n) { rekey.construct(n); });
			node.with_optional_sub_node("extend", [&] (Xml_node const &n) { extend.construct(n); });
		}

		Ui_config() { }

		void generate(Xml_generator &xml)
		{
			xml.attribute("passphrase", passphrase);
			xml.attribute("client_fs_size", client_fs_size);
			xml.attribute("journaling_buf_size", journaling_buf_size);
			if (rekey.constructed())
				xml.node("rekey", [&] { rekey->generate(xml); });
			if (extend.constructed())
				xml.node("extend", [&] { extend->generate(xml); });
		}

		bool passphrase_long_enough() const { return passphrase.length() >= MIN_PASSPHRASE_LENGTH + 1; }
	};

	inline Number_of_blocks tresor_tree_num_blocks(size_t num_lvls,
	                                                 size_t num_children,
	                                                 Number_of_leaves num_leaves)
	{
		Number_of_blocks num_blks { 0 };
		Number_of_blocks num_last_lvl_blks { num_leaves };
		for (size_t lvl_idx { 0 }; lvl_idx < num_lvls; lvl_idx++) {
			num_blks += num_last_lvl_blks;
			if (num_last_lvl_blks % num_children) {
				num_last_lvl_blks = num_last_lvl_blks / num_children + 1;
			} else {
				num_last_lvl_blks = num_last_lvl_blks / num_children;
			}
		}
		return num_blks;
	}

	inline Number_of_blocks tresor_num_blocks(Number_of_blocks num_superblocks,
	                                            size_t num_vbd_lvls,
	                                            size_t num_vbd_children,
	                                            Number_of_leaves num_vbd_leaves,
	                                            size_t num_ft_lvls,
	                                            size_t num_ft_children,
	                                            Number_of_leaves num_ft_leaves)
	{
		Number_of_blocks const num_vbd_blks {
			tresor_tree_num_blocks(num_vbd_lvls, num_vbd_children, num_vbd_leaves) };

		Number_of_blocks const num_ft_blks {
			tresor_tree_num_blocks(num_ft_lvls, num_ft_children, num_ft_leaves) };

		/* FIXME
		 *
		 * This would be the correct way to calculate the number of MT blocks
		 * but the Tresor still uses an MT the same size as the FT for simplicity
		 * reasons. As soon as the Tresor does it right we should fix also this path.
		 *
		 *	size_t const num_mt_leaves {
		 *		num_ft_blks - num_ft_leaves };
		 *
		 *	size_t const num_mt_blks {
		 *		_tree_num_blocks(
		 *			num_mt_lvls,
		 *			num_mt_children,
		 *			num_mt_leaves) };
		 */
		Number_of_blocks const num_mt_blks { num_ft_blks };

		return num_superblocks + num_vbd_blks + num_ft_blks + num_mt_blks;
	}

	inline Number_of_blocks tresor_tree_num_leaves(uint64_t payload_size)
	{
		Number_of_blocks num_leaves { payload_size / BLOCK_SIZE };
		if (payload_size % BLOCK_SIZE) {
			num_leaves++;
		}
		return num_leaves;
	}
}

#endif /* _FILE_VAULT__TYPES_H_ */
