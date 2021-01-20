
#include <pcap.h>

#ifdef WIN32
#pragma comment(lib, "wpcap.lib")
#else
#include <arpa/inet.h>
#include <syslog.h>
#endif

#include "common/mynew.h"

#include "cw.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <locale.h>

#include "common/netif.h"

#include "common/combinepath.h"
#include "common/mkdirs.h"
#include "common/wildcard.h"

#include "common/cpustat.h"
#include "common/memstat.h"
#include "common/fsstat.h"

#include "common/text.h"

#include "cw_analizer.h"

ContentsWatcher::MainServer *the_server = 0;


extern unsigned long long diagnostic_allocated_memory;
extern size_t diagnostic_log_queue_size;
extern size_t diagnostic_packet_capture_queue_size;
extern size_t diagnostic_event_output_queue_size;
extern size_t diagnostic_ip_session_map_size;

void initialize_the_server()
{
	the_server = new ContentsWatcher::MainServer();
}

void terminate_the_server()
{
	delete the_server;
}

TheServer *get_the_server()
{
	return the_server;
}

namespace ContentsWatcher {
	//

	int client_server_t::compare(client_server_t const &r) const
	{
		if (client.addr < r.client.addr) {
			return -1;
		}
		if (client.addr > r.client.addr) {
			return 1;
		}
		if (client.port < r.client.port) {
			return -1;
		}
		if (client.port > r.client.port) {
			return 1;
		}
		if (server.addr < r.server.addr) {
			return -1;
		}
		if (server.addr > r.server.addr) {
			return 1;
		}
		if (server.port < r.server.port) {
			return -1;
		}
		if (server.port > r.server.port) {
			return 1;
		}
		return 0;
	}

	template <typename T> static inline T max_(T const &left, T const &right)
	{
		return left > right ? left : right;
	}

	bool binary_handle_t::operator < (binary_handle_t const &right) const
	{
		size_t i = 0;
		size_t n = max_(data.size(), right.data.size());
		while (i < n) {
			unsigned char l = i < data.size() ? data[i] : 0;
			unsigned char r = i < right.data.size() ? right.data[i] : 0;
			if (l < r) {
				return true;
			}
			if (l > r) {
				return false;
			}
			i++;
		}
		return false;
	}

	std::string binary_handle_t::tostring() const
	{
		size_t i, n;
		n = data.size();
		if (n == 0) {
			return std::string();
		}
		std::vector<char> tmp(n * 3 + 1);
		for (i = 0; i < n; i++) {
			sprintf(&tmp[i * 3], " %02X", data[i]);
		}
		tmp[i * 3] = 0;
		return &tmp[1];
	}

	packet_capture_data_t::packet_capture_data_t()
	{
		time_us = 0;
		lastupdate = 0;
		FIN_received = false;
		//stype = PROTOCOL_UNKNOWN;
	}

} // namespace ContentsWatcher


namespace ContentsWatcher {

	void CaptureThread::addpacket(unsigned long long packetnumber, long sec, long usec, unsigned char const *dataptr, size_t datalen)
	{
		std::list<CapturedPacket> &q = the_server->_analyzer_thread->_queue;
		std::list<CapturedPacket>::iterator it = q.insert(q.end(), CapturedPacket());
		it->create_object(the_server->_packet_number, sec, usec, dataptr, datalen);
		the_server->_analyzer_thread->_event.signal();
	}

