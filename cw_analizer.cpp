#include "common/sockinet.h"
#include <pcap.h>
#include "cw_analizer.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <unistd.h>

size_t diagnostic_packet_capture_queue_size = 0;
size_t diagnostic_ip_session_map_size = 0;

namespace ContentsWatcher {

	// ログ出力

	void PacketAnalyzerThread::log_print(microsecond_t us, char const *str)
	{
		the_server->log_print(0, us, str);
	}

	void PacketAnalyzerThread::log_vprintf(microsecond_t us, char const *str, va_list ap)
	{
		the_server->log_vprintf(0, us, str, ap);
	}

	void PacketAnalyzerThread::log_printf(microsecond_t us, char const *str, ...)
	{
		va_list ap;
		va_start(ap, str);
		the_server->log_vprintf(0, us, str, ap);
		va_end(ap);
	}

	// 不要なデータを破棄する

	void PacketAnalyzerThread::dispose_forgotten_data()
	{
		// changed to 1-day
		// unsigned long timeout = 2 * 24 * 60 * 60;
		unsigned long timeout = 24 * 60 * 60;

		time_t now = time(0);

		// 最終更新から48時間を超えたTCPセッションは監視対象から外す

		{
			std::map<client_server_t, packet_capture_data_t> newmap;
			std::map<client_server_t, packet_capture_data_t>::iterator it;
			for (it = _ip_session_map.begin(); it != _ip_session_map.end(); it++) {
				if (it->second.FIN_received) {
					// (nop) <- erase this member
					_NOP
				}
				else {
					if (now - it->second.lastupdate < timeout) {
						newmap.insert(newmap.end(), *it);
					}
				}
			}
			std::swap(_ip_session_map, newmap);
		}

		//
		{
			std::map<ip_address_t, client_data_t> newmap;
			std::map<ip_address_t, client_data_t>::iterator it;
			for (it = _client_map.begin(); it != _client_map.end(); it++) {
				if (now - it->second.lastupdate < timeout) {
					newmap.insert(newmap.end(), *it);
				}					
			}
			std::swap(_client_map, newmap);
		}

	}

	void PacketAnalyzerThread::statistic_network_traffic(unsigned long size)
	{
		the_server->networkbytes += size;
	}

	Mutex *get_mutex_for_settings()
	{
		return the_server->get_mutex_for_settings();
	}

	Mutex *PacketAnalyzerThread::get_mutex_for_settings()
	{
		return the_server->get_mutex_for_settings();
	}

	Option const *PacketAnalyzerThread::get_option() const
	{
		return the_server->get_option();
	}

	EventProcessor *PacketAnalyzerThread::get_processor() const
	{
		return the_server->get_processor();
	}

	// IPパケットの種類に応じて分岐

