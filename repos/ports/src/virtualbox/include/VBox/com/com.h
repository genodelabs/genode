#ifndef ___VBox_com_com_h
#define ___VBox_com_com_h

#define COMGETTER(n)    get_##n
#define COMSETTER(n)    set_##n

#include <VBox/com/defs.h>

namespace com {
	int GetVBoxUserHomeDirectory(char *aDir, size_t aDirLen, bool fCreateDir = true);
	HRESULT Initialize(bool fGui = false);
	HRESULT Shutdown();
}

#endif /* ___VBox_com_com_h */
