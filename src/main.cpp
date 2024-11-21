#include "config.h"
#include "TheServer.h"
#include "cw.h"

#include "common/eq.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////

enum Operation {
	OP_DONE,
	OP_NORMAL,
	OP_INTERFACE,
	OP_FILENAME,
	OP_NO_DATABASE,
	OP_NO_IP_CHECKSUM,
	OP_SNAP_LENGTH,
	OP_IGNORE_TARGET_SERVERS,
	OP_IGNORE_DEFECTIVE_PACKET,
	OP_NO_SCAN_DEVICES,
	OP_VERSION,
	OP_NO_CONSOLE,
	OP_SAVE_PACKET,
	OP_DUMP_PACKET,
	OP_NO_LOG,
	OP_PCAP_FILTER,
	OP_NO_ARCHIVE,
#ifdef WIN32
	OP_INSTALL,
	OP_UNINSTALL,
	OP_START,
	OP_STOP,
#endif
	OP_SERVICE,
//#ifdef _DEBUG
	OP_DEBUG
//#endif
};

struct OptionTable {
	char const *string;
	bool parameter;
	int id;
} optiontable[] = {

	// ファイル解析モード：ファイル名を指定
	"-f", true, OP_FILENAME,

	// オンライン解析モード：インターフェース名を指定
	"-i", true, OP_INTERFACE,
	"--interface", true, OP_INTERFACE,

	// データベースに書き込まない
	"-ndb", false, OP_NO_DATABASE,
	"--no-database", false, OP_NO_DATABASE,

	// パケットのチェックサムを無視する
	"-ncs", false, OP_NO_IP_CHECKSUM,
	"--no-ip-checksum", false, OP_NO_IP_CHECKSUM,

	// キャプチャするパケットの最大バイト数
	"--snap-length", true, OP_SNAP_LENGTH,

	// 監視対象サーバの設定を無視する
	"-its", false, OP_IGNORE_TARGET_SERVERS,
	"--ignore-target-servers", false, OP_IGNORE_TARGET_SERVERS,

	// 不良パケットでも無理矢理解析する
	"-idp", false, OP_IGNORE_DEFECTIVE_PACKET,
	"--ignore-defective-packet", false, OP_IGNORE_DEFECTIVE_PACKET,

	// デバイスを検索しない
	"-nsd", false, OP_NO_SCAN_DEVICES,
	"--no-scan-devices", false, OP_NO_SCAN_DEVICES,

	// バージョン情報を表示
	"-v", false, OP_VERSION,
	"--version", false, OP_VERSION,

	// 最後にキャプチャしたパケットを保存する
	"--save-packet", true, OP_SAVE_PACKET,

	// キャプチャしたパケットを保存する
	"--dump-packet", true, OP_DUMP_PACKET,

	// ログを出力しない
	"--no-log", false, OP_NO_LOG,

	// ログをコンソールに出力しない
	"--no-console", false, OP_NO_CONSOLE,

	// pcapフィルタを適用する
	"--pcap-filter", true, OP_PCAP_FILTER,

	// アーカイブ（CSVファイル）を作成しない
	"-na", false, OP_NO_ARCHIVE,
	"--no_archive", false, OP_NO_ARCHIVE,

#ifdef WIN32
	"--install", false, OP_INSTALL,
	"--uninstall", false, OP_UNINSTALL,
	"--service--", false, OP_SERVICE,
#endif
//#ifdef _DEBUG
	"--debug", false, OP_DEBUG,
//#endif
	0
};

///////////////////////////////////////////////////////////////////////////////
// Windowsサービス

#ifdef WIN32
#include <windows.h>
#include <winsvc.h>
#include <string>


extern void WINAPI ServiceMain(DWORD ac, char **av);


extern SERVICE_STATUS_HANDLE g_srv_status_handle;