	int PacketAnalyzerThread::dispatch_ip_data_packet(protocol_type_t stype, client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream)
	{
		if (stype == PROTOCOL_SMB) {
			unsigned char const *ptr = stream->peek(0x10);
			if (!ptr) return 0;
			unsigned char const *end = ptr + 0x10;

			if (ptr + 8 <= end) {
				if (is_smb_packet(ptr, end)) {
					stype = PROTOCOL_SMB1;
				} else if (is_smb2_packet(ptr, end)) {
					stype = PROTOCOL_SMB2;
				}
			}
		}

		switch (stype) {
		case PROTOCOL_RPC_PORTMAP:
			return on_rpc_packet(sid, data, direction, stream);
		case PROTOCOL_SMB:
			return -1; // 不明なSMB/NetBIOSパケット
		case PROTOCOL_SMB1:
			return on_smb_packet(sid, data, direction, stream);
		case PROTOCOL_SMB2:
			return on_smb2_packet(sid, data, direction, stream);
		case PROTOCOL_DSI:
			return on_dsi_packet(sid, data, direction, stream);
		case PROTOCOL_RPC_NFS:
		case PROTOCOL_RPC_MOUNT:
			return on_rpc_packet(sid, data, direction, stream);
		case PROTOCOL_NETBIOS_DG:
			return on_udp138_packet(data, direction, stream);
		}

		if (false) { // http test
			unsigned char const *ptr = stream->c_str();
			unsigned char const *end = ptr + stream->size();
			if (ptr + 10 < end) {

				// http
				unsigned char const *nl = (unsigned char const *)memchr(ptr, '\n', end - ptr);
				if (nl && memcmp(ptr, "GET ", 4) == 0) {
					unsigned char const *h = ptr + 4;
					while (h) {
						h = (unsigned char const *)memchr(h, 'H', nl - h);
						if (!h) {
							break;
						}
						if (h + 5 < nl && memcmp(h, "HTTP/", 5) == 0) {
							bool accept = false;
							unsigned char const *p = h;
							while (p < end) {
								if (p[0] == '\n' && p + 1 < end && p[1] == '\n') {
									p += 2;
									accept = true;
									break;
								}
								if (p[0] == '\r' && p + 3 < end && memcmp(p, "\r\n\r\n", 4) == 0) {
									p += 4;
									accept = true;
									break;
								}
								p++;
							}
							if (accept) {
								char const *left = (char const *)ptr + 4;
								char const *right = (char const *)h;
								while (left < right && isspace(left[0])) left++;
								while (left < right && isspace(right[-1])) right--;
								std::string url(left, right);
								on_http_get_request(url);
								return p - ptr;
							}
							return -1;
						} else {
							h++;
						}
					}

				}
			}
		}

		return -1;
	}

	// TCP,UDPヘッダのチェックサムを検証
	static bool validate_tcp_udp_checksum(unsigned char const *ipheader, unsigned char const *tcpheader, size_t tcplength, bool tcp)
	{
		unsigned short tmp[20];
		size_t i, n;

		int ipversion = ipheader[0] >> 4;
		if (ipversion == 4) {
			memcpy(tmp, ipheader + 12, 8); // ip address
			if (tcp) {
				tmp[4] = htons(6); // protocol = tcp
			} else {
				tmp[4] = htons(17); // protocol = udp
			}
			tmp[5] = htons(tcplength); // tcp packet length
			n = 6;
		} else if (ipversion == 6) {
			memcpy(tmp, ipheader + 8, 32); // ip address
			*(unsigned long *)&tmp[16] = htonl(tcplength);
			tmp[18] = 0;
			if (tcp) {
				tmp[19] = htons(6); // protocol = tcp
			} else {
				tmp[19] = htons(17); // protocol = udp
			}
			n = 20;
		} else {
			return false;
		}

		long sum = 0;

		for (i = 0; i < n; i++) {
			unsigned short w = ntohs(tmp[i]);
			sum += w;
		}

		unsigned short const *p = (unsigned short const *)tcpheader;
		n = tcplength / 2;

		for (i = 0; i < n; i++) {
			unsigned short w = ntohs(p[i]);
			sum += w;
		}
		if (tcplength & 1) {
			sum += tcpheader[i * 2] << 8;
		}
		sum = (sum & 0xffff) + (sum >> 16);
		sum = (sum & 0xffff) + (sum >> 16);

		if (sum != 0xffff) {
			// checksum error
			return false;
		}
		return true;
	}

