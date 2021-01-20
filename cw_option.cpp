
#include "cw_option.h"


namespace ContentsWatcher {

	Option::Option()
	{
		mtime = 0;
		source_mode = PCAP_LIVE;
		snap_length = 1600;
		ip_checksum_validation = true;
		database.server = "localhost";
		database.uid = "root";
		database.pwd = "";
		database.name = "cwatcher";
		no_database = false;
		ignore_target_servers = false;
		ignore_defective_packet = false;
		no_scan_devices = false;
		no_log = false;
		no_log_console = false;
		save_packet = 0;
		dump_packet = 0;
		archive_csv = true;
		archive_csv_base_dir = "/usr/local/cwatcher/htdocs/viewer/export";
	}

} // namespace ContentsWatcher