// エラーメッセージを取得
std::wstring get_error_message(DWORD err)
{
    LPTSTR lpBuffer;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, LANG_USER_DEFAULT, (LPTSTR)&lpBuffer, 0, NULL);
	std::wstring str;
	if (lpBuffer) {
		str = lpBuffer;
	}
    LocalFree(lpBuffer);
	return str;
}



// サービスの状態を表示
void service_status()
{
	SC_HANDLE scm = 0;
	SC_HANDLE srv = 0;
	SERVICE_STATUS st;
	memset(&st, 0, sizeof(st));
	int err = 0;

	scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!scm) {
		err = GetLastError();
		goto done;
	}

	srv = OpenService(scm, WINDOWS_SERVICE_NAME, SERVICE_QUERY_STATUS);
	if (!srv) {
		err = GetLastError();
		goto done;
	}

	if (!QueryServiceStatus(srv, &st)) {
		err = GetLastError();
		goto done;
	}

	wprintf(L"service status: %08x\n", st.dwCurrentState);

	err = 0;
done:;
	if (srv) {
		CloseServiceHandle(srv);
	}
	if (scm) {
		CloseServiceHandle(scm);
	}

	if (err != 0) {
		std::wstring str = get_error_message(err);
		wprintf(L"%s\n", str.c_str());
	}
}

// サービスを開始する
bool service_start()
{
	bool success = false;

	SC_HANDLE scm = 0;
	SC_HANDLE srv = 0;
	SERVICE_STATUS st;
	memset(&st, 0, sizeof(st));

	scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!scm) {
		goto done;
	}
	srv = OpenService(scm, WINDOWS_SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_START);
	if (!srv) {
		goto done;
	}
	if (!StartService(srv, 0, 0)) {
		goto done;
	}
	for (int i = 0; i < 100; i++) {
		if (!QueryServiceStatus(srv, &st)) {
			goto done;
		}
		if (st.dwCurrentState == SERVICE_RUNNING) {
			success = true;
			break;
		}
		Sleep(100);
	}

done:;
	if (srv) {
		CloseServiceHandle(srv);
	}
	if (scm) {
		CloseServiceHandle(scm);
	}

	return success;
}

// サービスを停止する
bool service_stop()
{
	bool success = false;

	SC_HANDLE scm = 0;
	SC_HANDLE srv = 0;
	SERVICE_STATUS st;
	memset(&st, 0, sizeof(st));

	scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!scm) {
		goto done;
	}
	srv = OpenService(scm, WINDOWS_SERVICE_NAME, SERVICE_QUERY_STATUS | SERVICE_STOP);
	if (!srv) {
		goto done;
	}
	if (!ControlService(srv, SERVICE_CONTROL_STOP, &st)) {
		goto done;
	}

	for (int i = 0; i < 100; i++) {
		if (!QueryServiceStatus(srv, &st)) {
			goto done;
		}
		if (st.dwCurrentState == SERVICE_STOPPED) {
			success = true;
			break;
		}
		Sleep(100);
	}

done:;
	if (srv) {
		CloseServiceHandle(srv);
	}
	if (scm) {
		CloseServiceHandle(scm);
	}

	return success;
}

// サービスを登録する
void service_install()
{
	wchar_t path[MAX_PATH];
	int err = 0;
	if (!GetModuleFileName(0, path, MAX_PATH)) {
		err = GetLastError();
		goto done;
	}
	wcscat(path, L" --service--");

	SC_HANDLE scm = 0;
	SC_HANDLE srv = 0;
	scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!scm) {
		err = GetLastError();
		goto done;
	}
	srv = CreateService(scm, WINDOWS_SERVICE_NAME, WINDOWS_DISPLAY_NAME, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path, 0, 0, 0, 0, 0);
	if (!srv) {
		err = GetLastError();
		goto done;
	}

	err = 0;
done:;
	if (srv) {
		CloseServiceHandle(srv);
	}
	if (scm) {
		CloseServiceHandle(scm);
	}

	if (err != 0) {
		std::wstring str = get_error_message(err);
		wprintf(L"%s\n", str.c_str());
	}
}

