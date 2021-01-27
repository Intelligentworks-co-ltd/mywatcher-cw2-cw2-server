
#ifdef WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

#include <sys/stat.h>

#include "mkdirs.h"
#include <string>

bool mkdirs(char const *path)
{
	char const *p = path;
	if (p[0] && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) {
		p += 3;
	}
	while (true) {
		if (*p == 0 || *p == '\\' || *p == '/') {
			std::string dir(path, p);
#ifdef WIN32
			CreateDirectoryA(dir.c_str(), 0);
#else
			mkdir(dir.c_str(), 0777);
#endif
			if (*p == 0) {
				break;
			}
		}
		p++;
	}
#ifdef WIN32
	DWORD f = GetFileAttributesA(path);
	if (f & FILE_ATTRIBUTE_DIRECTORY) {
		return true;
	}
	return false;
#else
	struct stat st;
	stat(path, &st);
	if (S_ISDIR(st.st_mode)) {
		return true;
	}
	return false;
#endif

}
