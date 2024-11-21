
#ifndef __CW_OPTION_H
#define __CW_OPTION_H

#include <string>
#include <set>
#include "common/netaddr.h"

namespace ContentsWatcher {

	struct Option {
		time_t mtime; // last update time
		enum {
			PCAP_LIVE,
			PCAP_FILE,
		} source_mode;
		bool ip_checksum_validation;
		bool no_database;
		int snap_length;
		bool ignore_target_servers;
		bool no_scan_devices;
		bool no_log;
		bool no_log_console;
		bool ignore_defective_packet;
		unsigned int save_packet;
		unsigned int dump_packet;
		bool archive_csv;
		std::string archive_csv_base_dir;
		struct {
			std::string server;
			std::string name;
			std::string uid;
			std::string pwd;
		} database;

		std::set<ip_address_t> exclude_clients;

		std::string interface_text;
		// -i オプションで指定した値

		std::string pcap_filter;

		std::string device_name;
		// デバイス名
		// ex.
		//   Linux: eth0
		//   Windows: \Device\NPF_{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}

		std::vector<std::string> file_names;

		Option();
	};

} // namespace ContentsWatcher

#endif


