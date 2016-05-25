
#ifndef _FS_REGISTRY_H_
#define _FS_REGISTRY_H_

/* Genode includes */
#include <file_system_session/file_system_session.h>

namespace File_system {

	class FS_reference : public List<FS_reference>::Element
	{
	private:
		static List<FS_reference>   _filesystems;

		String      label;
		Connection &fs;

		FS_reference(char *label): label(label), fs(label) { }

	public:
		
		static void add_fs(char *label)
		{
			_filesystems.insert(new (env()->heap()) FS_reference(label));
		}

		static Connection *get_fs(char *label)
		{
			for (FS_reference *ref; ref = _filesystems.first(); ) {
				if (ref.label==label) return ref.fs;
			}
			throw Lookup_failed;
		}
	}
}

#endif 