	void CaptureThread::analize(int source_mode)
	{
		unsigned long long pcap_start_time = 0;
		unsigned long long pcap_end_time = 0;
		time_t start_time = time(0);

		{
			std::string filter = the_server->get_option()->pcap_filter;
			if (!filter.empty()) {
				int rc;
				bpf_u_int32 maskp = 0;
				struct bpf_program fcode;
				rc = pcap_compile(the_server->_pcap, &fcode, (char *)filter.c_str(), 0, maskp);
				if (rc < 0) {
					the_server->log_printf(0, 0, "pcap_compile: %s", pcap_geterr(the_server->_pcap));
					return;
				} else {
					rc = pcap_setfilter(the_server->_pcap, &fcode);
					if (rc < 0) {
						the_server->log_printf(0, 0, "pcap_setfilter: %s", pcap_geterr(the_server->_pcap));
						return;
					}
				}

				the_server->log_printf(0, 0, "pcap filter: %s", filter.c_str());
			}
		}

		while (!interrupted()) {
			try {
				struct pcap_pkthdr header;
				unsigned char const *data = pcap_next(the_server->_pcap, &header);

				if (!data) {
					if (source_mode == Option::PCAP_FILE) {
						break;
					}
					continue; // Timeout elapsed
				}

				the_server->statistic_network_traffic(header.caplen);

				the_server->_packet_number++;

				//if (the_server->_packet_number == 2191) {
				//	_NOP
				//}

				pcap_end_time = header.ts.tv_sec * 1000ULL + header.ts.tv_usec / 1000ULL;
				if (pcap_start_time == 0) {
					pcap_start_time = pcap_end_time;
				}

				switch (source_mode) {
				case Option::PCAP_LIVE:
					// キューに保存してイベントを発行
					{
						AutoLock lock(&the_server->_analyzer_thread->_mutex);
						if (the_server->_analyzer_thread->_queue.size() < CW_PACKET_CAPTURE_QUEUE_LIMIT) {
							addpacket(the_server->_packet_number, header.ts.tv_sec, header.ts.tv_usec, data, header.caplen);
						}
					}
					break;
				case Option::PCAP_FILE:
					// そのまま解析
					//addpacket(the_server->_packet_number, (long)time(0), 0, data, header.caplen);
					{
						CapturedPacket cp;
						cp.create_object(the_server->_packet_number, header.ts.tv_sec, header.ts.tv_usec, data, header.caplen);
						the_server->_analyzer_thread->dispatch_raw_packet(&cp);
					}
					break;
				}
			} catch (std::bad_alloc const &e) {
				on_bad_alloc(e, __FILE__, __LINE__);
			}
		}

		time_t end_time = time(0);

		if (source_mode == Option::PCAP_FILE) {
			double pcap_elapsed = (pcap_end_time - pcap_start_time) / 1000.0; // sec
			the_server->log_printf(0, 0, "statistics:");
			the_server->log_printf(0, 0, "number of packets: %u", the_server->_analyzer_thread->_packet_number);
			the_server->log_printf(0, 0, "capture duration: %.3fsec", pcap_elapsed);
			the_server->log_printf(0, 0, "analysis time: %usec", end_time - start_time);
		}
	}

	void CaptureThread::run()
	{
		int source_mode;
		int snap_length;
		std::string device_name;
		std::vector<std::string> file_names;

		{
			AutoLock lock(the_server->get_mutex_for_settings());
			Option const *option = the_server->get_option();
			source_mode = option->source_mode;
			snap_length = option->snap_length;
			device_name = option->device_name;
			file_names = option->file_names;
		}

		if (source_mode == Option::PCAP_LIVE) {

			/* Open the adapter */
			if (!the_server->open_live(device_name.c_str(), snap_length)) {
				the_server->log_printf(0, 0, "Error opening adapter \"%s\"", device_name.c_str());
				return;
			}

			the_server->log_printf(0, 0, "monitoring interface: \"%s\"", device_name.c_str());

			analize(source_mode);

			the_server->close_pcap();

		} else if (source_mode == Option::PCAP_FILE) {

			for (std::vector<std::string>::const_iterator it = file_names.begin(); it != file_names.end(); it++) {

				char const *path = it->c_str();

				/* Open the file */
				if (!the_server->open_file(path)) {
					the_server->log_printf(0, 0, "Error opening file \"%s\"", path);
					return;
				}

				the_server->log_printf(0, 0, "analyzing file: \"%s\"", path);
				analize(source_mode);

				the_server->close_pcap();

			}

		}

	}


} // namespace ContentsWatcher;

namespace ContentsWatcher {