// サービスを登録解除する
void service_uninstall()
{
	SC_HANDLE scm = 0;
	SC_HANDLE srv = 0;
	int err = 0;
	scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!scm) {
		err = GetLastError();
		goto done;
	}
	srv = OpenService(scm, WINDOWS_SERVICE_NAME, DELETE);
	if (!srv) {
		err = GetLastError();
		goto done;
	} else {
		if (!DeleteService(srv)) {
			err = GetLastError();
			goto done;
		}
	}

	err = 0;
done:;
	if (srv) {
		CloseServiceHandle(srv);
	}
	if (scm) {
		CloseServiceHandle(scm);
	}

	if (err != 0) {
		std::wstring str = get_error_message(err);
		wprintf(L"%s\n", str.c_str());
	}
}

#else // WIN32

#include <signal.h>
#include <fcntl.h>

///////////////////////////////////////////////////////////////////////////////
// Unixデーモン

static bool get_service_pid(pid_t *pid)
{
	char tmp[100];
	int fd = open(PID_FILE_PATH, O_RDONLY);
	if (fd == -1) {
		return false;
	}
	int n = read(fd, tmp, 99);
	close(fd);
	if (n < 1) {
		return false;
	}
	tmp[n] = 0;
	*pid = atoi(tmp);
	return true;
}

void daemon_stop()
{
	pid_t pid = 0;
	if (get_service_pid(&pid)) {
		kill(pid, SIGTERM);
	}
}

void daemon_restart()
{
	pid_t pid = 0;
	if (get_service_pid(&pid)) {
		kill(pid, SIGHUP);
	}
}


#endif // WIN32




#ifdef _DEBUG
extern void debug();
#endif

typedef unsigned short uchar_t;
typedef std::basic_string<uchar_t> ustring;


///////////////////////////////////////////////////////////////////////////////

#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "common/readtextfile.h"
#include "common/eq.h"
#include "cw_option.h"

static std::string trim(char const *left, char const *right)
{
	while (left < right && isspace(*left)) {
		left++;
	}
	while (left < right && isspace(right[-1])) {
		right--;
	}
	return std::string(left, right);
}

void parse_exclude_clients(ContentsWatcher::Option *option, char const *ptr)
{

	char const *end = ptr + strlen(ptr);
	while (1) {
		char const *next = strchr(ptr, ',');
		char const *p = next ? next : end;
		char const *left = ptr;
		char const *right = p;
		while (left < right && isspace(left[0])) left++;
		while (left < right && isspace(right[-1])) right--;
		std::string s(left, right);
		ip_address_t a;
		if (a.parse(s.c_str())) {
			//puts(a.tostring().c_str());
			option->exclude_clients.insert(a);
		}
		if (!next) {
			break;
		}
		ptr = next + 1;
	}
}

typedef void (*ini_file_handler_t)(std::string const &name, std::string const &value, void *cookie);

void option_handler(std::string const &name, std::string const &value, void *cookie)
{
	ContentsWatcher::Option *data = (ContentsWatcher::Option *)cookie;
	if (eq(name, "st_mtime")) {
		data->mtime = strtoul(value.c_str(), 0, 10);
		return;
	}
	if (eq(name, "db_server")) {
		data->database.server = value;
		return;
	}
	if (eq(name, "db_name")) {
		data->database.name = value;
		return;
	}
	if (eq(name, "db_uid")) {
		data->database.uid = value;
		return;
	}
	if (eq(name, "db_pwd")) {
		data->database.pwd = value;
		return;
	}
	if (eq(name, "nwif")) {
		data->interface_text = value;
		return;
	}
	if (eq(name, "exclude_clients")) {
		parse_exclude_clients(data, value.c_str());
		return;
	}
}

