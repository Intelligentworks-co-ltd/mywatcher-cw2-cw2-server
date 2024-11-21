
#ifndef __FSSTAT_H
#define __FSSTAT_H

struct fs_stat_t {
	unsigned long long totalbytes;
	unsigned long long freebytes;
};

bool get_fs_stat(char const *path, fs_stat_t *s);



#endif




