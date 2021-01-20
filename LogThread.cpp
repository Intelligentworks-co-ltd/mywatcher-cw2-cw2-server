#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "common/jcode/jstring.h"
#include "common/text.h"
#include "common/datetime.h"
#include "common/debuglog.h"
#include "LogThread.h"
#include "TheServer.h"

#include <deque>

size_t diagnostic_log_queue_size = 0;


///////////////////////////////////////////////////////////////////////////////
// ログスレッド

LogThread::LogThread()
{
	_fp = 0;
	_server = 0;
	_log_rotate_count = 100;
}

LogThread::~LogThread()
{
	if (_fp) {
		fclose(_fp);
		_fp = 0;
	}
}

void LogThread::setup(TheServer *server, std::string const &path, int rotatecount)
{
	_server = server;
	_log_path = path;
	_log_rotate_count = rotatecount;
	_fp = 0;
}

void LogThread::stop()
{
	Thread::stop();
	_server->get_log_data()->event.signal();
}

void make_log_line(LogItem *item)
{
	//struct tm tm;
	//localtime_r(&item->time, &tm);
	//int year = tm.tm_year + 1900;
	//int month = tm.tm_mon + 1;
	//int day = tm.tm_mday;
	//int hour = tm.tm_hour;
	//int minute = tm.tm_min;
	//int second = tm.tm_sec;

	//std::string text;

	//// make header

	//if (item->threadnumber < 0) {
	//	char tmp[100];
	//	sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u | "
	//		, year
	//		, month
	//		, day
	//		, hour
	//		, minute
	//		, second
	//		);
	//	text = tmp;
	//} else {
	//	char tmp[100];
	//	sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u [%2u] %21s | "
	//		, year
	//		, month
	//		, day
	//		, hour
	//		, minute
	//		, second
	//		, item->threadnumber
	//		, item->from.c_str()
	//		);
	//	text = tmp;
	//}

	std::string text;
	text = item->time.tostring();

	if (item->threadnumber < 0) {
		text += " | ";
	} else {
		char tmp[100];
		sprintf(tmp, " [%2u] %21s | "
			, item->threadnumber
			, item->from.c_str()
			);
		text += tmp;
	}

	// message

	{
		char const *left = item->text.c_str();
		char const *right = left + item->text.size();
		while (left < right && isspace(right[-1])) {
			right--;
		}
		text.append(left, right);
	}

	item->line = text;
}

void output_log_to_console(LogItem const *item)
{
#ifdef WIN32
	ustring text = utf8_string_to_ustring(item->line);
	_putws((wchar_t const *)text.c_str());
#endif

#ifdef Linux
	ustring w = utf8_string_to_ustring(item->line);
	std::string t = soramimi::jcode::convert(soramimi::jcode::EUCJP, w);
	puts(t.c_str());
#endif
}

void LogThread::run()
{
	while (1) {
		try {
			std::list<LogItem> logqueue;
			{
				AutoLock lock(&_server->get_log_data()->mutex);
				std::swap(logqueue, _server->get_log_data()->queue);
				diagnostic_log_queue_size = logqueue.size();
			}

			if (logqueue.empty()) {
				if (interrupted()) {
					break;
				}
				_server->get_log_data()->event.wait();
			} else {
				if (_server->get_log_option()->output) {
					for (std::list<LogItem>::const_iterator it = logqueue.begin(); it != logqueue.end(); it++) {

						if (_server->get_log_option()->output_syslog) {
							syslog(LOG_DEBUG, "%s", it->text.c_str());
						}

						if (!_server->_as_service && _server->get_log_option()->output_console) {
							output_log_to_console(&*it);
						}

						// output to file

						if (!_fp) {
							_fp = fopen(_log_path.c_str(), "a");
							if (!_fp) {
								perror("cw2_server");
								fprintf(stderr, "cannot open log file.\n");
							}
						}

						// rotation
						if (_fp) {
							struct stat st;
							if (fstat(fileno(_fp), &st) == 0) {
								if (st.st_size > 1000000) {
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

										if (_fp) {
											fclose(_fp);
											_fp = 0;
											sprintf(dst, "%s.0", _log_path.c_str());
											unlink(dst);
											rename(_log_path.c_str(), dst);
											_fp = fopen(_log_path.c_str(), "w");
										}
									}
								}
							}
						}

						// write to file

						if (_fp) {
							fwrite(it->line.c_str(), 1, it->line.size(), _fp);
							putc('\n', _fp);
							fflush(_fp);
						}
					}
				}
			}
		} catch (std::bad_alloc const &e) {
			on_bad_alloc(e, __FILE__, __LINE__);
		}
	}
}


