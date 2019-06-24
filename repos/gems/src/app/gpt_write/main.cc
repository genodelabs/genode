/*
 * \brief  GPT partitioning tool
 * \author Josef Soentgen
 * \date   2018-05-01
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>

/* local includes */
#include <gpt.h>
#include <pmbr.h>


namespace Gpt {

	struct Writer;
}

struct Gpt::Writer
{
	struct Io_error    : Genode::Exception { };
	struct Gpt_invalid : Genode::Exception { };

	using sector_t = Block::sector_t;

	Block::Connection<> &_block;

	Block::Session::Info const _info        { _block.info() };
	size_t               const _block_size  { _info.block_size };
	sector_t             const _block_count { _info.block_count };

	/*
	 * Blocks available is a crude approximation that _does not_ take
	 * alignment or unusable blocks because of the layout into account!
	 */
	uint64_t _blocks_avail { 0 };

	/* track actions */
	bool _new_gpt      { false };
	bool _new_pmbr     { false };
	bool _new_geometry { false };

	/* flags */
	bool   _verbose         { false };
	bool   _update_geometry { false };
	bool   _preserve_hybrid { false };
	bool   _initialize      { false };
	bool   _wipe            { false };
	bool   _force_alignment { false };

	Util::Number_of_bytes _entry_alignment { 4096u };

	void _handle_config(Genode::Xml_node config)
	{
		_verbose         = config.attribute_value("verbose",         false);
		_initialize      = config.attribute_value("initialize",      false);
		_wipe            = config.attribute_value("wipe",            false);
		_force_alignment = config.attribute_value("force_align",     false);
		_update_geometry = config.attribute_value("update_geometry", false);
		_preserve_hybrid = config.attribute_value("preserve_hybrid", false);

		{
			Util::Size_string align =
				config.attribute_value("align", Util::Size_string(4096u));
			ascii_to(align.string(), _entry_alignment);
		}

		bool const commands = config.has_sub_node("commands");

		if (_wipe && (_initialize || commands)) {
			Genode::warning("will exit after wiping");
		}
	}

	/************
	 ** Tables **
	 ************/

	Protective_mbr::Header _pmbr { };

	Gpt::Header _pgpt                      { };
	Gpt::Entry  _pgpt_entries[MAX_ENTRIES] { };

	Gpt::Header _bgpt                      { };
	Gpt::Entry  _bgpt_entries[MAX_ENTRIES] { };

	Block::sector_t _old_backup_hdr_lba { 0 };

	/**
	 * Fill in-memory GPT header/entries and valid
	 *
	 * \param primary  if set to true the primary GPT is checked, if false
	 *                 the backup header is checked
	 *
	 * \throw Io_error
	 */
	void _fill_and_check_header(bool primary)
	{
		using namespace Genode;

		Gpt::Header *hdr     = primary ? &_pgpt : &_bgpt;
		Gpt::Entry  *entries = primary ? _pgpt_entries : _bgpt_entries;

		memset(hdr,     0, sizeof(Gpt::Header));
		memset(entries, 0, ENTRIES_SIZE);

		try {
			/* header */
			{
				Block::sector_t const lba = primary
				                          ? 1 : _pgpt.backup_lba;
				Util::Block_io io(_block, _block_size, lba, 1);
				memcpy(hdr, io.addr<Gpt::Header*>(), sizeof(Gpt::Header));

				if (!hdr->valid(primary)) {
					error(primary ? "primary" : "backup",
					      " GPT header not valid");
					throw Gpt_invalid();
				}
			}

			/* entries */
			{
				uint32_t const max_entries = hdr->gpe_num > (uint32_t)MAX_ENTRIES
				                           ? (uint32_t)MAX_ENTRIES : hdr->gpe_num;
				Block::sector_t const lba   = hdr->gpe_lba;
				size_t          const count = max_entries * hdr->gpe_size / _block_size;

				Util::Block_io io(_block, _block_size, lba, count);
				size_t const len = count * _block_size;
				memcpy(entries, io.addr<void const*>(), len);
			}
		} catch (Util::Block_io::Io_error) {
			error("could not read GPT header/entries");
			throw Io_error();
		}

		if (!Gpt::valid(*hdr, entries, primary)) {
			error("GPT header and entries not valid");
			throw Gpt_invalid();
		}
	}

