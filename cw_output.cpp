#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "common/text.h"
#include "common/eq.h"
#include "common/calendar.h"
#include "common/datetime.h"
#include "common/combinepath.h"
#include "common/mkdirs.h"

#include "cw_server.h"



#include "common/jcode/memoryreader.h"
#include "common/jcode/stringwriter.h"
std::string utf8_to_sjis(std::string const &str)
{
	memoryreader r(str.c_str(), str.size());
	stringwriter w;
	soramimi::jcode::reader_state_t rs;
	soramimi::jcode::writer_state_t ws;
	setup_reader_state(&rs, soramimi::jcode::UTF8);
	setup_writer_state(&ws, soramimi::jcode::SJIS);
	convert(&rs, &ws, &r, &w);
	return w.str();
}


size_t diagnostic_event_output_queue_size = 0;



void csvwriter_t::close()
{
	if (_fp_csv) {
		::fclose(_fp_csv);
		_fp_csv = 0;
	}
}

void csvwriter_t::determinate_counters()
{

}

std::string csvwriter_t::get_dir(YMD const &ymd)
{
	char tmp[100];
	sprintf(tmp, "cwcsv/%u/%02u/%02u", ymd.year, ymd.month, ymd.day);
	return CombinePath(_basedir.c_str(), tmp, '/');
}

std::string get_csv_file_name(int number)
{
	char tmp[1000];
	sprintf(tmp, "%u.csv", number);
	return tmp;
}

