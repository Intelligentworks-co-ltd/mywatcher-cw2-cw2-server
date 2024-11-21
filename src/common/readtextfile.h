
#ifndef __READFILE_H
#define __READFILE_H

#include <vector>
#include <string>

#define ftruncate(FD, SIZE) _chsize(FD, SIZE)

bool readtextfile(int fd, std::vector<std::string> *out);

#endif