	// TCP/UDPを解析
	void PacketAnalyzerThread::dispatch_ip_packet(CapturedPacket const *packet, bool tcp, unsigned char const *header, size_t length)
	{
		int offset2IPHeader = packet->getOffset2IPHeader();		// adjusted VLAN-Tag size
		
		mac_address_t srcmac(packet->dataptr() + 6);
		mac_address_t dstmac(packet->dataptr() + 0);

		unsigned char const *ipheader = packet->dataptr() + offset2IPHeader;

		ip_address_t srcaddr;
		ip_address_t dstaddr;

		unsigned short ipv = packet->getFrameType();
		if (ipv == 0x0800) {		// ipv4
			srcaddr = BE4(ipheader + 12);
			dstaddr = BE4(ipheader + 16);
		} else if (ipv == 0x86dd) { // ipv6
			srcaddr.assign((unsigned short const *)(packet->dataptr() + (offset2IPHeader + 8)));
			dstaddr.assign((unsigned short const *)(packet->dataptr() + (offset2IPHeader + 24)));
		} else {
			return;
		}

		// ポート

		unsigned short srcport = BE2(header + 0);
		unsigned short dstport = BE2(header + 2);

		unsigned long tcp_seqnumber = 0;
		unsigned long tcp_acknumber = 0;
		unsigned char tcp_ctrlflag = 0;
		unsigned char dataoffset = 0;
		if (tcp) { // TCP
			tcp_seqnumber = BE4(header + 4);
			tcp_acknumber = BE4(header + 8);
			dataoffset = (header[12] >> 4) * 4;
			tcp_ctrlflag = header[13];
			if (length < dataoffset) {
				return;
			}
		} else { // UDP
			dataoffset = 8;
		}

		if (get_option()->ip_checksum_validation) {
			if (!validate_tcp_udp_checksum(ipheader, header, length, tcp)) {
				log_printf(packet->microsecond(), "[IP] checksum incorrect (#%llu)", _packet_number);
				return;
			}
		}

		unsigned char const *dataptr = header + dataoffset;
		size_t datalen = 0;
		if (tcp) {
			datalen = length - dataoffset;
		} else {
			datalen = ntohs(*(unsigned short const *)&header[4]);
		}

		bool direction = true;
		// 通信の方向
		// client => server: true
		// client <= server: false

		protocol_type_t stype = PROTOCOL_UNKNOWN;

		unsigned char tcpflags = 0;

		if (tcp) {

			if (length >= 0x0d) {
				tcpflags = header[0x0d];
			}

			// http
			if (dstport == 80 || srcport == 80) {
				direction = (dstport == 80);
				stype = PROTOCOL_HTTP;
				goto dispatch;
			}

			// check portmap packet
			if (dstport == 111 || srcport == 111) {
				direction = (dstport == 111);
				stype = PROTOCOL_RPC_PORTMAP;
				goto dispatch;
			}

			// check smb packet
			if (dstport == 445 || srcport == 445 || dstport == 139 || srcport == 139) {
				direction = (dstport == 445 || dstport == 139);
				stype = PROTOCOL_SMB;
				// SMB1かSMB2かは、あとで決定する。
				goto dispatch;
			}

			// check afp packet
			if (dstport == 548 || srcport == 548) {
				direction = (dstport == 548);
				stype = PROTOCOL_DSI;
				goto dispatch;
			}

		} else { // udp
			// netbios datagram
			if (dstport == 138 || srcport == 138) {
				direction = (dstport == 138);
				stype = PROTOCOL_NETBIOS_DG;
				goto dispatch;
			}
		}

		// check rpc packet

		{
			std::map<ip_address_t, rpc_server_data_t>::const_iterator it1;
			it1 = _rpc_server_map.find(dstaddr);
			if (it1 != _rpc_server_map.end()) {
				std::map<unsigned short, protocol_type_t>::const_iterator it2 = it1->second.stream_type_map.find(dstport);
				if (it2 != it1->second.stream_type_map.end()) {
					stype = it2->second;
					direction = true;
					goto dispatch;
				}
			}
			it1 = _rpc_server_map.find(srcaddr);
			if (it1 != _rpc_server_map.end()) {
				std::map<unsigned short, protocol_type_t>::const_iterator it2 = it1->second.stream_type_map.find(srcport);
				if (it2 != it1->second.stream_type_map.end()) {
					stype = it2->second;
					direction = false;
					goto dispatch;
				}
			}
		}

dispatch:;

		if (stype == PROTOCOL_UNKNOWN) {
			return;
		}

		// create/lookup session data

		client_server_t service_id;

		std::map<client_server_t, packet_capture_data_t>::iterator it = _ip_session_map.end();
		
		{
			if (direction) {
				service_id.client.addr = srcaddr;
				service_id.client.port = srcport;
				service_id.server.addr = dstaddr;
				service_id.server.port = dstport;
			} else {
				service_id.client.addr = dstaddr;
				service_id.client.port = dstport;
				service_id.server.addr = srcaddr;
				service_id.server.port = srcport;
			}

			// 除外処理
			if (get_option()->exclude_clients.find(service_id.client.addr) != get_option()->exclude_clients.end()) {
				return;
			}

			// セッション生成
			it = _ip_session_map.find(service_id);
			if (it == _ip_session_map.end()) {
				it = _ip_session_map.insert(_ip_session_map.end(), std::pair<client_server_t, packet_capture_data_t>(service_id, packet_capture_data_t()));
				it->second.lastupdate = time(0);
				it->second.client.sequence_is_valid = false;
				it->second.client.sequence = 0;
				it->second.server.sequence_is_valid = false;
				it->second.server.sequence = 0;
			}

			if (tcpflags & 0x01) {	// FIN
				it->second.FIN_received = true;		// FIN-flag = ON
				return;
			}

			if (direction) {
				it->second.client_mac_address = srcmac;
			} else {
				it->second.client_mac_address = dstmac;
			}
		}

		if (it == _ip_session_map.end()) { // 監視中のセッションか？
			// 監視対象ではない
			return;
		}

		// 監視対象サーバ
		{
			if (!get_option()->ignore_target_servers) {
				std::map<ip_address_t, int>::const_iterator it1;
				it1 = the_server->_targetservers_info.map.find(service_id.server.addr);
				if (it1 == the_server->_targetservers_info.map.end()) {
					return; // 監視対象ではない
				}
				target_protocol_flags = it1->second;
			}
		}

		// 時刻
		it->second.time_us = packet->microsecond();

		// ストリームバッファ
		packet_capture_data_t::stream_buffer_t *stream = direction ? &it->second.client : &it->second.server;

		if (datalen > 0) {
			if (!stream->sequence_is_valid) {
				stream->sequence = tcp_seqnumber;
				stream->sequence_is_valid = true;
			}
			if (tcp && stream->sequence != tcp_seqnumber) {
				// incorrect sequence number
				session_data_t *s = it->second.get_session_data();
				stream->buffer->clear();
				stream->sequence_is_valid = false;
				/* for DEBUG
				log_printf(packet->microsecond(), "[TCP] C=%s S=%s sequence number incorrect (#%u)"
					, it->first.client.addr.tostring().c_str()
					, it->first.server.addr.tostring().c_str()
					, _packet_number
					);
				*/
			} else {
				stream->sequence += datalen;
			}
			stream->buffer->write(dataptr, datalen);
		}

		//if (_packet_number == 4165) {
		//	_NOP
		//}

		if (!stream->buffer->empty()) {
			// 解析
			int n = dispatch_ip_data_packet(stype, &it->first, &it->second, direction, &*stream->buffer);

			if (n < 0) {
				stream->buffer->clear();
			} else if (n > 0) {
				stream->buffer->consume(n);
			}
		}


		if (it->second.session_data.protocol_mask == 0) {
			// セッション情報を破棄
			_ip_session_map.erase(it);
		} else if (tcp_ctrlflag & 0x01) { // FIN
			// セッション情報を破棄
			
			// _ip_session_map.erase(it);
			it->second.FIN_received = true;		// FIN-flag = ON

		} else {
			it->second.lastupdate = time(0);
		}

		diagnostic_ip_session_map_size = _ip_session_map.size();
	}