void csvwriter_t::write(time_t timestamp, std::string const &line)
{
	YMD ymd = timestamp;

	if (ymd != _date) {
		_file_number = 1;
		close();
	}

	if (!_fp_csv) { // ファイルがオープンされていないなら

		_date = YMD();

		std::string dir = get_dir(ymd); // ディレクトリ名取得

		if (!mkdirs(dir.c_str())) { // ディレクトリ作成
			fprintf(stderr, "could not create csv directory '%s'\n", dir.c_str());
			return;
		}

		_line_number = 0; // 行番号＝ゼロ

		std::string path;

		// 最終ファイル番号を取得
		std::string latest_path = CombinePath(dir.c_str(), "latest", '/');
		{
			int fd1 = ::open(latest_path.c_str(), O_RDONLY); // latestファイルを開く

			if (fd1 == -1) {
				fd1 = ::open(latest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				::write(fd1, "1", 1);
				::close(fd1);
			} else {

				{
					char tmp[100];
					int n = ::read(fd1, tmp, sizeof(tmp) - 1);
					tmp[n] = 0;
					_file_number = atoi(tmp); // 最終ファイル番号
				}
				std::string filename = get_csv_file_name(_file_number); // csvファイルを開く
				path = CombinePath(dir.c_str(), filename.c_str(), '/');
#ifndef O_BINARY
#define O_BINARY 0
#endif
				int fd2 = ::open(path.c_str(), O_RDONLY | O_BINARY);
				if (fd2 != -1) {
					struct stat st;
					if (fstat(fd2, &st) == 0) {
						// 改行を数える
						std::vector<unsigned char> vec;
						vec.resize(st.st_size);
						unsigned char *ptr = &vec[0];
						int len = ::read(fd2, ptr, st.st_size);
						unsigned char *end = ptr + len;
						for (unsigned char *p = ptr; p < end; p++) {
							if (*p == '\n') {
								_line_number++;
							}
						}
					}
					::close(fd2);
				}				
				::close(fd1);
			}
		}

		if (_file_number < 1) {
			_file_number = 1;
		}

		if (path.empty()) {
			std::string filename = get_csv_file_name(_file_number);
			path = CombinePath(dir.c_str(), filename.c_str(), '/');
		}

		_fp_csv = ::fopen(path.c_str(), "ab");
		if (!_fp_csv) {
			fprintf(stderr, "could not create csv file '%s'\n", path.c_str());
			return;
		}

		fseek(_fp_csv, 0, SEEK_END);

		_date = ymd;
	}

	if (ftell(_fp_csv) == 0) {
		if (!_header.empty()) {
			fwrite(_header.c_str(), 1, _header.size(), _fp_csv);
			putc('\r', _fp_csv);
			putc('\n', _fp_csv);
		}
	}

	fwrite(line.c_str(), 1, line.size(), _fp_csv);
	putc('\r', _fp_csv);
	putc('\n', _fp_csv);
	fflush(_fp_csv);

	_line_number++;

	if (_line_number >= CSV_FILE_LINES) {
		close();
		_file_number++;

		std::string dir = get_dir(ymd);
		std::string latest_path = CombinePath(dir.c_str(), "latest", '/');
		int fd3 = ::open(latest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd3 != -1) {
			char tmp[100];
			sprintf(tmp, "%u", _file_number);
			::write(fd3, tmp, strlen(tmp));
			::close(fd3);
		}
	}
}




struct command_translator_t {
	std::map<std::string, std::string> map;
	command_translator_t()
	{
		map["mknew"]               = utf8_ustring_to_string((uchar_t const *)L"新規作成[Windows]");
		map["opendir"]             = utf8_ustring_to_string((uchar_t const *)L"フォルダを開く");
		map["login"]               = utf8_ustring_to_string((uchar_t const *)L"ログイン");
		map["logout"]              = utf8_ustring_to_string((uchar_t const *)L"ログアウト");
		map["mkdir"]               = utf8_ustring_to_string((uchar_t const *)L"フォルダ作成");
		map["rmdir"]               = utf8_ustring_to_string((uchar_t const *)L"フォルダ削除");
		map["unlink"]              = utf8_ustring_to_string((uchar_t const *)L"ファイル削除");
		map["remove"]              = utf8_ustring_to_string((uchar_t const *)L"ファイル削除");
		map["read"]                = utf8_ustring_to_string((uchar_t const *)L"ファイル読み込み");
		map["readext"]             = utf8_ustring_to_string((uchar_t const *)L"ファイル読み込み");
		map["write"]               = utf8_ustring_to_string((uchar_t const *)L"ファイル書き込み");
		map["writeext"]            = utf8_ustring_to_string((uchar_t const *)L"ファイル書き込み");
		map["create"]              = utf8_ustring_to_string((uchar_t const *)L"ファイル作成");
		map["rename"]              = utf8_ustring_to_string((uchar_t const *)L"名前の変更");
		map["moveandrename"]       = utf8_ustring_to_string((uchar_t const *)L"移動＆名前の変更[Mac]");
		map["copy"]                = utf8_ustring_to_string((uchar_t const *)L"コピー[Mac]");
		map["exchange"]            = utf8_ustring_to_string((uchar_t const *)L"OSXのファイル更新");
		map["mnt"]                 = utf8_ustring_to_string((uchar_t const *)L"マウント");
		map["umnt"]                = utf8_ustring_to_string((uchar_t const *)L"アンマウント");
		map["symlink"]             = utf8_ustring_to_string((uchar_t const *)L"シンボリックリンク作成");
		map["mknod"]               = utf8_ustring_to_string((uchar_t const *)L"特殊デバイス作成");
		map["link"]                = utf8_ustring_to_string((uchar_t const *)L"ハードリンク作成");
		map["setattr"]             = utf8_ustring_to_string((uchar_t const *)L"属性変更");
	}
} command_translator;









namespace ContentsWatcher {

	EventProcessor::EventProcessor()
	{
		_logger = 0;
		_alert_target_map_last_update_time = 0;
		_archive_csv_enable = true;
	}

	EventProcessor::~EventProcessor()
	{
		close();
	}

	void EventProcessor::setup(Logger *logger)
	{
		_logger = logger;
	}

	void EventProcessor::setup_sqlite()
	{
#ifdef DB_SUPPORT_SQLITE
		db_parameter.dbmode = DBMODE_SQLITE;
		db_parameter.text = "/var/log/cwatcher/test.db";

		unlink(db_parameter.text.c_str());

		db_driver.create(&db_parameter);

		db_connection.connect(&db_driver, &db_parameter);

		DB_Statement st;
		st.begin_transaction(&db_connection, 0);
		{
			char const *sql = "create table alert_esc ("
				"id integer primary key autoincrement,"
				"path text,"
				"server text,"
				"type integer)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table alert_mail ("
				"id integer primary key autoincrement,"
				"alert_mail text)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table alert_target ("
				"id integer primary key autoincrement,"
				"unixpath text,"
				"act_mkdir integer,"
				"act_rmdir integer,"
				"act_create integer,"
				"act_unlink integer,"
				"act_read integer,"
				"act_write integer,"
				"act_rename integer,"
				"act_move integer,"
				"act_copy integer,"
				"act_osxfile integer,"
				"is_fldr integer default 1,"
				"act_mknew integer,"
				"server text,"
				"act_open integer,"
				"act_mnt integer,"
				"act_umnt integer,"
				"act_symlink integer,"
				"act_mknod integer,"
				"act_link integer,"
				"act_setattr integer)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table csv_view ("
				"accessdate datetime,"
				"clientip text,"
				"serverip text,"
				"username text,"
				"filepath text,"
				"cmd text,"
				"dstpath text,"
				"extinfo text,"
				"status text,"
				"protocol text,"
				"folk text,"
				"alert integer,"
				"macaddr text,"
				"alert_sent integer,"
				"uniqid text,"
				"u_name text,"
				"id integer primary key autoincrement)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table logfile ("
				"accessdate timestamp not null,"
				"clientip text,"
				"serverip text,"
				"username text,"
				"filepath text,"
				"cmd text,"
				"dstpath text"
				"extinfo text"
				"status text,"
				"protocol text,"
				"folk text,"
				"alert integer default 0,"
				"macaddr text,"
				"alert_sent tinyint(4) default 0,"
				"id integer primary key autoincrement)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table pattern ("
				"id integer primary key autoincrement,"
				"esc_name text,"
				"esc_num integer)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table pref ("
				"id integer primary key autoincrement,"
				"admin_pwd text,"
				"pdf_pwd text,"
				"admin_id text,"
				"alert_minute integer,"
				"savetime text,"
				"pdf_switch integer,"
				"from_mail text,"
				"usr_id text default 'rs',"
				"usr_pwd text default 'rs',"
				"in_hdd integer default 0,"
				"out_hdd integer default 0,"
				"csv_dump_switch integer default '1',"
				"csv_dump_location integer default 0,"
				"keep_log_days integer default '14',"
				"other_disk_alert integer default 0,"
				"csv_dump_timing integer default 0,"
				"usr_sort integer default 0,"
				"smtp_use integer default 0,"
				"smtp_server text,"
				"smtp_port integer,"
				"smtp_attest integer default 0,"
				"smtp_username text,"
				"smtp_pass text)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table user ("
				"id integer primary key autoincrement,"
				"logname text,"
				"email text,"
				"name text,"
				"kana text,"
				"esc integer,"
				"type integer default 0)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table user_gid ("
				"id integer primary key autoincrement,"
				"gid text,"
				"name text,"
				"kana text,"
				"esc integer)";
			st.prepare(sql);
			st.execute();
		}
		{
			char const *sql = "create table user_uid ("
				"id integer primary key autoincrement,"
				"logname text,"
				"name text,"
				"kana text,"
				"email text,"
				"esc integer,"
				"type integer default 0)";
			st.prepare(sql);
			st.execute();
		}
		st.commit_transaction();
#endif // DB_SUPPORT_SQLITE
	}

	void EventProcessor::open()
	{
		AutoLock(the_server->get_mutex_for_settings());

		Option const *option = the_server->get_option();

		db_parameter.dbmode = DBMODE_MYSQL;
		db_parameter.host = option->database.server;
		db_parameter.user = option->database.uid;
		db_parameter.pass = option->database.pwd;
		db_parameter.name = option->database.name;

		db_driver.create(&db_parameter);


#if 0
		setup_sqlite();
#endif
	}

	void EventProcessor::close()
	{
	}

	void EventProcessor::start()
	{
		open();
		Thread::start();
	}

	void EventProcessor::stop()
	{
		Thread::stop();
		_event.signal();
		Thread::join();
		close();
	}

	static char const *protocolname(file_access_event_item_t const &item)
	{
		switch (item->protocol) {
		case file_access_event_item_t::P_SMB:
			return "SMB";
			break;
		case file_access_event_item_t::P_SMB2:
			return "SMB2";
			break;
		case file_access_event_item_t::P_NFS:
			return "NFS";
			break;
		case file_access_event_item_t::P_AFP:
			return "AFP";
			break;
		}
		assert(0);
		return "UNKNOWN";
	}

	struct alert_item_t {
		unsigned long long id;
		int alert;
	};


	void EventProcessor::make_alert_user_flags_(DB_Statement *st)
	{
		_alert_user_flags.clear();
		char const *sql = "select type,logname from user where esc=0";
		DBVARIANT var(st->get_dbmode());
		st->prepare(sql);
		st->execute();
		while (st->fetch()) {
			alert_user_t t;
			t.type = (alert_type_t)st->column_value(0).get_integer();
			t.text = utf8_string_to_ustring(st->column_value(1).get_string());
			_alert_user_flags.insert(_alert_user_flags.end(), t);
		}
	}

	void EventProcessor::make_alert_target_map_(DB_Statement *st)
	{
		_alert_target_map.clear();

		//                        0  1        2         3         4          5          6        7         8          9        10       11          12      13        14     15       16      17       18          19        20       21          22
		char const *sql = "select id,unixpath,act_mkdir,act_rmdir,act_create,act_unlink,act_read,act_write,act_rename,act_move,act_copy,act_osxfile,is_fldr,act_mknew,server,act_open,act_mnt,act_umnt,act_symlink,act_mknod,act_link,act_setattr,server from alert_target"; 

		st->prepare(sql);
		st->execute();

		while (st->fetch()) {
			alert_target_row_t a;
			a.alert_id          = st->column_value( 0).get_integer();
			a.unixpath          = eucjp_string_to_ustring(st->column_value(1).get_string());
			a.act_mkdir         = st->column_value( 2).get_integer() != 0;
			a.act_rmdir         = st->column_value( 3).get_integer() != 0;
			a.act_create        = st->column_value( 4).get_integer() != 0;
			a.act_unlink        = st->column_value( 5).get_integer() != 0;
			a.act_read          = st->column_value( 6).get_integer() != 0;
			a.act_write         = st->column_value( 7).get_integer() != 0;
			a.act_rename        = st->column_value( 8).get_integer() != 0;
			a.act_move          = st->column_value( 9).get_integer() != 0;
			a.act_copy          = st->column_value(10).get_integer() != 0;
			a.act_osxfile       = st->column_value(11).get_integer() != 0;
			a.is_fldr           = st->column_value(12).get_integer() != 0;
			a.act_mknew         = st->column_value(13).get_integer() != 0;
			a.server            = st->column_value(14).get_string();
			a.act_open          = st->column_value(15).get_integer() != 0;
			a.act_mnt           = st->column_value(16).get_integer() != 0;
			a.act_umnt          = st->column_value(17).get_integer() != 0;
			a.act_symlink       = st->column_value(18).get_integer() != 0;
			a.act_mknod         = st->column_value(19).get_integer() != 0;
			a.act_link          = st->column_value(20).get_integer() != 0;
			a.act_setattr       = st->column_value(21).get_integer() != 0;
			ustring server      = utf8_string_to_ustring(st->column_value(22).get_string());

			_alert_target_map[server].push_back(a);
		}
	}

	void EventProcessor::make_alert_info(DB_Statement *st)
	{
		const int refresh_interval = 60;

		time_t now = time(0);
		if (now - _alert_target_map_last_update_time >= refresh_interval) {
			_alert_target_map_last_update_time = now;
			make_alert_user_flags_(st);
			make_alert_target_map_(st);
			_logger->log_printf(0, "alert info reloaded");
		}
	}

	bool EventProcessor::check_alert_user(DB_Statement *st, file_access_event_item_t const &item)
	{
		for (int type = 0; type < 3; type++) {
			alert_user_t t;
			t.type = (alert_type_t)type;
			switch (t.type) {
			case USERNAME:
				if (item->user_name.empty()) {
					continue;
				}
				t.text = item->user_name;
				break;
			case IPADDRESS:
				if (item->client_addr.empty()) {
					continue;
				}
				t.text = utf8_string_to_ustring(item->client_addr.tostring());
				break;
			case MACADDRESS:
				if (item->mac_address.empty()) {
					continue;
				}
				t.text = utf8_string_to_ustring(item->mac_address.tostring());
				break;
			}
			std::set<alert_user_t>::const_iterator it = _alert_user_flags.find(t);
			if (it != _alert_user_flags.end()) {
				return true;
			}
		}
		return false;
	}

	void EventProcessor::update_alert(DB_Statement *st, file_access_event_item_t const &item, unsigned long long log_id, char const *cmd)
	{
		ustring filepath = item->text1;

		make_alert_info(st);

		//アラート対象外ユーザーかチェック
		//if (!check_alert_user(st, item)) {
		//	return;
		//}

		std::vector<alert_item_t> alert_array;

		{
			ustring addr = utf8_string_to_ustring(item->server_addr.tostring());
			std::map<ustring, alert_target_list_t>::const_iterator it1 = _alert_target_map.find(addr);
			if (it1 != _alert_target_map.end()) {
				for (alert_target_list_t::const_iterator it2 = it1->second.begin(); it2 != it1->second.end(); it2++) {

					//unixpathがフォルダだった場合		
					if (it2->is_fldr) {
						//ファイルパス名がユニックスパスで始まるかどうかチェック
						if (start_with_ic(filepath.c_str(), it2->unixpath.c_str())) { // $check = eregi("^".$unixpath, $filepath); 
							goto check;
						}
						if (it2->act_open == 1) {
							ustring unixpath = it2->unixpath;
							size_t n = unixpath.size();
							if (n > 0 && unixpath[n - 1] == '/') {
								unixpath = ustring(unixpath.c_str(), n - 1);
							}
							if (unixpath == filepath) {
								goto check;
							}
						}
					} else { //ファイルの場合
						if (it2->unixpath == filepath) {
							goto check;
						}
					}

					continue;

check:			//ファイルパスとユニックスパスがマッチした場合コマンドでチェック

					if (it2->act_mkdir) {
						if (eq(cmd, "mkdir")) {
							goto alert;
						}
					}
					if (it2->act_rmdir) {
						if (eq(cmd, "rmdir")) {
							goto alert;
						}
					}
					if (it2->act_create) {
						if (eq(cmd, "create")) {
							goto alert;
						}
					}
					if (it2->act_unlink) {
						if (eq(cmd, "unlink") || eq(cmd, "remove")) {
							goto alert;
						}
					}
					if (it2->act_read) {
						if (eq(cmd, "read") || eq(cmd, "readext")) {
							goto alert;
						}
					}
					if (it2->act_write) {
						if (eq(cmd, "write") || eq(cmd, "writeext")) {
							goto alert;
						}
					}
					if (it2->act_rename) {
						if (eq(cmd, "rename")) {
							goto alert;
						}
					}
					if (eq(cmd, "move")) {
						if (it2->act_move) {
							goto alert;
						}
					}
					if (it2->act_move) {
						if (eq(cmd, "moveandrename")) {
							goto alert;
						}
					}
					if (it2->act_copy) {
						if (eq(cmd, "copy")) {
							goto alert;
						}
					}
					if (it2->act_mknew) {
						if (eq(cmd, "mknew")) {
							goto alert;
						}
					}
					if (it2->act_osxfile) {
						if (eq(cmd, "osxfile") || eq(cmd, "exchange")) {
							goto alert;
						}
					}
					if (it2->act_open) {
						if (eq(cmd, "opendir")) {
							goto alert;
						}
					}
					if (it2->act_mnt) {
						if (eq(cmd, "mnt")) {
							goto alert;
						}
					}
					if (it2->act_umnt) {
						if (eq(cmd, "umnt")) {
							goto alert;
						}
					}
					if (it2->act_symlink) {
						if (eq(cmd, "symlink")) {
							goto alert;
						}
					}
					if (it2->act_mknod) {
						if (eq(cmd, "mknod")) {
							goto alert;
						}
					}
					if (it2->act_link) {
						if (eq(cmd, "link")) {
							goto alert;
						}
					}
					if (it2->act_setattr) {
						if (eq(cmd, "setattr")) {
							goto alert;
						}
					}

					continue;

alert:
					alert_item_t item;
					item.alert = it2->alert_id;
					item.id = log_id;
					alert_array.push_back(item);
				}
			}
		}

		if (!alert_array.empty()) {
			// アラートフラグを更新
			for (std::vector<alert_item_t>::const_iterator it = alert_array.begin(); it != alert_array.end(); it++) {
				char const *sql = "update logfile set alert=? where id=?"; 
				DBVARIANT var0(st->get_dbmode());
				DBVARIANT var1(st->get_dbmode());
				var0.assign_integer(it->alert);
				var1.assign_longlong((long long)it->id);
				st->prepare(sql);
				st->bind_parameter(0, &var0);
				st->bind_parameter(1, &var1);
				st->execute();
			}
		}
	}

	void EventProcessor::check_db_connection(DB_Connection *db)
	{
		if (!db->is_valid_handle()) {
			db->connect(&db_driver, &db_parameter);
			_logger->log_printf(0, "connected to database");
		}
	}

	static inline bool is_need_quote(char const *p)
	{
		while (*p) {
			if (*p == ',' || *p == '"') {
				return true;
			}
			p++;
		}
		return false;
	}

	static std::string get_csv_header_line()
	{
		//$log=$id.",".$datetime.",".$serverip.",".$cmd."(".$cmd1."),\"".$filepath."\",\"".$dstpath."\",".$protocol.",\"".$u_name."\",".$username.",".$clientip.",".$macaddr.",".$alert."\n";
		//      1  2        3        4   5        6       7        8      9        10       11      12
		return "id,datetime,serverip,cmd,filepath,dstpath,protocol,u_name,username,clientip,macaddr,alert";
	}

	static std::string make_csv_line(unsigned long long id, time_t timestamp, file_access_event_item_t const &item)
	{
		std::vector<std::string> row;

		// field 1: id

		{
			char tmp[100];
			sprintf(tmp, "%llu", id);
			row.push_back(tmp);
		}

		// fields 2: datetime

		{
			char tmp[100];
			struct tm tm;
			localtime_r(&timestamp, &tm);
			sprintf(tmp, "%u/%02u/%02u %02u:%02u", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
			row.push_back(tmp);
		}

		// field 3: server address

		{
			row.push_back(item->server_addr.tostring());
		}

		// field 4: command

		{
			std::string desc;
			std::map<std::string, std::string>::const_iterator it = command_translator.map.find(item->command);
			if (it != command_translator.map.end()) {
				desc = it->second;
			} else {
				desc = item->command;
			}
			char tmp[100];
			sprintf(tmp, "%s(%s)", item->command.c_str(), desc.c_str());
			row.push_back(tmp);
		}

		// field 5: path 1

		{
			row.push_back(utf8_ustring_to_string(item->text1));
		}

		// field 6: path 2

		{
			row.push_back(utf8_ustring_to_string(item->text2));
		}

		// field 7: protocol

		{
			row.push_back(protocolname(item));
		}

		// field 8: user name 1

		{
			row.push_back(utf8_ustring_to_string(item->user_name));
		}

		// field 9: user name 2

		{
			row.push_back(utf8_ustring_to_string(item->user_name));
		}

		// field 10: client address

		{
			row.push_back(item->client_addr.tostring());
		}

		// field 11: mac address

		{
			row.push_back(item->mac_address.tostring());
		}

		// field 12: alert

		{
			row.push_back("0");
		}

		//

		stringbuffer line;

		size_t i, n;
		n = row.size();
		for (i = 0; i < n; i++) {
			if (i > 0) {
				line.put(',');
			}
			char const *p = row[i].c_str();
			if (is_need_quote(p)) {
				line.put('"');
				line.write(p);
				line.put('"');
			} else {
				line.write(p);
			}
		}

		return line.c_str();
	}

	void EventProcessor::writecsv(unsigned long long id, file_access_event_item_t const &item)
	{
		time_t timestamp = (time_t)(item->time_us / 1000000);
		std::string line = make_csv_line(id, timestamp, item);

		line = utf8_to_sjis(line);

		_csvwriter.write(timestamp, line.c_str());


	}

	// ログ削除
	void EventProcessor::autodelete(DB_Connection *db)
	{
		DB_Statement st1;
		st1.begin_transaction(db, 0);

		st1.prepare("select keep_log_days from pref");
		st1.execute();
		if (!st1.fetch()) {
			return;
		}
		int days = st1.column_value(0).get_integer(); // days日前まで保存する。それより古いログは削除。

		time_t t = time(0);
		struct tm tm;
		localtime_r(&t, &tm);

		int j = ToJDN(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday) - days;
		YMD ymd = ToYMD(j);

		tm.tm_year = ymd.year - 1900;
		tm.tm_mon = ymd.month - 1;
		tm.tm_mday = ymd.day;
#if 0
		st1.prepare("select min(id) from logfile");
		st1.execute();
		if (st1.fetch()) {
			unsigned long long id = st1.column_value(0).get_longlong();
			st1.prepare("delete from logfile where id=? and accessdate<?");
			do {
				DBVARIANT var0(db->get_dbmode());
				DBVARIANT var1(db->get_dbmode());
				var0.assign_longlong(id);
				var1.assign_datetime(&tm);
				st1.bind_parameter(0, &var0);
				st1.bind_parameter(1, &var1);
				st1.execute();
				putchar('.');
				id++;
			} while (st1.get_affected_rows() == 1);
		}
#else
		st1.prepare("select min(id),max(id) from logfile");
		st1.execute();
		if (st1.fetch()) {
			unsigned long long id = 0;
			{ // バイナリサーチで、削除するidを決める。
				unsigned long long lower = st1.column_value(0).get_longlong();
				unsigned long long upper = st1.column_value(1).get_longlong() + 1;
				while (lower + 1 < upper) {
					id = (lower + upper) / 2;
					st1.prepare("select 1 from logfile where id=? and accessdate<?");
					DBVARIANT var0(db->get_dbmode());
					DBVARIANT var1(db->get_dbmode());
					var0.assign_longlong(id);
					var1.assign_datetime(&tm);
					st1.bind_parameter(0, &var0);
					st1.bind_parameter(1, &var1);
					st1.execute();
					if (st1.fetch()) {
						lower = id + 1;
					} else {
						upper = id;
					}
				}

				id = lower;
			}
			// 削除
			st1.prepare("delete from logfile where id<?");
			DBVARIANT var0(db->get_dbmode());
			var0.assign_longlong(id);
			st1.bind_parameter(0, &var0);
			st1.execute();
			unsigned long long n = st1.get_affected_rows();
			if (n > 0) {
				_logger->log_printf(0, "log table: %llu rows deleted", n);
			}
		}
#endif
		st1.commit_transaction();
	}

	// キューからイベントを取り出しつつ、データベースに保存する。
	void EventProcessor::run()
	{
		DB_Connection db_connection;

		time_t db_last_access = 0;

		_csvwriter.setbasedir(_archive_csv_base_dir);

		_csvwriter.setheader(get_csv_header_line());

		while (1) {
			try {
				std::list<file_access_event_item_t> q;
				{
					AutoLock lock(&_mutex);
					std::swap(q, _queue);
					diagnostic_event_output_queue_size = q.size();
				}

				// database health check
				try {
					time_t now = time(0);
					if (now - db_last_access > 10) {
						check_db_connection(&db_connection);

						{
							DB_Statement st;
							st.begin_transaction(&db_connection, 0);
							int n = 0;
							st.query_get_integer("select 12345", &n);
							if (n == 12345) {
								_logger->log_printf(0, "database health check: ok");
							}
						}

						// cleanup

						autodelete(&db_connection);

						//

						db_last_access = now;
					}
				} catch (DBException const &e) {
					_logger->log_printf(0, "database error: %s", e.get_message());
					db_connection.disconnect();
				}

				if (q.empty()) {
					if (_terminate) {
						break;
					}

					// wait

					_event.wait(10 * 60 * 1000L); // 10min

				} else {
					try {
						check_db_connection(&db_connection);

						the_server->update_exclusion_info(&db_connection);

						DB_Statement st;
						st.begin_transaction(&db_connection, 0);

						int insertcount = 0;

						for (std::list<file_access_event_item_t>::const_iterator it = q.begin(); it != q.end(); it++) {
							file_access_event_item_t const &item = *it;
							ustring path = item->text1;

							if (end_with(path.c_str(), (uchar_t const *)L".DS_Store")) {
								continue; // 除外
							}

							if (the_server->is_excluded(item->server_addr, path.c_str())) {
								continue; // 除外
							}

							char const *sql = "insert into logfile ("
								"accessdate,"
								"clientip,"
								"serverip,"
								"username,"
								"filepath,"
								"cmd,"
								"dstpath,"
								"protocol,"
								"macaddr)"
								" values (?,?,?,?,?,?,?,?,?)";
							dbmode_t mode = db_connection.get_dbmode();
							DBVARIANT var0(mode);
							DBVARIANT var1(mode);
							DBVARIANT var2(mode);
							DBVARIANT var3(mode);
							DBVARIANT var4(mode);
							DBVARIANT var5(mode);
							DBVARIANT var6(mode);
							DBVARIANT var7(mode);
							DBVARIANT var8(mode);
							var0.assign_datetime((time_t)(item->time_us / 1000000));
							var1.assign_string(item->client_addr.tostring());
							var2.assign_string(item->server_addr.tostring());
							if (item->user_name.empty()) {
								var3.assign_string((char const *)0, (size_t)0);
							} else {
								var3.assign_string_eucjp(item->user_name);
							}
							var4.assign_string_eucjp(path);
							var5.assign_string(item->command);
							var6.assign_string_eucjp(item->text2);
							var7.assign_string(protocolname(item));
							var8.assign_string(item->mac_address.tostring());

							st.prepare(sql);
							st.reserve_parameter(9);
							st.bind_parameter(0, &var0);
							st.bind_parameter(1, &var1);
							st.bind_parameter(2, &var2);
							st.bind_parameter(3, &var3);
							st.bind_parameter(4, &var4);
							st.bind_parameter(5, &var5);
							st.bind_parameter(6, &var6);
							st.bind_parameter(7, &var7);
							st.bind_parameter(8, &var8);
							st.execute();

							unsigned long long id = st.x_mysql_insert_id();
							update_alert(&st, *it, id, (*it)->command.c_str());

							if (_archive_csv_enable) {
								writecsv(id, *it);
							}

							insertcount++;
						}
						st.commit_transaction();

						if (false) {
							if (insertcount > 0) {
								_logger->log_printf(0, "log table: %u rows inserted", insertcount);
							}
						}

					} catch (DBException const &e) {
						_logger->log_printf(0, "database error: %s", e.get_message());
						db_connection.disconnect();
					}

					db_last_access = time(0);
				}

			} catch (std::bad_alloc const &e) {
				_csvwriter.close();
				on_bad_alloc(e, __FILE__, __LINE__);
			}
		}
		_csvwriter.close();
	}

	void EventProcessor::emit_event(std::string const &command, file_access_event_item_t &item)
	{
		Option const *option = the_server->get_option();
		if (option->no_database) {
			return;
		}
		//AutoLock lock(&_mutex); // 呼び出し元でロックすること
		if (_queue.size() < CW_OUTPUT_QUEUE_LIMIT) {
			item->command = command;
			_queue.push_back(item);
			_event.signal();
		}
	}

	void EventProcessor::on_file_access(file_access_event_item_t &item)
	{
	}

	void EventProcessor::on_login(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s (login) %s"
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			);
		emit_event("login", item);
	}

	void EventProcessor::on_login_failed(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s (login failed) %s"
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			);
		emit_event("loginfailed", item);
	}

	void EventProcessor::on_logout(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s (logout) %s"
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			);
		emit_event("logout", item);
	}

	void EventProcessor::on_create_file(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (create file) \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			);
		emit_event("create", item);
	}

	void EventProcessor::on_create_dir(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (create dir) \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			);
		emit_event("mkdir", item);
	}

	void EventProcessor::on_exchange(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (exchange) \"%s\" \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			, utf8_ustring_to_string(item->text2).c_str()
			);
		emit_event("exchange", item);
	}

	void EventProcessor::on_rename(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (rename) \"%s\" \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			, utf8_ustring_to_string(item->text2).c_str()
			);
		emit_event("rename", item);
	}

	void EventProcessor::on_copy(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (copy) \"%s\" \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			, utf8_ustring_to_string(item->text2).c_str()
			);
		emit_event("copy", item);
	}

	void EventProcessor::on_move_and_rename(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (move and rename) \"%s\" \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			, utf8_ustring_to_string(item->text2).c_str()
			);
		emit_event("moveandrename", item);
	}

	void EventProcessor::on_delete_file(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (delete file) \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			);
		emit_event("remove", item);
	}

	void EventProcessor::on_delete_dir(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (delete dir) \"%s\""
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			);
		emit_event("rmdir", item);
	}

	void EventProcessor::on_read(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (read file) \"%s\" %ubytes"
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			, (unsigned int)item->size
			);

		if (the_server->get_exclusion_command_info()->cmd_read) {
			// readコマンド除外
			return;
		}

		emit_event("read", item);
	}

	void EventProcessor::on_write(file_access_event_item_t &item)
	{
		_logger->log_printf(item->time_us, "[%s] C=%s S=%s U=%s (write file) \"%s\" %ubytes"
			, protocolname(item)
			, item->client_addr.tostring().c_str()
			, item->server_addr.tostring().c_str()
			, utf8_ustring_to_string(item->user_name).c_str()
			, utf8_ustring_to_string(item->text1).c_str()
			, (unsigned int)item->size
			);
		emit_event("write", item);
	}

} // namespace ContentsWatcher



