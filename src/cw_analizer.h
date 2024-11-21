
#ifndef __CW_ANALIZER_H
#define __CW_ANALIZER_H

#include "common/thread.h"
#include "common/logger.h"
#include "common/uchar.h"
#include "cw_server.h"
#include <map>
#include <set>
#include <list>

namespace ContentsWatcher {

	struct rpc_server_data_t {
		std::map<rpc_program_number_t, unsigned short> rpc_program_data;
		std::map<unsigned short, protocol_type_t> stream_type_map;
	};

	struct client_data_t {
		mutable time_t lastupdate;
		ustring domain;
		ustring username;
		ustring hostname;
		client_data_t()
		{
			lastupdate = 0;
		}
		void touch() const
		{
			lastupdate = time(0);
		}
	};

	class PacketAnalyzerThread : public Thread, public Logger {
		friend class MainServer;
		friend class CaptureThread;
	private:
		Event _event;
		Mutex _mutex;
		bool _cleanup;
	private:
		virtual void log_print(microsecond_t us, char const *str);
		virtual void log_vprintf(microsecond_t us, char const *str, va_list ap);
		virtual void log_printf(microsecond_t us, char const *str, ...);
	public:
	private:

		std::list<CapturedPacket> _queue;

		Mutex *get_mutex_for_settings();
		Option const *get_option() const;

		unsigned long long _packet_number;

		int target_protocol_flags; // target_protocol_flag_t

		EventProcessor *get_processor() const;

		std::map<client_server_t, packet_capture_data_t> _ip_session_map;

		// IPアドレスからユーザ名を検索するマップ
		std::map<ip_address_t, client_data_t> _client_map;

		std::map<ip_address_t, rpc_server_data_t> _rpc_server_map;

	private:
		static inline bool is_smb_packet(unsigned char const *const ptr, unsigned char const *const end)
		{
			return ptr + 8 <= end && memcmp(ptr + 4, "\xffSMB", 4) == 0;
		}

		static inline bool is_smb2_packet(unsigned char const *const ptr, unsigned char const *const end)
		{
			return ptr + 8 <= end && memcmp(ptr + 4, "\xfeSMB", 4) == 0;
		}

	public:

		PacketAnalyzerThread()
		{
			_cleanup = false;
		}

		~PacketAnalyzerThread()
		{
			the_server->close_pcap();
		}

	private:

		void dispose_forgotten_data();

		void on_http_get_request(std::string const &url)
		{
			puts(url.c_str());
		}

	
		std::string decode_netbios_name(unsigned char const *ptr, size_t len)
		{
			size_t i, n;
			n = len / 2;
			std::vector<char> tmp(n);
			for (i = 0; i < n; i++) {
				char c = (char)(((ptr[0] - 0x41) << 4) | (ptr[1] - 0x41));
				tmp[i] = c;
				ptr += 2;
			}
			while (i > 0 && tmp[i - 1] <= ' ') {
				i--;
			}
			return std::string(&tmp[0], i);
		}


		void statistic_network_traffic(unsigned long size);

		// smb

		ustring smb_get_user_name(session_data_t const *session, smb_uid_t uid) const;

		bool cmd_tree_connect_request(session_data_t *session, smb_request_id_t const &request_id, unsigned char const *smbdata, unsigned char const *end, unsigned short flags2);
		void cmd_tree_connect_response(session_data_t *session, smb_request_id_t const &request_id, smb_uid_t uid, smb_tid_t tree_id);

		void smb_create_action(int smbversion, client_server_t const *sid, packet_capture_data_t *data, ustring const &username, unsigned long action, bool isdirectory, ustring const &path);
		int on_udp138_packet(packet_capture_data_t *data, bool direction, blob_t *stream);
		int on_smb_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream);
		int on_smb2_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream);

		// smb2

		ustring smb2_get_user_name(session_data_t const *session, smb2_sid_t sid) const;

		// afp

		int on_a_dsi_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream);
		int on_dsi_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream);

		// nfs

		struct rpc_request_t {
			client_server_t const *sid;
			packet_capture_data_t *data;
			unsigned long header;
			unsigned long xid;
			unsigned long message_type;
			unsigned long version;
			unsigned long program_number;
			unsigned long program_version;
			unsigned long procedure;
		};

		struct rpc_response_t {
			client_server_t const *sid;
			packet_capture_data_t *data;
			unsigned long header;
			unsigned long xid;
		};

		int on_rpc_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream);

		// ip

		int dispatch_ip_data_packet(protocol_type_t stype, client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream);

		void dispatch_ip_packet(CapturedPacket const *packet, bool tcp, unsigned char const *header, size_t length);

		void dispatch_ip4_packet(CapturedPacket const *packet);
		void dispatch_ip6_packet(CapturedPacket const *packet);

		//

		std::list< std::vector<unsigned char> > _last_packet;

		struct last_packet_file_t {
			int fd;
			last_packet_file_t()
			{
				fd = -1;
			}
			~last_packet_file_t()
			{
				close();
			}
			bool open();
			void close();
		} _last_packet_file;

		void savepacket(CapturedPacket const *packet);

		void dispatch_raw_packet(CapturedPacket const *packet);

		//

		bool is_interrupted()
		{
			return false;
		}

		virtual void run();

	};


} // namespace ContentsWatcher

#endif

