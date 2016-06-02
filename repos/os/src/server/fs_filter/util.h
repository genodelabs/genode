
#ifndef _UTIL_H
#define _UTIL_H

namespace File_system {

	inline void remove_trailing_slash(char *path)
	{
		int i = 0;
		while (path[i] && path[i + 1]) i++;

		/*
		 * Never touch the first character to preserve the invariant of
		 * the leading slash.
		 */
		if (i > 0 && path[i] == /)
			path[i] = 0;
	}
	
	inline char *last_element(char *path)
	{
		char *result = path;
		for (; *path; path++)
			if (path[0] == '/' && path[1] != 0)
				result = path;
		return result;
	}

}

#endif