	/**
	 * Wipe old backup GPT header from Block device
	 */
	bool _wipe_old_backup_header()
	{
		enum { BLOCK_SIZE = 4096u, };
		if (_block_size > BLOCK_SIZE) {
			Genode::error("block size of ", _block_size, "not supported");
			return false;
		}

		char zeros[BLOCK_SIZE] { };

		size_t const blocks = 1 + (ENTRIES_SIZE / _block_size);
		Block::sector_t const lba = _old_backup_hdr_lba - blocks;

		using namespace Util;

		try {
			Block_io clear(_block, _block_size, lba, blocks, true, zeros, _block_size);
		} catch (Block_io::Io_error) { return false; }

		return true;
	}

	/**
	 * Wipe all tables from Block device
	 *
	 * Note: calling this method actually destroys old data!
	 */
	bool _wipe_tables()
	{
		enum { BLOCK_SIZE = 4096u, };
		if (_block_size > BLOCK_SIZE) {
			Genode::error("block size of ", _block_size, "not supported");
			return false;
		}

		char zeros[BLOCK_SIZE] { };

		using namespace Util;

		try {
			/* PMBR */
			Block_io clear_mbr(_block, _block_size, 0, 1, true, zeros, _block_size);
			size_t const blocks = 1 + (Gpt::ENTRIES_SIZE / _block_size);

			/* PGPT */
			for (size_t i = 0; i < blocks; i++) {
				Block_io clear_lba(_block, _block_size, 1 + i, 1,
				                   true, zeros, _block_size);
			}

			/* BGPT */
			for (size_t i = 0; i < blocks; i++) {
				Block_io clear_lba(_block, _block_size, (_block_count - 1) - i, 1,
				                   true, zeros, _block_size);
			}

			return true;
		} catch (Block_io::Io_error) { return false; }
	}

	/**
	 * Setup protective MBR
	 *
	 * The first protective partition covers the whole Block device from the
	 * second block up to the 32bit boundary.
	 */
	void _setup_pmbr()
	{
		_pmbr.partitions[0].type    = Protective_mbr::TYPE_PROTECTIVE;
		_pmbr.partitions[0].lba     = 1;
		_pmbr.partitions[0].sectors = (uint32_t)(_block_count - 1);

		_new_pmbr = true;
	}

	/**
	 * Initialize tables
	 *
	 * All tables, primary GPT and backup GPT as well as the protective MBR
	 * will be cleared in memory, a new disk GUID will be generated and the
	 * default values will be set.
	 */
	void _initialize_tables()
	{
		_setup_pmbr();

		/* wipe PGPT and BGPT */
		Genode::memset(&_pgpt, 0, sizeof(_pgpt));
		Genode::memset(_pgpt_entries, 0, sizeof(_pgpt_entries));
		Genode::memset(&_bgpt,  0, sizeof(_bgpt));
		Genode::memset(_bgpt_entries,  0, sizeof(_bgpt_entries));

		size_t const blocks = (size_t)ENTRIES_SIZE / _block_size;

		/* setup PGPT, BGPT will be synced later */
		Genode::memcpy(_pgpt.signature, "EFI PART", 8);
		_pgpt.revision       = Gpt::REVISION;
		_pgpt.size           = sizeof(Gpt::Header);
		_pgpt.lba            = Gpt::PGPT_LBA;
		_pgpt.backup_lba     = _block_count - 1;
		_pgpt.part_lba_start = 2 + blocks;
		_pgpt.part_lba_end   = _block_count - (blocks + 2);
		_pgpt.guid           = Gpt::generate_uuid();
		_pgpt.gpe_lba        = 2;
		_pgpt.gpe_num        = Gpt::MAX_ENTRIES;
		_pgpt.gpe_size       = sizeof(Gpt::Entry);

		_blocks_avail = _pgpt.part_lba_end - _pgpt.part_lba_start;

		_new_gpt = true;
	}

	/**
	 * Synchronize backup header with changes in the primary header
	 */
	void _sync_backup_header()
	{
		size_t const len = _pgpt.gpe_num * _pgpt.gpe_size;
		Genode::memcpy(_bgpt_entries, _pgpt_entries, len);
		Genode::memcpy(&_bgpt, &_pgpt, sizeof(Gpt::Header));

		_bgpt.lba        = _pgpt.backup_lba;
		_bgpt.backup_lba = _pgpt.lba;
		_bgpt.gpe_lba    = _pgpt.part_lba_end + 1;
	}

