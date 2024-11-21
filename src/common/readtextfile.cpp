#include "readtextfile.h"

#ifdef WIN32
#include <io.h>
#pragma warning(disable:4996)
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>

bool readtextfile(int fd, std::vector<std::string> *out)
{
	struct stat st;
	if (fstat(fd, &st) != 0) {
		return false;
	}

	if (st.st_size == 0) {
		return true;
	}

	lseek(fd, 0, SEEK_SET);

	std::vector<unsigned char> buffer(st.st_size);

	unsigned char *ptr = &buffer[0];
	unsigned char *end = ptr + st.st_size;

	if (read(fd, ptr, st.st_size) != st.st_size) {
		return false;
	}

	unsigned char *left = ptr;
	unsigned char *right = ptr;
	while (1) {
		int c = -1;
		if (right < end) {
			c = *right;
		}
		if (c == -1 || c == '\n' || c == '\r') {
			std::string line(left, right);
			out->push_back(line);
			if (c == -1) {
				break;
			}
			right++;
			if (c == '\r') {
				if (right < end && *right == '\n') {
					right++;
				}
			}
			left = right;
		} else {
			right++;
		}
	}
	return true;
}