	static bool validate_checksum(unsigned char const *ptr, size_t len)
	{
		long sum = 0;
		unsigned short chk = 0;
		unsigned short const *p = (unsigned short const *)ptr;
		size_t i;
		for (i = 0; i < len; i++) {
			unsigned short w = ntohs(p[i]);
			sum += w;
		}
		sum = (sum & 0xffff) + (sum >> 16);
		sum = (sum & 0xffff) + (sum >> 16);
		return sum == 0xffff;
	}

	// IPパケットを解析
	void PacketAnalyzerThread::dispatch_ip4_packet(CapturedPacket const *packet)
	{
		int offset2IPHeader = packet->getOffset2IPHeader();		// adjusted VLAN-Tag size

		{
			unsigned char const *ipheader = packet->dataptr() + offset2IPHeader;
			unsigned short packetlen = BE2(&ipheader[2]);

			// test (ethernet header length + (VLAN-Tag) + ip packet length)
			if (packet->datalen() < (size_t)(offset2IPHeader + packetlen)) {
				if (!get_option()->ignore_defective_packet) {
					// for DEBUG
					// log_printf(packet->microsecond(), "[IP] defective packet (#%u)", _packet_number);
					return; // invalid packet
				}
			}

			// ignore fragmented packet
			unsigned short fragment = BE2(ipheader + 6);
			if (fragment & 0x2000) { // more fragments
				return;
			}
			if ((fragment & 0x1fff) != 0) { // fragment offset
				return;
			}

			unsigned char headerlen = ipheader[0] & 0x0f;
			size_t len = headerlen * 4;
			if (packet->datalen() < len) {
				return; // invalid packet
			}

			// checksum
			if (get_option()->ip_checksum_validation) {
				if (!validate_checksum(ipheader, len / 2)) {
					log_printf(packet->microsecond(), "[IP] checksum incorrect (#%llu)", _packet_number);
					return;
				}
			}
		}

		unsigned char const *ipheader = packet->dataptr() + offset2IPHeader;
		unsigned short ipheaderlen = (ipheader[0] & 0x0f) * 4;
		unsigned short totallen = BE2(&ipheader[2]);
		if (totallen < ipheaderlen) { // Bogus IP Length
			if (packet->datalen() < (unsigned int)offset2IPHeader) {
				return;
			}
			totallen = packet->datalen() - offset2IPHeader;
		}
		unsigned short ipdatalen = totallen - ipheaderlen;
		unsigned char version = (ipheader[0] >> 4) & 0x0f;
		if (version != 4) {
			return;
		}
		if (ipdatalen < 20) {
			return;
		}
		unsigned char headerlen = (ipheader[0] & 0x0f) * 4;
		unsigned char const *ipdataptr = ipheader + headerlen;
		unsigned char protocol = ipheader[9];
		switch (protocol) {
		case 6: // TCP
			dispatch_ip_packet(packet, true, ipdataptr, ipdatalen);
			return;
		case 17: // UDP
			dispatch_ip_packet(packet, false, ipdataptr, ipdatalen);
			break;
		}
	}

