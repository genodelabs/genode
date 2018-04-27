/*
 * \brief  Representation of block-device partition
 * \author Norman Feske
 * \date   2018-05-02
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__PARTITION_H_
#define _MODEL__PARTITION_H_

#include "types.h"
#include "capacity.h"

namespace Sculpt {

	struct File_system;
	struct Partition;
	struct Partition_update_policy;

	typedef List_model<Partition> Partitions;
};


struct Sculpt::File_system
{
	enum Type { UNKNOWN, EXT2, FAT32 } type;

	bool accessible() const { return type == EXT2 || type == FAT32; }

	bool expandable() const { return type == EXT2; }

	File_system(Type type) : type(type) { }
};


struct Sculpt::Partition : List_model<Partition>::Element
{
	typedef String<16> Number;
	typedef String<32> Label;

	enum Expandable { FIXED_SIZE, EXPANDABLE };

	Number     const number;
	Label      const label;
	Capacity   const capacity;

	Expandable const _expandable;

	Label next_label = label; /* used to set/unset the default partition */

	File_system file_system;

	bool check_in_progress      = false;
	bool format_in_progress     = false;
	bool file_system_inspected  = false;
	bool gpt_expand_in_progress = false;
	bool fs_resize_in_progress  = false;

	bool relabel_in_progress() const { return label != next_label; }

	bool expand_in_progress() const { return gpt_expand_in_progress
	                                      || fs_resize_in_progress; }

	bool checkable() const { return file_system.type == File_system::EXT2; }

	bool expandable() const
	{
		return file_system.expandable() && _expandable == EXPANDABLE;
	}

	bool idle() const { return !check_in_progress
	                        && !format_in_progress
	                        && !file_system_inspected
	                        && !relabel_in_progress(); }

	bool genode_default() const { return label == "GENODE*"; }
	bool genode()         const { return label == "GENODE" || genode_default(); }

	void toggle_default_label()
	{
		if (label == "GENODE")  next_label = Label("GENODE*");
		if (label == "GENODE*") next_label = Label("GENODE");
	}

	struct Args
	{
		Number            number;
		Label             label;
		Capacity          capacity;
		Expandable        expandable;
		File_system::Type fs_type;

		static Args from_xml(Xml_node node)
		{
			auto const file_system = node.attribute_value("file_system", String<16>());
			File_system::Type const fs_type = (file_system == "Ext2")  ? File_system::EXT2
			                                : (file_system == "FAT32") ? File_system::FAT32
			                                :                            File_system::UNKNOWN;

			Number const number = node.attribute_value("number", Number());

			unsigned long const block_size =
				node.attribute_value("block_size", 512ULL);

			unsigned long long const expandable =
				node.attribute_value("expandable", 0ULL);

			return Args { number == "0" ? Number{} : number,
			              node.attribute_value("name", Label()),
			              Capacity { node.attribute_value("length", 0ULL)*block_size },
			              (expandable*block_size > 1024*1024) ? EXPANDABLE : FIXED_SIZE,
			              fs_type };
		}

		static Args whole_device(Capacity capacity)
		{
			return { Number{}, "", capacity, FIXED_SIZE, File_system::UNKNOWN };
		};
	};

	Partition(Args const &args)
	:
		number(args.number), label(args.label), capacity(args.capacity),
		_expandable(args.expandable), file_system(args.fs_type)
	{ }

	bool whole_device() const { return !number.valid(); }
};


/**
 * Policy for transforming a part_blk report into a list of partitions
 */
struct Sculpt::Partition_update_policy : List_model<Partition>::Update_policy
{
	Allocator &_alloc;

	Partition_update_policy(Allocator &alloc) : _alloc(alloc) { }

	void destroy_element(Partition &elem) { destroy(_alloc, &elem); }

	Partition &create_element(Xml_node node)
	{
		return *new (_alloc) Partition(Partition::Args::from_xml(node));
	}

	void update_element(Partition &, Xml_node) { }

	static bool node_is_element(Xml_node node)
	{
		/*
		 * Partition "0" is a pseudo partition that refers to the whole device
		 * with no partition table.
		 */
		return (node.attribute_value("number", Partition::Number()) != "0");
	}

	static bool element_matches_xml_node(Partition const &elem, Xml_node node)
	{
		return node.attribute_value("number", Partition::Number()) == elem.number;
	}
};

#endif /* _MODEL__PARTITION_H_ */