	MainServer::MainServer()
	{
		_pcap = 0;
		_packet_number = 0;

		_last_update_exclusion_info = 0;

		_analyzer_thread = new PacketAnalyzerThread();

		_processor.setup(_analyzer_thread);
	}

	MainServer::~MainServer()
	{
		delete _analyzer_thread;
	}

	// ネットワークデバイスをオープン
	bool MainServer::open_live(char const *name, int snaplen)
	{
		char errbuf[PCAP_ERRBUF_SIZE];
	
		close_pcap();

		the_server->_pcap = pcap_open_live((char *)name,       // name of the device
			snaplen,                      // portion of the packet to capture. 
			1,                          // promiscuous mode (nonzero means promiscuous)
			1000,                       // read timeout
			errbuf                      // error buffer
			);

                if (errbuf[0] != '\0') {
			the_server->log_printf(0, 0, "Warning in pcap_open_live(%s): \"%s\"", name, errbuf);
			the_server->log_printf(0, 0, "server shutdown");
			exit(EXIT_FAILURE);
		}

		if (!the_server->_pcap) {
			return false;
		}

		return true;
	}

	// キャプチャファイルをオープン
	bool MainServer::open_file(char const *name)
	{
		char errbuf[PCAP_ERRBUF_SIZE];
	
		close_pcap();

		the_server->_pcap = pcap_open_offline(name, errbuf);

		if (!the_server->_pcap) {
			return false;
		}

		return true;
	}

	// ネットワークデバイスをクローズ
	void MainServer::close_pcap()
	{
		if (the_server->_pcap) {
			pcap_close(the_server->_pcap);
			the_server->_pcap = 0;
		}
	}

	void MainServer::statistic_network_traffic(unsigned long size)
	{
		the_server->networkbytes += size;
	}

	void MainServer::update_exclusion_info(DB_Connection *db)
	{
		std::vector<exclusion_path_entry_t> newvec;
		//bool changed = false;

		time_t now = time(0);
		if (_last_update_exclusion_info + 60 < now) {
			AutoLock lock(get_mutex_for_settings());

			_exclusion_info.file_and_dir_map.clear();
			{
				DB_Statement st;
				st.begin_transaction(db, 0);

				char const *sql = "select server,path,type from alert_esc";
				st.prepare(sql);
				st.execute();
				while (st.fetch()) {
					if (st.column_count() == 3) {
						std::string server = st.column_value(0).get_string();
						std::string path = st.column_value(1).get_string();
						int type = st.column_value(2).get_integer();
						exclusion_path_entry_t t;
						if (t.server_address.parse(server.c_str())) {
							t.pattern = soramimi::jcode::convert(soramimi::jcode::EUCJP, path);
							t.type = type;
							_exclusion_info.file_and_dir_map.push_back(t);
						}
					}
				}
			}

			_exclusion_info.pattern_map.clear();
			{
				DB_Statement st;
				st.begin_transaction(db, 0);

				char const *sql = "select esc_name,esc_num from pattern";
				st.prepare(sql);
				st.execute();
				while (st.fetch()) {
					if (st.column_count() == 2) {
						std::string path = st.column_value(0).get_string();
						int type = st.column_value(1).get_integer();
						exclusion_pattern_entry_t t;
						t.pattern = soramimi::jcode::convert(soramimi::jcode::EUCJP, path);
						t.type = (exclusion_pattern_entry_t::Type)type;
						_exclusion_info.pattern_map.push_back(t);
					}
				}
			}

			_last_update_exclusion_info = now;
			//changed = true;
		}

		//return true;
	}

