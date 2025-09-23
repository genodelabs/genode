/*
 * \brief  Implementation of dialog API
 * \author Norman Feske
 * \date   2025-09-23
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <dialog/types.h>


bool Dialog::Id::operator == (Id const &other) const
{
	auto with_trimmed = [] (auto const &s, auto const &fn)
	{
		return s.with_span([&] (Span const &s) {
			return s.trimmed([&] (Span const &trimmed) {
				return fn(trimmed); }); });
	};

	return with_trimmed(other.value, [&] (Span const &trimmed_other) {
		return with_trimmed(value, [&] (Span const &trimmed_value) {
			return trimmed_other.equals(trimmed_value); }); });
}