void debug(ContentsWatcher::Option const &option)
{
	DB_Parameter db_param;
	db_param.dbmode = DBMODE_MYSQL;
	db_param.host = option.database.server;
	db_param.name = option.database.name;
	db_param.user = option.database.uid;
	db_param.pass = option.database.pwd;
	DB_Driver db_driver;
	db_driver.create(&db_param);
	DB_Connection db;
	db.connect(&db_driver, &db_param);

	DB_Statement st1;
	st1.begin_transaction(&db, 0);

	time_t t = time(0);
	struct tm tm;
	localtime_r(&t, &tm);

	st1.prepare("select keep_log_days from pref");
	st1.execute();
	if (!st1.fetch()) {
		return;
	}
	int days = st1.column_value(0).get_integer(); // days日前まで保存する。それより古いログは削除。

	int j = ToJDN(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday) - days;
	YMD ymd = ToYMD(j);

	tm.tm_year = ymd.year - 1900;
	tm.tm_mon = ymd.month - 1;
	tm.tm_mday = ymd.day;

	st1.prepare("select min(id),max(id) from logfile");
	st1.execute();
	if (st1.fetch()) {
		unsigned long long id = 0;
		{ // バイナリサーチで、削除するidを決める。
			unsigned long long lower = st1.column_value(0).get_longlong();
			unsigned long long upper = st1.column_value(1).get_longlong() + 1;
			while (lower + 1 < upper) {
				id = (lower + upper) / 2;
				printf("lower %llu, upper: %llu, id: %llu\n", lower, upper, id);
				st1.prepare("select 1 from logfile where id=? and accessdate<?");
				DBVARIANT var0(db.get_dbmode());
				DBVARIANT var1(db.get_dbmode());
				var0.assign_longlong(id);
				var1.assign_datetime(&tm);
				st1.bind_parameter(0, &var0);
				st1.bind_parameter(1, &var1);
				st1.execute();
				if (st1.fetch()) {
					lower = id + 1;
				} else {
					upper = id;
				}
			}

			id = lower;
			printf("id %llu\n", id);
		}
	}

}

