/*
 * \brief  Integration of the Tresor block encryption
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

#ifndef _TRESOR__INIT__CONFIGURATION_H_
#define _TRESOR__INIT__CONFIGURATION_H_

/* base includes */
#include <util/xml_node.h>

/* tresor includes */
#include <tresor/types.h>

namespace Tresor_init {

	using namespace Genode;
	using namespace Tresor;

	class Configuration;
}

class Tresor_init::Configuration
{
	private:

		uint64_t _vbd_nr_of_lvls     { 0 };
		uint64_t _vbd_nr_of_children { 0 };
		uint64_t _vbd_nr_of_leafs    { 0 };
		uint64_t _ft_nr_of_lvls      { 0 };
		uint64_t _ft_nr_of_children  { 0 };
		uint64_t _ft_nr_of_leafs     { 0 };

		static bool _is_power_of_2(uint64_t val)
		{
			for (; val && (val & 1) == 0; val >>= 1);
			return val == 1;
		}

	public:

		struct Invalid : Exception { };

		Configuration (Xml_node const &node)
		{
			node.with_optional_sub_node("virtual-block-device",
			                   [&] (Xml_node const &vbd)
			{
				_vbd_nr_of_lvls =
					vbd.attribute_value("nr_of_levels", (uint64_t)0);
				_vbd_nr_of_children =
					vbd.attribute_value("nr_of_children", (uint64_t)0);
				_vbd_nr_of_leafs =
					vbd.attribute_value("nr_of_leafs", (uint64_t)0);
			});
			node.with_optional_sub_node("free-tree",
			                   [&] (Xml_node const &ft)
			{
				_ft_nr_of_lvls =
					ft.attribute_value("nr_of_levels", (uint64_t)0);
				_ft_nr_of_children =
					ft.attribute_value("nr_of_children", (uint64_t)0);
				_ft_nr_of_leafs =
					ft.attribute_value("nr_of_leafs", (uint64_t)0);
			});
			ASSERT(_vbd_nr_of_lvls);
			ASSERT(_vbd_nr_of_lvls <= TREE_MAX_NR_OF_LEVELS);
			ASSERT(_vbd_nr_of_leafs);
			ASSERT(_is_power_of_2(_vbd_nr_of_children));
			ASSERT(_vbd_nr_of_children <= NR_OF_T1_NODES_PER_BLK);
			ASSERT(_ft_nr_of_lvls);
			ASSERT(_ft_nr_of_lvls <= TREE_MAX_NR_OF_LEVELS);
			ASSERT(_ft_nr_of_leafs);
			ASSERT(_is_power_of_2(_ft_nr_of_children));
			ASSERT(_ft_nr_of_children <= NR_OF_T1_NODES_PER_BLK);
			ASSERT(_ft_nr_of_children <= NR_OF_T2_NODES_PER_BLK);
		}

		Configuration (Configuration const &other)
		{
			_vbd_nr_of_lvls     = other._vbd_nr_of_lvls    ;
			_vbd_nr_of_children = other._vbd_nr_of_children;
			_vbd_nr_of_leafs    = other._vbd_nr_of_leafs   ;
			_ft_nr_of_lvls      = other._ft_nr_of_lvls     ;
			_ft_nr_of_children  = other._ft_nr_of_children ;
			_ft_nr_of_leafs     = other._ft_nr_of_leafs    ;
		}

		uint64_t vbd_nr_of_lvls     () const { return _vbd_nr_of_lvls    ; }
		uint64_t vbd_nr_of_children () const { return _vbd_nr_of_children; }
		uint64_t vbd_nr_of_leafs    () const { return _vbd_nr_of_leafs   ; }
		uint64_t ft_nr_of_lvls      () const { return _ft_nr_of_lvls     ; }
		uint64_t ft_nr_of_children  () const { return _ft_nr_of_children ; }
		uint64_t ft_nr_of_leafs     () const { return _ft_nr_of_leafs    ; }

		void print(Output &out) const
		{
			Genode::print(out,
				"vbd=(lvls=", _vbd_nr_of_lvls,
				" children=", _vbd_nr_of_children,
				" leafs=",    _vbd_nr_of_leafs, ")",
				" ft=(lvls=", _ft_nr_of_lvls,
				" children=", _ft_nr_of_children,
				" leafs=",    _ft_nr_of_leafs, ")");
		}
};

#endif /* _TRESOR__INIT__CONFIGURATION_H_ */