	void PacketAnalyzerThread::dispatch_ip6_packet(CapturedPacket const *packet)
	{
		int offset2IPHeader = packet->getOffset2IPHeader();		// adjusted VLAN-Tag size

		// test (EtherFrame(14) + (VLAN-Tag(4)) + IP v6 Header(40))
		if (packet->datalen() < (unsigned int)(offset2IPHeader + 40)) {
			return;
		}

		unsigned char const *ipdataptr = packet->dataptr() + (offset2IPHeader + 40);
		size_t ipdatalen = packet->datalen() - (offset2IPHeader + 40);
		unsigned char protocol = packet->dataptr()[(offset2IPHeader + 6)];
		switch (protocol) {
		case 6: // TCP
			dispatch_ip_packet(packet, true, ipdataptr, ipdatalen);
			return;
		case 17: // UDP
			dispatch_ip_packet(packet, false, ipdataptr, ipdatalen);
			break;
		}
	}

	bool PacketAnalyzerThread::last_packet_file_t::open()
	{
		close();
#ifndef O_BINARY
#define O_BINARY 0
#endif
		fd = ::open(CW_LAST_PACKET_FILE, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, S_IREAD | S_IWRITE);
		if (fd == -1) {
			return false;
		}
		return true;
	}

	void PacketAnalyzerThread::last_packet_file_t::close()
	{
		if (fd != -1) {
			::close(fd);
			fd = -1;
		}
	}

