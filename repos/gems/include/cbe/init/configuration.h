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

#ifndef _CBE__INIT__CONFIGURATION_H_
#define _CBE__INIT__CONFIGURATION_H_

/* Genode includes */
#include <util/xml_node.h>

namespace Cbe_init { class Configuration; }

class Cbe_init::Configuration
{
	private:

		Genode::uint64_t _vbd_nr_of_lvls     { 0 };
		Genode::uint64_t _vbd_nr_of_children { 0 };
		Genode::uint64_t _vbd_nr_of_leafs    { 0 };
		Genode::uint64_t _ft_nr_of_lvls      { 0 };
		Genode::uint64_t _ft_nr_of_children  { 0 };
		Genode::uint64_t _ft_nr_of_leafs     { 0 };

	public:

		struct Invalid : Genode::Exception { };

		Configuration (Genode::Xml_node const &node)
		{
			node.with_optional_sub_node("virtual-block-device",
			                   [&] (Genode::Xml_node const &vbd)
			{
				_vbd_nr_of_lvls =
					vbd.attribute_value("nr_of_levels", (Genode::uint64_t)0);
				_vbd_nr_of_children =
					vbd.attribute_value("nr_of_children", (Genode::uint64_t)0);
				_vbd_nr_of_leafs =
					vbd.attribute_value("nr_of_leafs", (Genode::uint64_t)0);
			});
			node.with_optional_sub_node("free-tree",
			                   [&] (Genode::Xml_node const &ft)
			{
				_ft_nr_of_lvls =
					ft.attribute_value("nr_of_levels", (Genode::uint64_t)0);
				_ft_nr_of_children =
					ft.attribute_value("nr_of_children", (Genode::uint64_t)0);
				_ft_nr_of_leafs =
					ft.attribute_value("nr_of_leafs", (Genode::uint64_t)0);
			});
			if (_vbd_nr_of_lvls     == 0 ||
				_vbd_nr_of_children == 0 ||
				_vbd_nr_of_leafs    == 0 ||
				_ft_nr_of_lvls      == 0 ||
				_ft_nr_of_children  == 0 ||
				_ft_nr_of_leafs     == 0)
			{
				throw Invalid();
			}
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

		Genode::uint64_t vbd_nr_of_lvls     () const { return _vbd_nr_of_lvls    ; }
		Genode::uint64_t vbd_nr_of_children () const { return _vbd_nr_of_children; }
		Genode::uint64_t vbd_nr_of_leafs    () const { return _vbd_nr_of_leafs   ; }
		Genode::uint64_t ft_nr_of_lvls      () const { return _ft_nr_of_lvls     ; }
		Genode::uint64_t ft_nr_of_children  () const { return _ft_nr_of_children ; }
		Genode::uint64_t ft_nr_of_leafs     () const { return _ft_nr_of_leafs    ; }

		void print(Genode::Output &out) const
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

#endif /* _CBE__INIT__CONFIGURATION_H_ */