void exclusion_info_handler(std::string const &name, std::string const &value, void *cookie)
{
	ContentsWatcher::ExclusionInfo *data = (ContentsWatcher::ExclusionInfo *)cookie;
	if (eq(name, "st_mtime")) {
		data->mtime = strtoul(value.c_str(), 0, 10);
		return;
	}
	if (eq(name, "read")) {
		data->cmd_read = atoi(value.c_str()) != 0;
		return;
	}
}

bool parse_ini_file(char const *path, ini_file_handler_t handler, void *cookie)
{
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd == -1) {
		fprintf(stderr, "could not open ini file '%s'\n", path);
		return false;
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		fprintf(stderr, "fstat failed\n");
		return false;
	}

	//option->st_mtime = st.st_mtime;
	{
		char tmp[100];
		sprintf(tmp, "%u", st.st_mtime);
		handler("st_mtime", tmp, cookie);
	}

	std::vector<std::string> lines;
	if (readtextfile(fd, &lines)) {
		for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++) {
			char const *e = strchr(it->c_str(), '=');
			if (e) {
				char const *left;
				char const *right;
				left = it->c_str();
				right = e;
				std::string name = trim(left, right);
				left = e + 1;
				right = e + strlen(e);
				std::string value = trim(left, right);
				handler(name, value, cookie);
			}
		}
	}
	close(fd);

	return true;
}

bool parse_option_ini_file(ContentsWatcher::Option *data)
{
	return parse_ini_file(CW_INI_FILE, option_handler, data);
}

bool parse_exclusion_command_info_file(ContentsWatcher::ExclusionInfo *data)
{
	return parse_ini_file(CW_EXCLUSION_COMMAND_INFO_FILE, exclusion_info_handler, data);
}

bool parse_targetservers_file(ContentsWatcher::TargetServersInfo *data)
{
	char const *path = CW_TARGETSERVERS_FILE;

	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd == -1) {
		fprintf(stderr, "could not open file '%s'\n", path);
		return false;
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		fprintf(stderr, "fstat failed\n");
		return false;
	}

	data->mtime = st.st_mtime;

	std::vector<std::string> lines;
	if (readtextfile(fd, &lines)) {
		for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++) {
			std::vector<std::string> row;
			char const *left = it->c_str();
			while (isspace(*left)) {
				left++;
			}
			if (*left == '#') {
				continue;
			}
			do {
				char const *right = strchr(left, '\t');
				if (right) {
					std::string v(left, right);
					row.push_back(v);
					left = right;
					while (isspace(*left)) {
						left++;
					}
				} else {
					std::string v(left);
					row.push_back(v);
					left = 0;
				}
			} while (left);
			//
			if (row.size() == 3) {
				ip_address_t addr;
				int flags;
				addr.parse(row[0].c_str());
				flags = atoi(row[1].c_str());
				data->map[addr] = flags;
			}
		}
	}
	close(fd);

	return true;
}

///////////////////////////////////////////////////////////////////////////////

extern char const *build_timestamp;
extern char const *build_system;
extern int build_revision;

#ifdef WIN32
std::string get_build_info()
{
	stringbuffer sb;
	sb.write(build_timestamp);
	sb.put(',');
	sb.put(' ');
	sb.write(build_system);
	char tmp[100];
	sprintf(tmp, ", _MSC_VER=%u", _MSC_VER);
	sb.write(tmp);
	return sb.str();
}
#else
std::string get_build_info()
{
	stringbuffer sb;
	sb.write(build_timestamp);
	sb.put(',');
	sb.put(' ');
	sb.write(build_system);
	return sb.str();
}
#endif

void get_title_text(std::vector<std::string> *lines)
{
	char tmp[1000];
	lines->push_back("============================================");
	lines->push_back("Contents Watcher");
	lines->push_back("Copyright (C) " CW_COPYRIGHT_YEAR " IntelligentWorks Co.,Ltd.");
	lines->push_back("============================================");

	sprintf(tmp, "version: %s.%u", CW_VERSION, build_revision);
	lines->push_back(tmp);

	sprintf(tmp, "build: %s", get_build_info().c_str());
	lines->push_back(tmp);
}


