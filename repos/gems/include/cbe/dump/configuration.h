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

#ifndef _CBE__DUMP__CONFIGURATION_H_
#define _CBE__DUMP__CONFIGURATION_H_

/* Genode includes */
#include <util/xml_node.h>

namespace Cbe_dump { class Configuration; }

class Cbe_dump::Configuration
{
	private:

		bool             _unused_nodes;
		Genode::uint32_t _max_superblocks;
		Genode::uint32_t _max_snapshots;
		bool             _vbd;
		bool             _vbd_pba_filter_enabled;
		Genode::uint64_t _vbd_pba_filter;
		bool             _vbd_vba_filter_enabled;
		Genode::uint64_t _vbd_vba_filter;
		bool             _free_tree;
		bool             _meta_tree;
		bool             _hashes;

	public:

		Configuration (Genode::Xml_node const &node)
		:
			_unused_nodes           { node.attribute_value("unused_nodes", true) },
			_max_superblocks        { node.attribute_value("max_superblocks", ~(Genode::uint32_t)0) },
			_max_snapshots          { node.attribute_value("max_snapshots", ~(Genode::uint32_t)0) },
			_vbd                    { node.attribute_value("vbd", true) },
			_vbd_pba_filter_enabled { node.attribute_value("vbd_pba_filter_enabled", false) },
			_vbd_pba_filter         { node.attribute_value("vbd_pba_filter", (Genode::uint64_t)0) },
			_vbd_vba_filter_enabled { node.attribute_value("vbd_vba_filter_enabled", false) },
			_vbd_vba_filter         { node.attribute_value("vbd_vba_filter", (Genode::uint64_t)0) },
			_free_tree              { node.attribute_value("free_tree", true) },
			_meta_tree              { node.attribute_value("meta_tree", true) },
			_hashes                 { node.attribute_value("hashes", true) }
		{ }

		Configuration (Configuration const &other)
		:
			_unused_nodes           { other._unused_nodes },
			_max_superblocks        { other._max_superblocks },
			_max_snapshots          { other._max_snapshots },
			_vbd                    { other._vbd },
			_vbd_pba_filter_enabled { other._vbd_pba_filter_enabled },
			_vbd_pba_filter         { other._vbd_pba_filter },
			_vbd_vba_filter_enabled { other._vbd_vba_filter_enabled },
			_vbd_vba_filter         { other._vbd_vba_filter },
			_free_tree              { other._free_tree },
			_meta_tree              { other._meta_tree },
			_hashes                 { other._hashes }
		{ }

		bool             unused_nodes()           const { return _unused_nodes; }
		Genode::uint32_t max_superblocks()        const { return _max_superblocks; }
		Genode::uint32_t max_snapshots()          const { return _max_snapshots; }
		bool             vbd()                    const { return _vbd; }
		bool             vbd_pba_filter_enabled() const { return _vbd_pba_filter_enabled; }
		Genode::uint64_t vbd_pba_filter()         const { return _vbd_pba_filter; }
		bool             vbd_vba_filter_enabled() const { return _vbd_vba_filter_enabled; }
		Genode::uint64_t vbd_vba_filter()         const { return _vbd_vba_filter; }
		bool             free_tree()              const { return _free_tree; }
		bool             meta_tree()              const { return _meta_tree; }
		bool             hashes()                 const { return _hashes; }

		void print(Genode::Output &out) const
		{
			Genode::print(out,
				"unused_nodes=",            _unused_nodes          ,
				" max_superblocks=",        _max_superblocks       ,
				" max_snapshots=",          _max_snapshots         ,
				" vbd=",                    _vbd                   ,
				" vbd_pba_filter_enabled=", _vbd_pba_filter_enabled,
				" vbd_pba_filter=",         _vbd_pba_filter        ,
				" vbd_vba_filter_enabled=", _vbd_vba_filter_enabled,
				" vbd_vba_filter=",         _vbd_vba_filter        ,
				" free_tree=",              _free_tree             ,
				" meta_tree=",              _meta_tree             ,
				" hashes=",                 _hashes                );
		}

} __attribute__((packed));

#endif /* _CBE__DUMP__CONFIGURATION_H_ */