	// 除外チェック
	bool MainServer::is_excluded(ip_address_t const &addr, uchar_t const *path)
	{
		if (path[0] == '/') {
			if (path[1] == '/') {
				uchar_t const *p = ucs::wcschr(path + 2, '/');
				if (p) {
					path = p;
				}
			}
		} else {
			uchar_t const *p = ucs::wcschr(path, '/');
			if (p) {
				path = p;
			}
		}
		{
			std::vector<exclusion_path_entry_t>::const_iterator it;
			for (it = _exclusion_info.file_and_dir_map.begin(); it != _exclusion_info.file_and_dir_map.end(); it++) {
				if (it->server_address == addr) {
					if (it->type == 0) { // ディレクトリ：前方一致
						if (ucs::wcsncmp(path, it->pattern.c_str(), it->pattern.size()) == 0) { // 一致したら除外
							return true;
						}
					} else if (it->type == 1) { // ファイル：完全一致
						if (ucs::wcscmp(path, it->pattern.c_str()) == 0) { // 一致したら除外
							return true;
						}
					}
				}
			}
		}
		{
			size_t pathlen = ucs::wcslen(path);
			std::vector<exclusion_pattern_entry_t>::const_iterator it;
			for (it = _exclusion_info.pattern_map.begin(); it != _exclusion_info.pattern_map.end(); it++) {
				switch (it->type) {
				case exclusion_pattern_entry_t::CONTAIN:
					if (ucs::wcsstr(path, it->pattern.c_str())) {
						return true;
					}
					break;
				case exclusion_pattern_entry_t::STARTS:
					if (ucs::wcsncmp(path, it->pattern.c_str(), it->pattern.size()) == 0) {
						return true;
					}
					break;
				case exclusion_pattern_entry_t::ENDS:
					if (pathlen >= it->pattern.size()) {
						if (ucs::wcscmp(path + pathlen - it->pattern.size(), it->pattern.c_str()) == 0) {
							return true;
						}
					}
					break;
				case exclusion_pattern_entry_t::COMPLETE:
					if (ucs::wcscmp(path, it->pattern.c_str()) == 0) {
						return true;
					}
					break;
				}			
			}
		}
		return false;
	}

	void MainServer::print_diagnostic()
	{
		FILE *fp = fopen(CW_DIAGNOSTIC_FILE, "w");
		if (!fp) {
			fprintf(stderr, "could not open file '%s'\n", CW_DIAGNOSTIC_FILE);
			return;
		}

		fprintf(fp, "allocated memory: %llu\n", diagnostic_allocated_memory);
		fprintf(fp, "log queue size: %u\n", diagnostic_log_queue_size);
		fprintf(fp, "packet capture queue size: %u\n", diagnostic_packet_capture_queue_size);
		fprintf(fp, "event output queue size: %u\n", diagnostic_event_output_queue_size);
		fprintf(fp, "ip session map size: %u\n", diagnostic_ip_session_map_size);


		fclose(fp);
	}

	void MainServer::everyminute(time_t now)
	{
		unsigned long bytes = networkbytes;
		networkbytes = 0;
		double cpu;
		double mem;
		if (!cpuload.get(&cpu)) {
			cpu = -1;
		}
		mem = memory_used_ratio();

#if 1
		log_printf(0, 0, "[HEALTH1] CPU=%.2f%%, Memory=%.2f%%, Traffic=%.0fbps"
			, cpu
			, mem
			, bytes * 8.0 / (now - last_time)
			);
#else
		log_printf(0, 0, "[HEALTH] CPU=%.2f%%, Memory=%.2f%%, Allocated=%ubytes, Traffic=%.0fbps"
			, cpu
			, mem
			, mynew_allocated
			, bytes * 8.0 / (now - last_time)
			);
#endif

		log_printf(0, 0, "[HEALTH2] AllocMem=%llu, LogQ=%u, PackCapQ=%u, EveOutQ=%u, SessionMapSize=%u\n", 
			diagnostic_allocated_memory,
			diagnostic_log_queue_size,
			diagnostic_packet_capture_queue_size,
			diagnostic_event_output_queue_size,
			diagnostic_ip_session_map_size);

		_analyzer_thread->_cleanup = true;
		_analyzer_thread->_event.signal();
	}