	/**
	 * Write given header to Block device
	 *
	 * \param hdr      reference to header
	 * \param entries  pointer to GPT entry array
	 * \param primary  flag to indicate which header to write, primary if true
	 *                 and backup when false
	 *
	 * \return  true when successful, otherwise false
	 */
	bool _write_header(Gpt::Header const &hdr, Gpt::Entry const *entries, bool primary)
	{
		using namespace Util;

		try {
			Block::sector_t const hdr_lba = primary
			                              ? 1 : _pgpt.backup_lba;
			Block_io hdr_io(_block, _block_size, hdr_lba, 1, true,
			                &hdr, sizeof(Gpt::Header));

			size_t const len    = hdr.gpe_num * hdr.gpe_size;
			size_t const blocks = len / _block_size;
			Block::sector_t const entries_lba = primary
			                                  ? hdr_lba + 1
			                                  : _block_count - (blocks + 1);
			Block_io entries_io(_block, _block_size, entries_lba, blocks, true,
			                    entries, len);
		} catch (Block_io::Io_error) { return false; }

		return true;
	}


	/**
	 * Write protective MBR to Block device
	 *
	 * \return  true when successful, otherwise false
	 */
	bool _write_pmbr()
	{
		using namespace Util;
		try {
			Block_io pmbr(_block, _block_size, 0, 1, true, &_pmbr, sizeof(_pmbr));
		} catch (Block_io::Io_error) {
			return false;
		}

		return true;
	}

	/**
	 * Commit in-memory changes to Block device
	 *
	 * \return true if successful, otherwise false
	 */
	bool _commit_changes()
	{
		/* only if in-memory structures changed we want to write */
		if (!_new_gpt && !_new_geometry) { return true; }

		/* remove stale backup header */
		if (_new_geometry) { _wipe_old_backup_header(); }

		_sync_backup_header();

		Gpt::update_crc32(_pgpt, _pgpt_entries);
		Gpt::update_crc32(_bgpt, _bgpt_entries);

		return    _write_header(_pgpt, _pgpt_entries, true)
		       && _write_header(_bgpt, _bgpt_entries, false)
		       && (_new_pmbr ? _write_pmbr() : true) ? true : false;
	}

	/**
	 * Update geometry information, i.e., fill whole Block device
	 */
	void _update_geometry_information()
	{
		if (_pgpt.backup_lba == _block_count - 1) { return; }

		if (!_preserve_hybrid) { _setup_pmbr(); }

		_old_backup_hdr_lba = _pgpt.backup_lba;

		size_t const blocks = (size_t)ENTRIES_SIZE / _block_size;

		_pgpt.backup_lba   = _block_count - 1;
		_pgpt.part_lba_end = _block_count - (blocks + 2);

		_new_geometry = true;
	}

	/*************
	 ** Actions **
	 *************/

	enum {
		INVALID_ENTRY = ~0u,
		INVALID_START = 0,
		INVALID_SIZE  = ~0ull,
	};

	/**
	 * Check if given entry number is in range
	 */
	uint32_t _check_range(Gpt::Header const &hdr, uint32_t const entry)
	{
		return (entry != (uint32_t)INVALID_ENTRY
		    && (entry == 0 || entry > hdr.gpe_num))
		     ? (uint32_t)INVALID_ENTRY : entry;
	}

	/**
	 * Lookup entry by number or label
	 *
	 * \return pointer to entry if found, otherwise a nullptr is returned
	 */
	Gpt::Entry *_lookup_entry(Genode::Xml_node node)
	{
		Gpt::Label const label = node.attribute_value("label", Gpt::Label());
		uint32_t   const entry =
			_check_range(_pgpt,
			             node.attribute_value("entry", (uint32_t)INVALID_ENTRY));

		if (entry == INVALID_ENTRY && !label.valid()) {
			Genode::error("cannot lookup entry, invalid arguments");
			return nullptr;
		}

		if (entry != INVALID_ENTRY && label.valid()) {
			Genode::warning("entry and label given, entry number will be used");
		}

		Gpt::Entry *e = nullptr;

		if (entry != INVALID_ENTRY) {
			/* we start at 0 */
			e = &_pgpt_entries[entry - 1];
		}

		if (e == nullptr && label.valid()) {
			e = Gpt::lookup_entry(_pgpt_entries, _pgpt.gpe_num, label);
		}

		return e;
	}

