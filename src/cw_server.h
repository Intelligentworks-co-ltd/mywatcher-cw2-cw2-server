
#ifndef __CW_SERVER_H
#define __CW_SERVER_H

#ifdef WIN32
	#pragma warning(disable:4996)
	struct pcap;
	struct pcap_if;
	typedef struct pcap pcap_t;
	typedef struct pcap_if pcap_if_t;
#else
	#include <pcap.h>
#endif

#include "TheServer.h"
#include "common/uchar.h"
#include "common/mutex.h"
#include "cw.h"
#include "cw_output.h"
#include "cw_lookup.h"
#include "common/cpustat.h"
#include "common/container.h"
#include "cw_option.h"

namespace ContentsWatcher {

	class PacketAnalyzerThread;

	struct CapturedPacket {

		// add analyze VLAN-tag (added by kitam)
		unsigned short frameType;
		int offset2IPHeader;
		
		unsigned short getFrameType() const {
			return frameType;
		}
		int getOffset2IPHeader() const {
			return offset2IPHeader;
		}

		struct Entity {
			unsigned long long packetnumber;
			long tv_sec;
			long tv_usec;
			blob_t data;
			Entity()
			{
				tv_sec = 0;
				tv_usec = 0;
			}
		};

		container<Entity> _container;

		CapturedPacket()
		{
		}
		CapturedPacket(CapturedPacket const &r)
		{
			_container = r._container;
		}
		~CapturedPacket()
		{
		}
		void operator = (CapturedPacket const &r)
		{
			_container = r._container;
		}
		void create_object(unsigned long long packetnumber, long sec, long usec, unsigned char const *dataptr, size_t datalen)
		{
			_container.create();
			_container->packetnumber = packetnumber;
			_container->tv_sec = sec;
			_container->tv_usec = usec;
			_container->data.assign(dataptr, dataptr + datalen);


			// only adjust VLAN Single Tagging (added by kitam)
			frameType = BE2(dataptr + 12);
			if (frameType == 0x8100) {
				// VLAN-tag
				frameType = BE2(dataptr + 16);
				offset2IPHeader = 18;
			}
			else {
				// NO VLAN
				offset2IPHeader = 14;
			}

		}
		unsigned char const *dataptr() const
		{
			return &_container->data[0];
		}
		size_t datalen() const
		{
			return _container->data.size();
		}
		unsigned long long microsecond() const
		{
			assert(_container.has());
			return _container->tv_sec * 1000000ULL + _container->tv_usec;
		}
	};

	class CaptureThread : public Thread {
		friend class MainServer;
	private:
		void addpacket(unsigned long long packetnumber, long sec, long usec, unsigned char const *dataptr, size_t datalen);
		void analize(int source_mode);
		virtual void run();
	public:
		CaptureThread()
		{
		}
	};

	class MainServer : public TheServer {
		friend class PacketAnalyzerThread;
		friend class CaptureThread;
	private:
		Mutex _mutex_for_settings;
		Option _option;

		pcap_t *_pcap;
		unsigned long long _packet_number;

		TargetServersInfo _targetservers_info;
		ExclusionInfo _exclusion_info;
		time_t _last_update_exclusion_info;

		CaptureThread _capture_thread;
		PacketAnalyzerThread *_analyzer_thread;
		EventProcessor _processor;
		NameLookupThread _lookuper;
	public:
		MainServer();
		~MainServer();

		bool open_live(char const *name, int snaplen);
		bool open_file(char const *name);
		void close_pcap();

		void statistic_network_traffic(unsigned long size);

		virtual void set_option(void *ptr)
		{
			_option = *(Option const *)ptr;
		}
		Mutex *get_mutex_for_settings()
		{
			return &_mutex_for_settings;
		}
		Option const *get_option() const
		{
			return &_option;
		}
		Option *get_option()
		{
			return &_option;
		}
		ExclusionInfo const *get_exclusion_command_info() const
		{
			return &_exclusion_info;
		}
		std::string get_log_directory() const
		{
			return LOG_PATH;
		}
		EventProcessor *get_processor()
		{
			return &_processor;
		}

		virtual bool is_interrupted() const;

	private:
		time_t last_time;
		ProcessorLoad_ cpuload;
		unsigned long networkbytes;

		bool select_device(char const *target_name, std::string *device_name);

		void print_title_banner();

	public:

		void update_exclusion_info(DB_Connection *db);
		bool is_excluded(ip_address_t const &addr, uchar_t const *path);

		void print_diagnostic();

		virtual void everyminute(time_t now);
		virtual void everysecond();
		void run();

	};

} // namespace ContentsWatcher

extern ContentsWatcher::MainServer *the_server;

bool parse_option_ini_file(ContentsWatcher::Option *data);
bool parse_targetservers_file(ContentsWatcher::TargetServersInfo *data);
bool parse_exclusion_command_info_file(ContentsWatcher::ExclusionInfo *data);

#endif
