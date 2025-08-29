/*
 * \brief  ID type with generic size
 * \author Stefan Kalkowski
 * \date   2025-08-29
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__ID_H_
#define _SRC__INCLUDE__HW__ID_H_

#include <base/output.h>

namespace Hw { template <typename> struct Id; }

template <typename T>
struct Hw::Id
{
	T value { };

	Id() = default;

	Id(T v) : value(v) {}

	static constexpr T max() { return ~T(0); }

	bool operator == (Id const &o) const {
		return value == o.value; }

	bool operator != (Id const &o) const {
		return value != o.value; }

	bool operator < (Id const &o) const {
		return value < o.value; }

	bool operator > (Id const &o) const {
		return value > o.value; }

	void print(Genode::Output &out) const {
		Genode::print(out, "id=", value, " "); }
};

#endif