	/**
	 * Add new GPT entry
	 *
	 * \param node  action node that contains the arguments
	 *
	 * \return true if entry was successfully added, otherwise false
	 */
	bool _do_add(Genode::Xml_node node)
	{
		bool       const add   = node.has_attribute("entry");
		Gpt::Label const label = node.attribute_value("label", Gpt::Label());
		Gpt::Type  const type  = node.attribute_value("type",  Gpt::Type());
		uint64_t   const size  = Util::convert(node.attribute_value("size",
		                                            Util::Size_string()));

		if (_verbose) {
			Genode::log(add ? "Add" : "Append", " entry '",
			            label.valid() ? label : "", "' size: ", size);
		}

		if (!size) {
			Genode::error("invalid size");
			return false;
		}

		/* check if partition will fit */
		Block::sector_t length = Util::size_to_lba(_block_size, size);

		Block::sector_t lba = node.attribute_value("start", (Block::sector_t)INVALID_START);

		Gpt::Entry *e = nullptr;
		if (add) {
			uint32_t const entry = _check_range(_pgpt,
				node.attribute_value("entry", (uint32_t)INVALID_ENTRY));

			if (  entry == INVALID_ENTRY
			   || lba   == INVALID_START
			   || size  == INVALID_SIZE) {
				Genode::error("cannot add entry, invalid arguments");
				return false;
			}

			if (length > _blocks_avail) {
				Genode::error("not enough sectors left (", _blocks_avail,
							  ") to satisfy request");
				return false;
			}

			if (_pgpt_entries[entry].valid()) {
				Genode::error("cannot add already existing entry ", entry);
				return false;
			}

			e = &_pgpt_entries[entry-1];

			if (e->valid()) {
				Genode::error("cannot add existing entry ", entry);
				return false;
			}

		} else {
			/* assume append operation */
			Entry const *last = Gpt::find_last_valid(_pgpt, _pgpt_entries);

			e = last ? Gpt::find_next_free(_pgpt, _pgpt_entries, last)
			         : Gpt::find_free(_pgpt, _pgpt_entries);

			if (!e) {
				Genode::error("cannot append partition, no free entry found");
				return false;
			}

			if (lba != INVALID_START) {
				Genode::warning("will ignore start LBA in append mode");
			}

			lba = last ? last->lba_end + 1 : _pgpt.part_lba_start;
			if (lba == INVALID_START) {
				Genode::error("cannot find start LBA");
				return false;
			}

			/* limit length to available blocks */
			if (length == (~0ull / _block_size)) {
				length = Gpt::gap_length(_pgpt, _pgpt_entries, last ? last : nullptr);
			}

			/* account for alignment */
			uint64_t const align = _entry_alignment / _block_size;
			if (length < align) {
				Genode::error("cannot satisfy alignment constraints");
				return false;
			}
		}

		Gpt::Uuid const type_uuid = Gpt::type_to_uuid(type);

		e->type      = type_uuid;
		e->guid      = Gpt::generate_uuid();
		e->lba_start = Util::align_start(_block_size,
		                                 _entry_alignment, lba);
		uint64_t const lba_start = e->lba_start;
		if (lba_start != lba) {
			Genode::warning("start LBA ", lba, " set to ", lba_start,
			                " due to alignment constraints");
		}
		e->lba_end = e->lba_start + (length - 1);

		if (label.valid()) { e->write_name(label); }

		_blocks_avail -= length;
		return true;
	}

	/**
	 * Delete existing GPT entry
	 *
	 * \param node  action node that contains the arguments
	 *
	 * \return true if entry was successfully added, otherwise false
	 */
	bool _do_delete(Genode::Xml_node node)
	{
		Gpt::Entry *e = _lookup_entry(node);
		if (!e) { return false; }

		if (_verbose) {
			char tmp[48];
			e->read_name(tmp, sizeof(tmp));
			uint32_t const num = Gpt::entry_num(_pgpt_entries, e);
			Genode::log("Delete entry ", num, " '", (char const*)tmp, "'");
		}

		_blocks_avail += e->length();

		Genode::memset(e, 0, sizeof(Gpt::Entry));
		return true;
	}

