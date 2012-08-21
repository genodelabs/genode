/*
 * \brief  TAR record lookup function
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2012-08-20
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOOKUP_H_
#define _LOOKUP_H_

/* Genode includes */
#include <os/path.h>

/* local includes */
#include <node.h>

namespace File_system {

	extern char  *_tar_base;
	extern size_t _tar_size;

	typedef Genode::Path<File_system::MAX_PATH_LEN> Absolute_path;

	struct Lookup_criterion { virtual bool match(char const *path) = 0; };

	struct Lookup_exact : public Lookup_criterion
	{
		Absolute_path _match_path;

		Lookup_exact(char const *match_path)
		: _match_path(match_path)
		{
			_match_path.remove_trailing('/');
		}

		bool match(char const *path)
		{
			Absolute_path test_path(path);
			test_path.remove_trailing('/');
			return _match_path.equals(test_path);
		}
	};


	/**
	 * Lookup the Nth record in the specified path
	 */
	struct Lookup_member_of_path : public Lookup_criterion
	{
		Absolute_path _dir_path;
		int const index;
		int cnt;

		Lookup_member_of_path(char const *dir_path, int index)
		: _dir_path(dir_path), index(index), cnt(0)
		{
			_dir_path.remove_trailing('/');
		}

		bool match(char const *path)
		{
			Absolute_path test_path(path);

			if (!test_path.strip_prefix(_dir_path.base()))
				return false;

			if (!test_path.has_single_element())
				return false;

			cnt++;

			/* match only if index is reached */
			if (cnt - 1 != index)
				return false;

			return true;
		}
	};

	Record *_lookup(Lookup_criterion *criterion)
	{
		/* measure size of archive in blocks */
		unsigned block_id = 0, block_cnt = _tar_size/Record::BLOCK_LEN;

		/* scan metablocks of archive */
		while (block_id < block_cnt) {

			Record *record = (Record *)(_tar_base + block_id*Record::BLOCK_LEN);

			/* get infos about current file */
			if (criterion->match(record->name()))
				return record;

			size_t file_size = record->size();

			/* some datablocks */       /* one metablock */
			block_id = block_id + (file_size / Record::BLOCK_LEN) + 1;

			/* round up */
			if (file_size % Record::BLOCK_LEN != 0) block_id++;

			/* check for end of tar archive */
			if (block_id*Record::BLOCK_LEN >= _tar_size)
				break;

			/* lookout for empty eof-blocks */
			if (*(_tar_base + (block_id*Record::BLOCK_LEN)) == 0x00)
				if (*(_tar_base + (block_id*Record::BLOCK_LEN + 1)) == 0x00)
					break;
		}

		return 0;
	}

}

#endif /* _LOOKUP_H_ */
