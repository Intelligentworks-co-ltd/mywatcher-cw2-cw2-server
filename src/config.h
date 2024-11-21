#define CW_VERSION "2.3.2"
#define CW_COPYRIGHT_YEAR "2009-2021"

// ログ出力キューの上限
#define CW_LOG_QUEUE_LIMIT 100000

// パケットキャプチャキューの上限
#define CW_PACKET_CAPTURE_QUEUE_LIMIT 500000

// 出力（DB格納待ち）キューの上限
#define CW_OUTPUT_QUEUE_LIMIT 10000

// CSVファイル内の行数
#define CSV_FILE_LINES 10000

#ifdef WIN32
#define CW_INI_FILE "C:/usr/local/cwatcher/conf/cwatcher.ini"
#define CW_TARGETSERVERS_FILE "C:/usr/local/cwatcher/conf/targetservers"
#define CW_EXCLUSION_COMMAND_INFO_FILE "C:/usr/local/cwatcher/conf/cmd_escape"
#else
#define CW_INI_FILE "/usr/local/cwatcher/conf/cwatcher.ini"
#define CW_TARGETSERVERS_FILE "/usr/local/cwatcher/conf/targetservers"
#define CW_EXCLUSION_COMMAND_INFO_FILE "/usr/local/cwatcher/conf/cmd_escape"
#endif

#define CW_LAST_PACKET_FILE "/tmp/lastpacket.pcap"
#define CW_PACKET_DUMP_FILE "/var/log/cwatcher/capture.pcap"
#define CW_DIAGNOSTIC_FILE "/var/log/cwatcher/diagnostic.txt"

//// log file setting
#define LOG_PATH "/var/log/cwatcher"
#define LOG_NAME "cw.log"

//// service/daemon setting
#ifdef WIN32
	#pragma warning(disable:4996)
	#define WINDOWS_SERVICE_NAME L"Contents Watcher 2 Service"
	#define WINDOWS_DISPLAY_NAME L"Contents Watcher 2 Service"
#else
	//#define DAEMON_USER_NAME "nobody"
	#define PID_FILE_PATH "/var/run/cw2_server.pid"
#endif

//// database setting
#define DB_SUPPORT_MYSQL
//#define DB_SUPPORT_ODBC
//#define DB_SUPPORT_SQLITE