	void PacketAnalyzerThread::savepacket(CapturedPacket const *packet)
	{
		if (_last_packet_file.fd == -1) {
			return;
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

		{
			std::list< std::vector<unsigned char> >::iterator it = _last_packet.insert(_last_packet.end(), std::vector<unsigned char>());
			it->resize(sizeof(pcaprec_hdr_t) + packet->datalen());
			unsigned char *p = &it->at(0);

			pcaprec_hdr_t *packetheader = (pcaprec_hdr_t *)p;
			packetheader->ts_sec = packet->_container->tv_sec;
			packetheader->ts_usec = packet->_container->tv_usec;
			packetheader->incl_len = packet->datalen();
			packetheader->orig_len = packet->datalen();

			memcpy(p + sizeof(pcaprec_hdr_t), packet->dataptr(), packet->datalen());

			unsigned int limit = get_option()->save_packet;

			while (_last_packet.size() > limit) {
				_last_packet.pop_front();
			}
		}

		{
			pcap_hdr_t globalheader;
			globalheader.magic_number = 0xa1b2c3d4;
			globalheader.version_major = 2;
			globalheader.version_minor = 4;
			globalheader.thiszone = 0;
			globalheader.sigfigs = 0;
			globalheader.snaplen = 65535;
			globalheader.network = 1;

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef WIN32
			_lseek(_last_packet_file.fd, 0, SEEK_SET);
			_chsize(_last_packet_file.fd, 0);
#else
			lseek(_last_packet_file.fd, 0, SEEK_SET);
			ftruncate(_last_packet_file.fd, 0);
#endif
			write(_last_packet_file.fd, &globalheader, sizeof(pcap_hdr_t));
			std::list< std::vector<unsigned char> >::const_iterator it;
			for (it = _last_packet.begin(); it != _last_packet.end(); it++) {
				write(_last_packet_file.fd, &it->at(0), it->size());
			}

		}
	}

	// パケットを解析
	void PacketAnalyzerThread::dispatch_raw_packet(CapturedPacket const *packet)
	{
		_packet_number = packet->_container->packetnumber;

		if (get_option()->save_packet > 0) {
			savepacket(packet);
		}

		if (get_option()->dump_packet > 0) {
			the_server->_dump.add(packet);
		}

//if (_packet_number == 7869) {
//	_NOP
//}

		// change to VLAN-Tag
		unsigned short ethernettype = packet->getFrameType();
		
		switch (ethernettype) {
		case 0x0800:
			dispatch_ip4_packet(packet);
			return;
		case 0x86dd:
			dispatch_ip6_packet(packet);
			return;
		}
	}

	void PacketAnalyzerThread::run()
	{
		while (1) {
			try {
				std::list<CapturedPacket> q;
				{
					AutoLock lock(&_mutex);
					std::swap(q, _queue);
					diagnostic_packet_capture_queue_size = q.size();
				}
				if (q.empty()) {
					if (_terminate) {
						break;
					}
					_event.wait(); // パケット到着待ち
				} else {
					{
						AutoLock lock(&the_server->_mutex_for_settings); // オプションをロック

						for (std::list<CapturedPacket>::const_iterator it = q.begin(); it != q.end(); it++) {
							CapturedPacket const &item = *it;
							dispatch_raw_packet(&item);
						}
					}
				}
				if (_cleanup) {
					dispose_forgotten_data();
					_cleanup = false;
				}
			} catch (std::bad_alloc const &e) {
				on_bad_alloc(e, __FILE__, __LINE__);
			}
		}
	}


} // namespace ContentsWatcher


