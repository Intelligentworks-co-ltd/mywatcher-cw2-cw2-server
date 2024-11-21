
// PacketAnalyzerThreadのNFS解析処理

#include "cw_analizer.h"
#include "common/text.h"
#include "common/combinepath.h"


namespace ContentsWatcher { // nfs

	enum {
		RPC_PORTMAP_GETPORT = 3,

		RPC_MOUNT_MOUNT = 1,
		RPC_MOUNT_UMOUNT = 3,

		RPC_NFS_LOOKUP = 3,
		RPC_NFS_READ = 6,
		RPC_NFS_WRITE = 7,
		RPC_NFS_CREATE = 8,
		RPC_NFS_REMOVE = 12,
	};

	void set_nfs_flag(session_data_t *session, bool f)
	{
		if (f) {
			session->protocol_mask |= ProtocolMask::NFS;
		} else {
			session->protocol_mask &= ~ProtocolMask::NFS;
		}
	}

	static inline void nfs_peek_stream(blob_t *stream, unsigned char const **ptr, unsigned char const **end)
	{
		unsigned char const *p = stream->c_str();
		*ptr = p;
		*end = p + stream->size();
	}

	static inline bool nfs_peek_stream(size_t len, blob_t *stream, unsigned char const **ptr)
	{
		unsigned char const *p = stream->peek(len);
		if (!p) {
			return false;
		}
		*ptr = p;
		return true;
	}

