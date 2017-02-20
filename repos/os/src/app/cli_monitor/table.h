/*
 * \brief  Utility for printing a table to the terminal
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TABLE_H_
#define _TABLE_H_

#include <terminal_session/terminal_session.h>

namespace Cli_monitor { template <typename TI> class Table; }


template <typename TI>
class Cli_monitor::Table
{
	private:

		static void _print_cell(TI &info, Terminal::Session &terminal,
		                        typename TI::Column column, size_t column_size)
		{
			size_t const padding = column_size - info.len(column);

			if (!TI::left_aligned(column))
				tprint_padding(terminal, padding);

			info.print_cell(terminal, column);

			if (TI::left_aligned(column))
				tprint_padding(terminal, padding);
		}

		/**
		 * Print centered title of table column
		 */
		static void _print_label(Terminal::Session &terminal,
		                         typename TI::Column column, size_t column_size)
		{
			size_t const padding = column_size - strlen(TI::label(column));
			size_t const left_padding = padding / 2;

			tprint_padding(terminal, left_padding);
			tprintf(terminal, "%s", TI::label(column));
			tprint_padding(terminal, padding - left_padding);
		}

	public:

		static void print(Terminal::Session &terminal, TI info[], unsigned num_rows)
		{
			/*
			 * Determine formatting of table
			 */
			size_t column_size[TI::num_columns()];
			for (unsigned i = 0; i < TI::num_columns(); i++)
				column_size[i] = strlen(TI::label((typename TI::Column)i));

			for (unsigned i = 0; i < num_rows; i++) {
				for (unsigned j = 0; j < TI::num_columns(); j++)
					column_size[j] = max(column_size[j],
					                     info[i].len((typename TI::Column)j));
			}

			/*
			 * Print table
			 */
			tprintf(terminal, "  ");
			for (unsigned j = 0; j < TI::num_columns(); j++) {
				_print_label(terminal, (typename TI::Column)j, column_size[j]);
				if (j < TI::num_columns() - 1) tprintf(terminal, " | ");
			}
			tprintf(terminal, "\n");

			tprintf(terminal, "  ");
			for (unsigned j = 0; j < TI::num_columns(); j++) {
				for (unsigned i = 0; i < column_size[j]; i++)
					tprintf(terminal, "-");
				if (j < TI::num_columns() - 1) tprintf(terminal, "-+-");
			}
			tprintf(terminal, "\n");

			for (unsigned i = 0; i < num_rows; i++) {
				tprintf(terminal, "  ");
				for (unsigned j = 0; j < TI::num_columns(); j++) {
					_print_cell(info[i], terminal, (typename TI::Column)j, column_size[j]);
					if (j < TI::num_columns() - 1) tprintf(terminal, " | ");
				}
				tprintf(terminal, "\n");
			}
		}
};

#endif /* _TABLE_H_ */
