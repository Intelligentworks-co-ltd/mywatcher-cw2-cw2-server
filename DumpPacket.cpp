#include <time.h>
#include <sys/stat.h>

#include "config.h"
#include "common/jcode/jstring.h"
#include "common/text.h"
#include "common/datetime.h"
#include "common/debuglog.h"
#include "DumpPacket.h"
#include "cw_server.h"

#include <deque>

#ifdef WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////
// ログスレッド

DumpPacket::DumpPacket()
{
	_fd = -1;
	_server = 0;
	_log_rotate_count = 100;
}

DumpPacket::~DumpPacket()
{
	if (_fd != -1) {
		::close(_fd);
		_fd = -1;
	}
}

void DumpPacket::setup(TheServer *server, std::string const &path, int rotatecount)
{
	_server = server;
	_log_path = path;
	_log_rotate_count = rotatecount;
	_fd = -1;
}

struct pcap_hdr_t {
	unsigned long magic_number;   /* magic number */
	unsigned short version_major;  /* major version number */
	unsigned short version_minor;  /* minor version number */
	long thiszone;       /* GMT to local correction */
	unsigned long sigfigs;        /* accuracy of timestamps */
	unsigned long snaplen;        /* max length of captured packets, in octets */
	unsigned long network;        /* data link type */
};
struct pcaprec_hdr_t {
	unsigned long ts_sec;         /* timestamp seconds */
	unsigned long ts_usec;        /* timestamp microseconds */
	unsigned long incl_len;       /* number of octets of packet saved in file */
	unsigned long orig_len;       /* actual length of packet */
};

#ifndef O_BINARY
#define O_BINARY 0
#endif

static int create_pcap_file(char const *path)
{
	int fd;
	fd = ::open(path, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, S_IREAD | S_IWRITE);
	if (!fd) {
		perror("cw2_server");
		fprintf(stderr, "cannot open pcap file.\n");
		return -1;
	}
	pcap_hdr_t globalheader;
	globalheader.magic_number = 0xa1b2c3d4;
	globalheader.version_major = 2;
	globalheader.version_minor = 4;
	globalheader.thiszone = 0;
	globalheader.sigfigs = 0;
	globalheader.snaplen = 65535;
	globalheader.network = 1;
	write(fd, &globalheader, sizeof(pcap_hdr_t));
	return fd;
}

void DumpPacket::rotate()
{
	if (_log_rotate_count > 0) {
		char src[1000];
		char dst[1000];
		int i = _log_rotate_count;
		while (i > 1) {
			sprintf(src, "%s.%u", _log_path.c_str(), i - 2);
			sprintf(dst, "%s.%u", _log_path.c_str(), i - 1);
			unlink(dst);
			rename(src, dst);
			i--;
		}

		if (_fd != -1) {
			close(_fd);
		}

		sprintf(dst, "%s.0", _log_path.c_str());
		unlink(dst);
		rename(_log_path.c_str(), dst);
	}
}

void DumpPacket::add(ContentsWatcher::CapturedPacket const *packet)
{
	// output to file

	if (_fd == -1) {
		char const *path = _log_path.c_str();
#ifdef WIN32
		if (_access(path, 0) == 0) {
			rotate();
		}
#else
		if (access(path, F_OK) == 0) {
			rotate();
		}
#endif
		_fd = create_pcap_file(path);
	}

	// rotation
	if (_fd) {
		struct stat st;
		if (fstat(_fd, &st) == 0) {
			if (st.st_size > 10000000) {
				rotate();
				_fd = create_pcap_file(_log_path.c_str());
			}
		}
	}

	// write to file

	if (_fd) {
		pcaprec_hdr_t h;
		h.ts_sec = packet->_container->tv_sec;
		h.ts_usec = packet->_container->tv_usec;
		h.incl_len = packet->datalen();
		h.orig_len = packet->datalen();
		::write(_fd, &h, sizeof(pcaprec_hdr_t));
		::write(_fd, packet->dataptr(), packet->datalen());
	}
}

