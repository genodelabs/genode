/*
 * \brief  Pointer of const object safe against null dereferencing
 * \author Martin Stein
 * \date   2021-04-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SNAPSHOT_H_
#define _SNAPSHOT_H_

namespace File_vault {

	class Snapshot;
}

class File_vault::Snapshot
{
	private:

		Generation const _generation;

	public:

		Snapshot(Generation const &generation)
		:
			_generation { generation }
		{ }

		virtual ~Snapshot() { }


		/***************
		 ** Accessors **
		 ***************/

		Generation const &generation() const { return _generation; }
};

#endif /* _SNAPSHOT_H_ */
