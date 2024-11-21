
#include "fsstat.h"


#ifdef WIN32

#include <windows.h>

bool get_fs_stat(char const *path, fs_stat_t *s)
{
    ULARGE_INTEGER availablebytes;
    ULARGE_INTEGER totalbytes;
    ULARGE_INTEGER freebytes;
	if (GetDiskFreeSpaceExA(path, &availablebytes, &totalbytes, &freebytes)) {
		s->totalbytes = totalbytes.QuadPart;
		s->freebytes = freebytes.QuadPart;
		return true;
	}
	return false;
}

#else

#ifdef Darwin
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

bool get_fs_stat(char const *path, fs_stat_t *s)
{
	struct statfs t;
	if (statfs(path, &t) == 0) {
		s->totalbytes = (unsigned long long)t.f_blocks * t.f_bsize;
		s->freebytes = (unsigned long long)t.f_bfree * t.f_bsize;
		return true;			
	}
	return false;
}

#endif