	void MainServer::everysecond()
	{
		struct stat st;

		{
			AutoLock lock(&_mutex_for_settings);
			Option const *option = get_option();

			if (stat(CW_INI_FILE, &st) == 0) {
				if (_option.mtime < st.st_mtime) {
					Option option;
					if (parse_option_ini_file(&option)) {
						std::swap(option, _option);
						log_printf(0, 0, "option ini file \"%s\" changed"
							, CW_INI_FILE
							);
					}
				}
			}

			if (!option->ignore_target_servers) {
				if (stat(CW_TARGETSERVERS_FILE, &st) == 0) {
					if (_targetservers_info.mtime < st.st_mtime) {
						TargetServersInfo info;
						if (parse_targetservers_file(&info)) {
							std::swap(info, _targetservers_info);
							log_printf(0, 0, "targetservers file \"%s\" changed"
								, CW_TARGETSERVERS_FILE
								);
						}
					}
				}
			}

			if (stat(CW_EXCLUSION_COMMAND_INFO_FILE, &st) == 0) {
				if (_exclusion_info.mtime < st.st_mtime) {
					ExclusionInfo info;
					if (parse_exclusion_command_info_file(&info)) {
						std::swap(info.cmd_read, _exclusion_info.cmd_read);
						_exclusion_info.mtime = st.st_mtime;
						log_printf(0, 0, "exclusion info file \"%s\" changed"
							, CW_EXCLUSION_COMMAND_INFO_FILE
							);
					}
				}
			}
		}

		//

		// print_diagnostic();

		//

		time_t now = time(0);
		if (last_time + 60 <= now) {
			everyminute(now);
			last_time = now;
		}
	}

	bool MainServer::is_interrupted() const
	{
		if (!_analyzer_thread->running()) {
			return true;
		}
		return TheServer::is_interrupted();
	}

	static std::string get_device_text(int index, mac_address_t const &mac, pcap_if_t *device)
	{
		stringbuffer sb;
		char tmp[100];

		sprintf(tmp, "(%d) ", index);
		sb.write(tmp);

		sb.write(mac.tostring());

		if (device->name) {
			sb.put(' ');
			sb.write(device->name);
		}
		if (device->description) {
			if (device->description[0]) {
				sb.put(' ');
				sb.put('\"');
				sb.write(device->description);
				sb.put('\"');
			}
		}
		return (char const *)sb;
	}

	bool MainServer::select_device(char const *target_name, std::string *device_name)
	{
		Option const *option = get_option();
		if (option->no_scan_devices) {
			*device_name = target_name;
			return true;
		}

		pcap_if_t *alldevs, *device;
		char errbuf[PCAP_ERRBUF_SIZE];

		log_printf(0, 0, "interface detecting:");

		enum {
			NWIF_TEXT,
			NWIF_MAC,
			NWIF_IPV4,
		} nwiftype = NWIF_TEXT;

		ipv4addr_t target_ipv4addr;
		mac_address_t target_macaddr;
		{
			if (target_ipv4addr.parse(target_name)) {
				nwiftype = NWIF_IPV4;
			} else if (target_macaddr.parse(target_name)) {
				nwiftype = NWIF_MAC;
			}
		}

		std::string selected_device_text;

		{
			NetworkInterfaces nwif;
			nwif.open();

			/* The user didn't provide a packet source: Retrieve the local device list */
			if (pcap_findalldevs(&alldevs, errbuf) == -1) {
				log_printf(0, 0, "Error in pcap_findalldevs: %s\n", errbuf);
				return false;
			}

			int index = 0;

			for (device = alldevs; device; device = device->next) {
				mac_address_t macaddr;
				{
					unsigned long long a = 0;
					nwif.query(device->name, &a);
					macaddr = a;
				}
				bool accept = false;
				pcap_addr const *devaddr = device->addresses;
				while (devaddr) {
					if (devaddr->addr->sa_family == AF_INET) {
						index++;

						ipv4addr_t ipv4addr;
						ipv4addr_t ipv4mask;
						{
							sockaddr_in const *a = (sockaddr_in const *)devaddr->addr;
							sockaddr_in const *m = (sockaddr_in const *)devaddr->netmask;
							ipv4addr = ntohl(*(unsigned long *)&a->sin_addr);
							ipv4mask = ntohl(*(unsigned long *)&m->sin_addr);
						}
						std::string text = get_device_text(index, macaddr, device);
						log_printf(0, 0, "%s, network=%s netmask=%s"
							, text.c_str()
							, ipv4addr.tostring().c_str()
							, ipv4mask.tostring().c_str()
							);

						switch (nwiftype) {
						case NWIF_IPV4:
							if ((ipv4addr & ipv4mask) == target_ipv4addr) {
								accept = true;
							}
							break;
						case NWIF_MAC:
							if (macaddr == target_macaddr) {
								accept = true;
							}
							break;
						default:
							if (device->name && wildcard(target_name, device->name)) {
								accept = true;
								break;
							}
							if (device->description && wildcard(target_name, device->description)) {
								accept = true;
								break;
							}
							break;
						}

						if (accept) {
							if (device_name->empty()) {
								*device_name = device->name;
								selected_device_text = text;
							}
						}
					}
					devaddr = devaddr->next;
				}
			}

			pcap_freealldevs(alldevs);
		}

		if (device_name->empty()) {
			log_printf(0, 0, "no interfaces found");
			return false;
		}

		return true;
	}