	/**
	 * Update existing GPT entry
	 *
	 * \param node  action node that contains the arguments
	 *
	 * \return true if entry was successfully added, otherwise false
	 */
	bool _do_modify(Genode::Xml_node node)
	{
		using namespace Genode;

		Gpt::Entry *e = _lookup_entry(node);

		if (!e) {
			Genode::error("could not lookup entry");
			return false;
		}

		if (_verbose) {
			char tmp[48];
			e->read_name(tmp, sizeof(tmp));
			uint32_t const num = Gpt::entry_num(_pgpt_entries, e);
			Genode::log("Modify entry ", num, " '", (char const*)tmp, "'");
		}

		uint64_t const new_size = Util::convert(node.attribute_value("new_size",
		                                        Util::Size_string()));
		if (new_size) {
			bool const fill = new_size == ~0ull;

			uint64_t length = fill ? Gpt::gap_length(_pgpt, _pgpt_entries, e)
			                       : Util::size_to_lba(_block_size, new_size);

			if (length == 0) {
				 error("cannot modify: ", fill ? "no space left"
				                               : "invalid length");
				 return false;
			 }

			uint64_t const old_length = e->length();
			uint64_t const new_length = length + (fill ? old_length : 0);
			uint64_t const expand     = new_length > old_length ? new_length - old_length : 0;

			if (expand && expand > _blocks_avail) {
				Genode::error("cannot modify: new length ", expand, " too large");
				return false;
			}

			if (!expand) { _blocks_avail += (old_length - new_length); }
			else         { _blocks_avail -= (new_length - old_length); }

			/* XXX overlapping check anyone? */
			e->lba_end = e->lba_start + new_length - 1;
		}

		Gpt::Label const new_label = node.attribute_value("new_label", Gpt::Label());
		if (new_label.valid()) { e->write_name(new_label); }

		Gpt::Type const new_type  = node.attribute_value("new_type",  Gpt::Type());
		if (new_type.valid()) {
			try {
				Gpt::Uuid type_uuid = Gpt::type_to_uuid(new_type);
				memcpy(&e->type, &type_uuid, sizeof(Gpt::Uuid));
			} catch (...)  {
				warning("could not update invalid type");
			}
		}

		/* XXX should we generate a new GUID? */
		// e->guid = Gpt::generate_uuid();
		return true;
	}

	/**
	 * Constructor
	 *
	 * \param block   reference to underlying Block::Connection
	 * \param config  copy of config Xml_node
	 *
	 * \throw Io_error
	 */
	Writer(Block::Connection<> &block, Genode::Xml_node config) : _block(block)
	{
		if (!_info.writeable) {
			Genode::error("cannot write to Block session");
			throw Io_error();
		}

		/* initial config read in */
		_handle_config(config);

		/*
		 * In case of wiping, end here.
		 */
		if (_wipe) { return; }

		/*
		 * Read and validate the primary GPT header and its entries first
		 * and check the backup GPT header afterwards.
		 */
		if (!_initialize) {
			_fill_and_check_header(true);
			_fill_and_check_header(false);

			if (_update_geometry) {
				Genode::log("Update geometry information");
				_update_geometry_information();
			}

			/* set available blocks */
			uint64_t const total = _pgpt.part_lba_end - _pgpt.part_lba_start;
			_blocks_avail = total - Gpt::get_used_blocks(_pgpt, _pgpt_entries);
		}
	}

	/**
	 * Execute actions specified in config
	 *
	 * \return  true if actions were executed successfully, otherwise
	 *          false
	 */
	bool execute_actions(Genode::Xml_node actions)
	{
		if (_wipe) { return _wipe_tables(); }

		if (_initialize) { _initialize_tables(); }

		try {
			actions.for_each_sub_node([&] (Genode::Xml_node node) {
				bool result = false;

				if        (node.has_type("add")) {
					result = _do_add(node);
				} else if (node.has_type("delete")) {
					result = _do_delete(node);
				} else if (node.has_type("modify")) {
					result = _do_modify(node);
				} else {
					Genode::warning("skipping invalid action");
					return;
				}

				if (!result) { throw -1; }

				_new_gpt |= result;
			});
		} catch (...) { return false; }

		/* finally write changes to disk */
		return _commit_changes();
	}
};


struct Main
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config_rom { _env, "config" };

	enum { TX_BUF_SIZE = 128u << 10, };
	Genode::Allocator_avl _block_alloc { &_heap };
	Block::Connection<>   _block       { _env, &_block_alloc, TX_BUF_SIZE };

	Genode::Constructible<Gpt::Writer> _writer { };

	Main(Genode::Env &env) : _env(env)
	{
		if (!_config_rom.valid()) {
			Genode::error("invalid config");
			_env.parent().exit(1);
			return;
		}

		Util::init_random(_heap);

		Genode::Xml_node const config = _config_rom.xml();

		try {
			_writer.construct(_block, config);
		} catch (...) {
			_env.parent().exit(1);
			return;
		}

		bool success = false;
		config.with_sub_node("actions", [&] (Genode::Xml_node actions) {
			success = _writer->execute_actions(actions); });

		_env.parent().exit(success ? 0 : 1);
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