void print_version()
{
	std::vector<std::string> lines;
	get_title_text(&lines);
	for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++) {
		puts(it->c_str());
	}
}

///////////////////////////////////////////////////////////////////////////////

void debug(ContentsWatcher::Option const &option);



void setup_export_dir(ContentsWatcher::Option *option)
{
	std::string exportdir;
	FILE *fp = fopen("/usr/local/cwatcher/conf/export_path", "r");
	if (fp) {
		char tmp[1000];
		if (fgets(tmp, 1000, fp)) {
			char *p = tmp + strlen(tmp);
			while (p > tmp && isspace(p[-1] & 255)) {
				p--;
			}
			p[0] = 0;
			exportdir.assign(tmp);
		}
		fclose(fp);
	}
	option->archive_csv_base_dir = exportdir;
	if (option->archive_csv_base_dir.empty()) {
		option->archive_csv = false;
	}
}



int main2(int argc, char const **argv)
{
#ifdef WIN32
	setlocale(LC_ALL, "Japanese_Japan.932");
#endif // WIN32

	ContentsWatcher::Option option;

	parse_option_ini_file(&option);
	// setup_export_dir(&option);		// (commented out by kitamura)

	TheServer *server = get_the_server();

	int result = 0;

	int operation = OP_NORMAL;

	int pass, argi, i;
	for (pass = 0; pass < 2; pass++) {
		for (argi = 1; argi < argc; argi++) {
			char const *arg = argv[argi];
			if (arg[0] == '-') {
				bool unknownoption = true;
				for (i = 0; optiontable[i].string; i++) {
					size_t l = strlen(optiontable[i].string);
					if (strlen(arg) >= l && (arg[l] == 0 || arg[l] == '=')) {
						if (memcmp(arg, optiontable[i].string, l) == 0) {
							char const *param = "";
							unknownoption = false;
							if (optiontable[i].parameter) {
								if (arg[l] == '=') {
									param = &arg[l + 1];
								} else if (argi + 1 < argc) {
									argi++;
									param = argv[argi];
								}
							}
							switch (optiontable[i].id) {
							case OP_INTERFACE:
								if (pass == 0) {
									option.source_mode = ContentsWatcher::Option::PCAP_LIVE;
									option.interface_text = param;
								}
								break;
							case OP_FILENAME:
								if (pass == 0) {
									option.source_mode = ContentsWatcher::Option::PCAP_FILE;
									option.file_names.push_back(param);
								}
								break;
							case OP_NO_DATABASE:
								if (pass == 0) {
									option.no_database = true;
								}
								break;
							case OP_NO_IP_CHECKSUM:
								if (pass == 0) {
									option.ip_checksum_validation = false;
								}
								break;
							case OP_SNAP_LENGTH:
								if (pass == 0) {
									option.snap_length = strtol(param, 0, 10);
								}
								break;
							case OP_IGNORE_TARGET_SERVERS:
								if (pass == 0) {
									option.ignore_target_servers = true;
								}
								break;
							case OP_IGNORE_DEFECTIVE_PACKET:
								if (pass == 0) {
									option.ignore_defective_packet = true;
								}
								break;
							case OP_NO_SCAN_DEVICES:
								if (pass == 0) {
									option.no_scan_devices = true;
								}
								break;
							case OP_NO_CONSOLE:
								if (pass == 0) {
									option.no_log_console = true;
								}
								break;
							case OP_PCAP_FILTER:
								if (pass == 0) {
									option.pcap_filter = param;
								}
								break;
							case OP_NO_ARCHIVE:
								if (pass == 0) {
									option.archive_csv = false;
								}
								break;
							case OP_SAVE_PACKET:
								if (pass == 0) {
									option.save_packet = strtol(param, 0, 10);
								}
								break;
							case OP_DUMP_PACKET:
								if (pass == 0) {
									option.dump_packet = strtol(param, 0, 10);
								}
								break;
							case OP_NO_LOG:
								if (pass == 1) {
									option.no_log = true;
								}
								break;
							case OP_VERSION:
								if (pass == 1) {
									print_version();
									operation = OP_DONE;
								}
								break;
#ifdef WIN32
							case OP_SERVICE:
								operation = optiontable[i].id;
								break;
							case OP_INSTALL:
								if (pass == 1) {
									service_install();
									operation = OP_DONE;
								}
								break;
							case OP_UNINSTALL:
								if (pass == 1) {
									service_uninstall();
									operation = OP_DONE;
								}
								break;
#endif // WIN32
//#ifdef _DEBUG
							case OP_DEBUG:
								if (pass == 1) {
									debug(option);
									operation = OP_DONE;
								}
								break;
//#endif // _DEBUG
							}
						}
					}
				}
				if (unknownoption) {
					if (pass == 0) {
						fprintf(stderr, "Unknown option '%s'\n", arg);
					}
				}
			} else {
#ifdef WIN32
				if (eq(arg, "start")) {
					if (pass == 1) {
						service_start();
						operation = OP_DONE;
					}
					continue;
				}
				if (eq(arg, "stop")) {
					if (pass == 1) {
						service_stop();
						operation = OP_DONE;
					}
					continue;
				}
				if (eq(arg, "restart") || eq(arg, "reload")) {
					if (pass == 1) {
						service_stop();
						service_start();
						operation = OP_DONE;
					}
					continue;
				}
#else // WIN32
				if (eq(arg, "start")) {
					if (pass == 1) {
						server->set_option(&option);
						result = server->main(true);
						operation = OP_DONE;
					}
					continue;
				}
				if (eq(arg, "stop")) {
					if (pass == 1) {
						daemon_stop();
						operation = OP_DONE;
					}
					continue;
				}
				if (eq(arg, "restart") || eq(arg, "reload")) {
					if (pass == 1) {
						daemon_restart();
						operation = OP_DONE;
					}
					continue;
				}
#endif // WIN32
			}
		}
	}
	server->set_option(&option);
	switch (operation) {
	case OP_NORMAL:
		return server->main(false);
	case OP_SERVICE:
#ifdef WIN32
		{
			SERVICE_TABLE_ENTRY ent[] = { { WINDOWS_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain }, { 0, 0 }, };
			if (!StartServiceCtrlDispatcher(ent)) {
				return 1;
			}
		}
#else // WIN32
		return server->main(true);
#endif // WIN32
	}
	return result;
}