	void MainServer::print_title_banner()
	{
		std::vector<std::string> lines;

		get_title_text(&lines);

		for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++) {
			log_printf(0, 0, it->c_str());
		}
	}

	void MainServer::run()
	{
		if (!mkdirs(get_log_directory().c_str())) {
			fprintf(stderr, "cw2_server: could not access log directory.\n");
			return;
		}

		Option *option = get_option();

		get_log_option()->output = !option->no_log;
		get_log_option()->output_console = !option->no_log_console;

		_log_thread.setup(this, CombinePath(LOG_PATH, LOG_NAME), 100);
		_log_thread.start();

		_processor.enable_archive_csv(option->archive_csv, option->archive_csv_base_dir);

		_dump.setup(this, CW_PACKET_DUMP_FILE, option->dump_packet);

		print_title_banner();

		parse_exclusion_command_info_file(&_exclusion_info);

		_packet_number = 0;

		if (option->source_mode == Option::PCAP_LIVE) { // リアルタイムキャプチャモード

			pcap_if_t *device = 0;

			std::string devname;
			if (select_device(option->interface_text.c_str(), &devname)) {

				option->device_name = devname;

				last_time = time(0);
				networkbytes = 0;

				_capture_thread.start();
				_analyzer_thread->start();

				if (option->save_packet > 0) {
					_analyzer_thread->_last_packet_file.open();
				}

				if (!option->no_database) {
					try {
						_processor.start();
					} catch (DBException const &e) {
						log_print(0, 0, e.get_message());
					}
					_lookuper.start();
				}

				cpuload.open();

				loop();

				cpuload.close();

				if (!option->no_database) {
					_processor.stop();
					_lookuper.stop();
					_processor.join();
					_lookuper.join();
				} else {
					_NOP
				}

				_analyzer_thread->_last_packet_file.close();
				_analyzer_thread->stop();
				_capture_thread.stop();

			} else {
				log_printf(0, 0, "no device selected");
			}

		} else if (option->source_mode == Option::PCAP_FILE) { // ファイル解析モード

			if (!option->no_database) {
				_processor.start();
			}

			_capture_thread.run();

			if (!option->no_database) {
				_processor.stop();
				_processor.join();
			}

		}

		log_printf(0, 0, "server shutdown");

		_log_thread.stop();
		_log_thread.join();
	}

} // namespace ContentsWatcher

void on_bad_alloc(std::bad_alloc const &e, char const *file, int line)
{
	char tmp[1000];
	sprintf(tmp, "EXCEPTION (%s) file=\"%s\" line=%u", e.what(), file, line);
#ifdef WIN32
#else
	syslog(LOG_ERR, tmp);
#endif
	//the_server->log_print(0, 0, tmp);
	abort();
}



void debug()
{
}