	int PacketAnalyzerThread::on_rpc_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream)
	{
		if (!get_option()->ignore_target_servers) {
			if (!(target_protocol_flags & target_protocol::NFS)) {
				return -1; // 監視対象ではない
			}
		}

		session_data_t *session = data->get_session_data();
		set_nfs_flag(session, true);

		unsigned long header;
		{
			unsigned char const *p;
			if (!nfs_peek_stream(4, stream, &p)) {
				return 0;
			}
			header = BE4(p);
		}

		if (direction) {
			rpc_request_t rpc;
			std::string cred_name;
			int rpccalloffset;

			{
				unsigned char const *p;
				if (!nfs_peek_stream(36, stream, &p)) {
					return 0;
				}

				rpc.header = header;
				rpc.xid = BE4(p + 4);
				rpc.sid = sid;
				rpc.data = data;

				rpc.message_type = BE4(p + 8);
				rpc.version = BE4(p + 12);
				rpc.program_number = BE4(p + 16);
				rpc.program_version = BE4(p + 20);
				rpc.procedure = BE4(p + 24);

				unsigned char const *p_cred = p + 28;

				unsigned long rpc_cred_flavor = BE4(p + 28);
				unsigned long rpc_cred_len = BE4(p + 32);

				{ // credential
					switch (rpc_cred_flavor) {
					case 1: // AUTH_UNIX
						{
							unsigned long l = BE4(p_cred + 0x0c);
							if (!nfs_peek_stream(28 + 16 + l, stream, &p)) {
								return -1;
							}
							unsigned char const *left = p_cred + 0x10;
							unsigned char const *right = left + l;
							cred_name.assign(left, right);
						}
						break;
					}
				}

				{
					if (!nfs_peek_stream(36 + rpc_cred_len + 8, stream, &p)) {
						return 0;
					}
					p += 36 + rpc_cred_len;
					unsigned long rpc_veri_flavor = BE4(p);
					unsigned long rpc_veri_len = BE4(p + 4);

					rpccalloffset = 36 + rpc_cred_len + 8 + rpc_veri_len;
				}
			}

			if (rpc.message_type == 0) { // call

				session->rpc_session.xid = rpc.xid;
				session->rpc_session.program = rpc.program_number;
				session->rpc_session.procedure = rpc.procedure;

				switch (rpc.program_number) {

				case RPC_PORTMAP:
					if (rpc.procedure == RPC_PORTMAP_GETPORT) {
						unsigned char const *ptr;
						unsigned char const *end;
						nfs_peek_stream(stream, &ptr, &end);
						unsigned char const *rpccall = ptr + rpccalloffset;
						if (rpccall + 16 > end) {
							break;
						}
						rpc_program_number_t program = BE4(rpccall);
						session->rpc_session.param_program_number = program;
					}
					break;

				case RPC_MOUNT:
					switch (rpc.procedure) {
					case RPC_MOUNT_MOUNT:
						{
							unsigned char const *ptr;
							unsigned char const *end;
							nfs_peek_stream(stream, &ptr, &end);
							unsigned char const *rpccall = ptr + rpccalloffset;
							if (rpccall + 4 > end) {
								return 0;
							}
							unsigned long len = BE4(rpccall);
							if (rpccall + 4 + len > end) {
								return 0;
							}
							std::string dir((char const *)(rpccall + 4), (size_t)len);
							session->rpc_session.param_directory = dir;
							session->rpc_session.param_credential = cred_name;
						}
						break;
					case RPC_MOUNT_UMOUNT:
						{
							unsigned char const *ptr;
							unsigned char const *end;
							nfs_peek_stream(stream, &ptr, &end);
							unsigned char const *rpccall = ptr + rpccalloffset;
							if (rpccall + 4 > end) {
								return 0;
							}
							unsigned long len = BE4(rpccall);
							if (rpccall + 4 + len > end) {
								return 0;
							}
							std::string name((char const *)(rpccall + 4), (size_t)len);
							session->rpc_session.param_directory = name;
							session->rpc_session.param_credential = cred_name;
						}
						break;
					}
					break;

				case RPC_NFS:
					switch (rpc.procedure) {
					case RPC_NFS_LOOKUP:
						{
							unsigned char const *ptr;
							unsigned char const *end;
							nfs_peek_stream(stream, &ptr, &end);
							unsigned char const *p = ptr + rpccalloffset;
							unsigned long l;
							if (p + 4 > end) {
								return 0;
							}
							l = BE4(p);
							p += 4;
							if (p + l > end) {
								return 0;
							}
							nfs_file_handle_t h(p, p + l); // dir handle
							p += l;
							if (p + 4 > end) {
								return 0;
							}
							l = BE4(p);
							p += 4;
							if (p + l > end) {
								return 0;
							}
							std::string name((char const *)p, l);
							session->nfs_session.temp.current_file_handle = h;
							session->nfs_session.temp.sub_item_name = name;
						}
						break;

					case RPC_NFS_READ:
					case RPC_NFS_WRITE:
						{
							unsigned char const *rpccall;
							if (!nfs_peek_stream(rpccalloffset + 4, stream, &rpccall)) {
								return -1;
							}
							rpccall += rpccalloffset;

							unsigned long l = BE4(rpccall);

							if (!nfs_peek_stream(rpccalloffset + 4 + l, stream, &rpccall)) {
								return -1;
							}
							rpccall += rpccalloffset;

							unsigned char const *p = rpccall + 4;
							nfs_file_handle_t handle(p, p + l);
							session->nfs_session.temp.current_file_handle = handle;
						}
						break;

					case RPC_NFS_CREATE:
						{
							unsigned char const *ptr;
							unsigned char const *end;
							nfs_peek_stream(stream, &ptr, &end);
							unsigned char const *p = ptr + rpccalloffset;
							if (p + 4 > end) return 0;
							unsigned long nfs_dir_len = BE4(p);
							unsigned char const *nfs_dir_ptr = (unsigned char const *)(p + 4);
							p += 4 + nfs_dir_len;
							if (p + 4 > end) return 0;
							unsigned long nfs_name_len = BE4(p);
							p += 4;
							if (p > end) return 0;
							char const *nfs_name_ptr = (char const *)p;

							std::string dir;
							if (nfs_dir_len > 0) {
								nfs_file_handle_t h(nfs_dir_ptr, nfs_dir_ptr + nfs_dir_len);
								std::map<nfs_file_handle_t, nfs_file_item_t>::const_iterator it = session->nfs_session.pathmap.find(h);
								if (it != session->nfs_session.pathmap.end()) {
									dir = it->second.name;
								}
							}
							if (dir.empty()) {
								dir = "?/";
							}

							std::string name(nfs_name_ptr, nfs_name_len);
							session->nfs_session.temp.path = CombinePath(dir.c_str(), name.c_str(), '/');
						}
						break;
					case RPC_NFS_REMOVE:
						{
							unsigned char const *ptr;
							unsigned char const *end;
							nfs_peek_stream(stream, &ptr, &end);
							unsigned char const *p = ptr + rpccalloffset;
							if (p + 4 > end) return 0;
							unsigned long nfs_dir_len = BE4(p);
							unsigned char const *nfs_dir_ptr = (unsigned char const *)(p + 4);
							p += 4 + nfs_dir_len;
							if (p + 4 > end) return 0;
							unsigned long nfs_name_len = BE4(p);
							p += 4;
							if (p > end) return 0;
							char const *nfs_name_ptr = (char const *)p;

							std::string dir;
							if (nfs_dir_len > 0) {
								nfs_file_handle_t h(nfs_dir_ptr, nfs_dir_ptr + nfs_dir_len);
								std::map<nfs_file_handle_t, nfs_file_item_t>::const_iterator it = session->nfs_session.pathmap.find(h);
								if (it != session->nfs_session.pathmap.end()) {
									dir = it->second.name;
								}
							}
							if (dir.empty()) {
								dir = "?/";
							}

							std::string name(nfs_name_ptr, nfs_name_len);
							session->nfs_session.temp.path = CombinePath(dir.c_str(), name.c_str(), '/');
						}
						break;
					}
					break;
				}

			}

		} else { // !direction

			rpc_response_t rpc;
			int rpccalloffset;

			unsigned long rpc_message_type;
			unsigned long rpc_reply_state;
			unsigned long rpc_veri_flavor;
			unsigned long rpc_veri_len;
			unsigned long rpc_accept_state;

			unsigned char const *p;
			{
				unsigned char const *ptr;
				unsigned char const *end;
				nfs_peek_stream(stream, &ptr, &end);

				rpc.header = header;
				rpc.xid = BE4(ptr + 4);
				rpc.sid = sid;
				rpc.data = data;

				rpc_message_type = BE4(ptr + 8);
				rpc_reply_state = BE4(ptr + 12);
				rpc_veri_flavor = BE4(ptr + 16);
				rpc_veri_len = BE4(ptr + 20);

				p = ptr + 24 + rpc_veri_len;

				if (p + 4 > end) {
					return 0;
				}

				rpc_accept_state = BE4(p);

				rpccalloffset = 24 + rpc_veri_len + 4;
			}

			session_data_t *session = data->get_session_data();

			if (rpc_message_type == 1 && rpc_reply_state == 0) { // reply
				if (rpc.xid == session->rpc_session.xid) { // reply && accepted
					switch (session->rpc_session.program) {
					case RPC_PORTMAP:
						switch (session->rpc_session.procedure) {
						case RPC_PORTMAP_GETPORT:
							{
								protocol_type_t stype = PROTOCOL_UNKNOWN;
								switch (session->rpc_session.param_program_number) {
								case RPC_NFS:
									stype = PROTOCOL_RPC_NFS;
									break;
								case RPC_MOUNT:
									stype = PROTOCOL_RPC_MOUNT;
									break;
								}
								if (stype != PROTOCOL_UNKNOWN) {
									unsigned char const *ptr;
									unsigned char const *end;
									nfs_peek_stream(stream, &ptr, &end);
									unsigned char const *rpccall = ptr + rpccalloffset;
									unsigned short port = (unsigned short)BE4(rpccall);
									std::map<ip_address_t, rpc_server_data_t>::iterator it1 = _rpc_server_map.insert(_rpc_server_map.end(), std::pair<ip_address_t, rpc_server_data_t>(rpc.sid->server.addr, rpc_server_data_t()));
									std::map<rpc_program_number_t, unsigned short>::iterator it2 = it1->second.rpc_program_data.insert(it1->second.rpc_program_data.end(), std::pair<rpc_program_number_t, unsigned short>(session->rpc_session.param_program_number, 0));
									it2->second = port;
									it1->second.stream_type_map[port] = stype;
log_printf(data->time_us, "[portmap] %u = %u", session->rpc_session.param_program_number, port);
								}
							}
							break;
						}
						break;
					case RPC_MOUNT:
						switch (session->rpc_session.procedure) {
						case RPC_MOUNT_MOUNT:
							{
								unsigned char const *ptr;
								unsigned char const *end;
								nfs_peek_stream(stream, &ptr, &end);
								unsigned char const *rpccall = ptr + rpccalloffset;
								if (rpccall + 8 > end) {
									return 0;
								}
								unsigned long status = BE4(rpccall);
								unsigned long length = BE4(rpccall + 4);
								if (status != 0) {
									return -1;
								}
								if (rpccall + 8 + length > end) {
									return 0;
								}

								std::string cred_name = session->rpc_session.param_credential;
								std::string name = session->rpc_session.param_directory;
								unsigned char const *fhptr = rpccall + 8;
								unsigned char const *fhend = fhptr + length;
								std::map<nfs_file_handle_t, nfs_file_item_t>::iterator it = session->nfs_session.pathmap.insert(session->nfs_session.pathmap.end(), std::pair<nfs_file_handle_t, nfs_file_item_t>(nfs_file_handle_t(fhptr, fhend), nfs_file_item_t()));
								if (length) {
									it->second.name = name;
								}
log_printf(data->time_us, "[nfs] mount \"%s\" (%s)", name.c_str(), cred_name.c_str());
							}
							break;
						case RPC_MOUNT_UMOUNT:
							{
								std::string cred_name = session->rpc_session.param_credential;
								std::string name = session->rpc_session.param_directory;
								std::map<nfs_file_handle_t, nfs_file_item_t>::iterator it;
								for (it = session->nfs_session.pathmap.begin(); it != session->nfs_session.pathmap.end(); it++) {
									if (it->second.name == name) {
										session->nfs_session.pathmap.erase(it);
										break;
									}
								}
log_printf(data->time_us, "[nfs] umount \"%s\" (%s)", session->rpc_session.param_directory.c_str(), cred_name.c_str());
								set_nfs_flag(session, false);
							}
							break;
						}
						break;
					case RPC_NFS:
						switch (session->rpc_session.procedure) {
						case RPC_NFS_LOOKUP:
							{
								unsigned char const *ptr;
								unsigned char const *end;
								nfs_peek_stream(stream, &ptr, &end);
								unsigned char const *rpccall = ptr + rpccalloffset;
								if (rpc_accept_state == 0) {
									if (rpccall + 8 > end) {
										return 0;
									}
									unsigned long l = BE4(rpccall + 4);
									unsigned char const *p = rpccall + 8;
									if (p + l > end) {
										return 0;
									}
									nfs_file_handle_t handle(p, p + l);
									std::string name = session->nfs_session.temp.sub_item_name;
									std::map<nfs_file_handle_t, nfs_file_item_t>::const_iterator it;
									it = session->nfs_session.pathmap.find(session->nfs_session.temp.current_file_handle);
									if (it != session->nfs_session.pathmap.end()) {
										name = CombinePath(it->second.name.c_str(), name.c_str(), '/');
									}
									session->nfs_session.pathmap[handle] = nfs_file_item_t(name, nfs_file_handle_t());
log_printf(data->time_us, "[nfs] lookup \"%s\" (%s)"
	   , name.c_str()
	   , handle.tostring().c_str()
	   );
								}
							}
							break;

						case RPC_NFS_READ:
							{
								if (rpc_accept_state == 0) {
									std::map<nfs_file_handle_t, nfs_file_item_t>::const_iterator it;
									it = session->nfs_session.pathmap.find(session->nfs_session.temp.current_file_handle);
									if (it != session->nfs_session.pathmap.end()) {
										log_printf(data->time_us, "[nfs] read \"%s\""
											, it->second.name.c_str()
											);
									}
								}
							}
							break;

						case RPC_NFS_WRITE:
							{
								if (rpc_accept_state == 0) {
									std::map<nfs_file_handle_t, nfs_file_item_t>::const_iterator it;
									it = session->nfs_session.pathmap.find(session->nfs_session.temp.current_file_handle);
									if (it != session->nfs_session.pathmap.end()) {
										log_printf(data->time_us, "[nfs] write \"%s\""
											, it->second.name.c_str()
											);
									}
								}
							}
							break;
						case RPC_NFS_CREATE:
log_printf(data->time_us, "[nfs] open \"%s\"", session->nfs_session.temp.path.c_str());
							break;
						case RPC_NFS_REMOVE:
log_printf(data->time_us, "[nfs] delete \"%s\"", session->nfs_session.temp.path.c_str());
							break;
						}
						break;
					}
				}
			}
		}

		return -1;
	}

} // namespace ContentsWatcher