void parse_response_file(char const *path, std::vector<std::string> *out)
{
	int fd = open(path, O_RDONLY | O_BINARY);
	if (fd == -1) {
		fprintf(stderr, "could not open file '%s'\n", path);
		return;
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		fprintf(stderr, "fstat failed\n");
		return;
	}

	std::vector<char> buffer(st.st_size);
	read(fd, &buffer[0], st.st_size);

	char const *ptr = &buffer[0];
	char const *end = ptr + st.st_size;

	while (ptr < end) {
		char const *next;
		while (ptr < end && isspace(*ptr)) {
			ptr++;
		}
		next = ptr;
		while (next < end && !isspace(*next)) {
			next++;
		}
		if (ptr < next) {
			std::string s(ptr, next);
			out->push_back(s);
		}
		ptr = next;
	}

	close(fd);
}

void initialize_mynew();
void terminate_mynew();

void initialize_the_server();
void terminate_the_server();


int main(int argc, char const **argv)
{
	initialize_mynew();

	std::vector<char const *> vec;
	std::vector<std::string> values;

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '@') {
			size_t j = values.size();
			parse_response_file(argv[i] + 1, &values);
			while (j < values.size()) {
				vec.push_back(values[j].c_str());
				j++;
			}
		} else {
			vec.push_back(argv[i]);
		}
	}

	vec.push_back(0);

	initialize_the_server();
	int ret = main2(vec.size() - 1, &vec[0]);
	terminate_the_server();

	terminate_mynew();

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
