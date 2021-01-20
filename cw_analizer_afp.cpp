
// PacketAnalyzerThreadのAFP解析処理

#include "cw_analizer.h"
#include "common/text.h"
#include "common/combinepath.h"


static inline ustring sjis_string_to_ustring(char const *ptr, size_t len)
{
	return soramimi::jcode::convert(soramimi::jcode::SJIS, ptr, len);
}

static inline ustring sjis_string_to_ustring(std::string const &str)
{
	return soramimi::jcode::convert(soramimi::jcode::SJIS, str.c_str(), str.size());
}

namespace ContentsWatcher { // afp

	afp_dir_item_t *session_data_t::afp_session_t::find_did(afp_volume_info_t *v, afp_did_t did)
	{
		std::list<afp_dir_map_t>::iterator it;
		for (it = v->dir_map_table.begin(); it != v->dir_map_table.end(); it++) {
			afp_dir_map_t::iterator itd = it->find(did);
			if (itd != it->end()) {
				return &itd->second;
			}
		}

		return 0;
	}

	ustring session_data_t::afp_session_t::get_folder(client_server_t const *sid, afp_vid_t vid, afp_did_t did)
	{
		int nest_limit = 100;

		ustring path;
		std::map<afp_vid_t, afp_volume_info_t>::iterator itv = volumemap.find(vid);
		if (itv != volumemap.end()) {
			while (1) {
				if (did < 3) {
					break;
				}
				afp_dir_item_t const *p = find_did(&itv->second, did);
				if (!p) {
					break;
				}
				if (path.empty()) {
					path = p->name;
				} else {
					path = CombinePath(p->name.c_str(), path.c_str(), '/');
				}

				if (did == p->parent_did) {
					return ustring(); // could not resolve did
				}

				did = p->parent_did;
				nest_limit--;

				if (nest_limit < 1) {
					return ustring(); // could not resolve did
				}

			}
			ustring volume = itv->second.name;
			if (volume.empty()) {
				afp_dir_item_t const *p = find_did(&itv->second, did);
				if (p) {
					path = CombinePath(p->name.c_str(), path.c_str(), '/');
				} else {
					char tmp[100];
					sprintf(tmp, "<DID:%u>", did);
					path = CombinePath(sjis_string_to_ustring(tmp).c_str(), path.c_str(), '/');
				}
			} else {
				path = CombinePath(volume.c_str(), path.c_str(), '/');
			}
		} else {
			char tmp[100];
			sprintf(tmp, "<VID:%u>/<DID:%u>", vid, did);
			path = sjis_string_to_ustring(tmp);
		}

		char tmp[100];
		sprintf(tmp, "//%s/", sid->server.addr.tostring().c_str());

		uchar_t const *p = path.c_str();
		if (*p == '/') {
			p++;
		}

		return CombinePath(sjis_string_to_ustring(tmp).c_str(), p, '/');
	}

	ustring session_data_t::afp_session_t::make_path(client_server_t const *sid, afp_vid_t vid, afp_did_t did, ustring const &name)
	{
		ustring path = get_folder(sid, vid, did);
		if (path.empty()) {
			path = name;
		} else {
			path = CombinePath(path.c_str(), name.c_str(), '/');
		}
		return path;
	}

	void session_data_t::afp_session_t::insert_dir_entry(afp_vid_t vid, afp_did_t did, afp_did_t fid, ustring const &name, bool isdir, unsigned long long packetnumber)
	{
		if (vid == 0 || did == 0) {
			return;
		}
		if (did == fid) {
			return;
		}
		//if (name.empty()) {
		//	return;
		//}

		afp_volume_info_t *volume = get_volume_info(vid);

		afp_dir_item_t child_key;
		afp_dir_item_t parent_key;

		child_key.parent_did = did;
		child_key.name = name;
		child_key.packetnumber = packetnumber;

		if (isdir) { // ディレクトリ

			bool parent_found = false;

			{
				std::list<afp_dir_map_t>::iterator it1;
				for (it1 = volume->dir_map_table.begin(); it1 != volume->dir_map_table.end(); it1++) {
					afp_dir_map_t::iterator it2 = it1->find(did);
					if (it2 != it1->end()) {
						parent_key = it2->second;
						it1->erase(it2);
						parent_found = true;
					}
					afp_dir_map_t::iterator it3 = it1->find(fid);
					if (it3 != it1->end()) {
						it1->erase(it3);
					}
				}
			}
	
			bool insertnewmap = false;
			if (volume->dir_map_table.empty()) {
				insertnewmap = true;
			} else if (volume->dir_map_table.front().size() >= 100) {
				if (volume->dir_map_table.size() >= 10) {
					volume->dir_map_table.pop_back();
				}
				insertnewmap = true;
			}

			afp_dir_item_t *item;
			{
				afp_dir_map_t *p;

				if (insertnewmap) {
					std::list<afp_dir_map_t>::iterator it4 = volume->dir_map_table.insert(volume->dir_map_table.begin(), afp_dir_map_t());
					p = &*it4;
				} else {
					p = &volume->dir_map_table.front();
				}

				if (parent_found) {
					p->insert(p->end(), std::pair<afp_did_t, afp_dir_item_t>(did, parent_key));
				}

				afp_dir_map_t::iterator it5 = p->insert(p->end(), std::pair<afp_did_t, afp_dir_item_t>(fid, afp_dir_item_t()));
				item = &it5->second;
			}

			*item = child_key;

			volume->is_dir_table.insert(child_key);

		} else { // ファイル

			std::set<afp_dir_item_t>::iterator it = volume->is_dir_table.find(child_key);
			if (it != volume->is_dir_table.end()) {
				volume->is_dir_table.erase(it);
			}

		}

	}

	enum AFPCOMMAND {
		AFP_FPCloseFork = 4,
		AFP_FPCopyFile = 5,
		AFP_FPCreateDir = 6,
		AFP_FPCreateFile = 7,
		AFP_FPDelete = 8,
		AFP_FPLogin = 18,
		AFP_LoginCont = 19,
		AFP_FPLogout = 20,
		AFP_FPMoveAndRename = 23,
		AFP_FPOpenVol = 24,
		AFP_FPOpenFork = 26,
		AFP_FPRead = 27,
		AFP_FPRename = 28,
		AFP_FPWrite = 33,
		AFP_FPGetFileDirParams = 34,
		AFP_FPExchangeFiles = 42,
		AFP_FPReadExt = 60,
		AFP_FPWriteExt = 61,
		AFP_FPLoginExt = 63,
	};

	void fix_mac_unicode(ustring *str)
	{
		ustringbuffer sb;
		uchar_t const *ptr = str->c_str();
		while (*ptr) {
			uchar_t c = ptr[0];
			if (c == '/') {
				c = ':';
			} else if (ptr[1] == 0x3099) { // 濁音
				c += 1;
				ptr++;
			} else if (ptr[1] == 0x309a) { // 半濁音
				c += 2;
				ptr++;
			}
			sb.put(c);
			ptr++;
		}
		*str = sb.str();
	}

	inline ustring get_mac_utf8_string(char const *ptr, size_t len)
	{
		ustring str;
		str = utf8_string_to_ustring(ptr, len);
		fix_mac_unicode(&str);
		return str;
	}

	static bool get_user_name_string(unsigned char const *p, unsigned char const *end, ustring *out, unsigned char const **next_p)
	{
		if (p[0] == 3) {
			unsigned short l = BE2(p + 1);
			if (l == 0) {
				return false;
			}
			if (p + 3 + l > end) {
				if (next_p) {
					*next_p = 0;
				}
				return false;
			}
			p += 3;
			*out = get_mac_utf8_string((char const *)p, l);
			if (next_p) {
				*next_p = p + l;
			}
			return true;
		}
		return false;
	}

	ustring get_mac_sjis(unsigned char const *ptr, size_t len)
	{
		return soramimi::jcode::convert(soramimi::jcode::SJIS, (char const *)ptr, len);
	}

	static bool get_path_string(unsigned char const *p, unsigned char const *end, ustring *out, unsigned char const **next_p)
	{
		if (p[0] == 1) {
			if (p + 7 > end) {
				if (next_p) {
					*next_p = 0;
				}
				return false;
			}
			unsigned short l = BE2(p + 5);
			if (p + 7 + l > end) {
				if (next_p) {
					*next_p = 0;
				}
				return false;
			}
			p += 7;
			*out = get_mac_sjis(p, l);
			if (next_p) {
				*next_p = p + l;
			}
			fix_mac_unicode(out);
			return true;
		} else if (p[0] == 2) {
			if (p + 2 > end) {
				if (next_p) {
					*next_p = 0;
				}
				return false;
			}
			unsigned char l = p[1];
			if (p + 2 + l > end) {
				if (next_p) {
					*next_p = 0;
				}
				return false;
			}
			p += 2;
			*out = get_mac_sjis(p, l);
			if (next_p) {
				*next_p = p + l;
			}
			fix_mac_unicode(out);
			return true;
		} else if (p[0] == 3) {
			unsigned short l = BE2(p + 5);
			if (p + 7 + l > end) {
				if (next_p) {
					*next_p = 0;
				}
				return false;
			}
			p += 7;
			*out = get_mac_utf8_string((char const *)p, l);
			if (next_p) {
				*next_p = p + l;
			}
			return true;
		}
		return false;
	}

	void set_afp_flag(session_data_t *session, bool f)
	{
		if (f) {
			session->protocol_mask |= ProtocolMask::AFP;
		} else {
			session->protocol_mask &= ~ProtocolMask::AFP;
		}
	}

	static inline void afp_peek_stream(blob_t const *stream, unsigned char const **ptr, unsigned char const **end, unsigned char const **afp)
	{
		unsigned char const *p = stream->c_str();
		*ptr = p;
		*end = p + stream->size();
		*afp = p + 0x10;
	}

	static inline bool afp_peek_stream(size_t len, blob_t const *stream, unsigned char const **ptr, unsigned char const **end, unsigned char const **afp)
	{
		assert(len > 0x10);
		unsigned char const *p = stream->peek(len);
		if (!p) {
			return false;
		}
		*ptr = p;
		*end = p + len;
		*afp = p + 0x10;
		return true;
	}

	int PacketAnalyzerThread::on_a_dsi_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream)
	{
		if (_packet_number == 1921) {
			_NOP
		}

		if (!get_option()->ignore_target_servers) {
			if (!(target_protocol_flags & target_protocol::AFP)) {
				return -1; // 監視対象ではない
			}
		}

		session_data_t *session = data->get_session_data();
		set_afp_flag(session, true);

		struct {
			unsigned char flags;
			unsigned char command;
			unsigned short request_id;
			unsigned long offset;
			unsigned long length;
		} dsi;

		{
			unsigned char const *p = stream->peek(0x10);
			if (!p) return 0;

			dsi.flags = p[0];
			dsi.command = p[1];
			dsi.request_id = BE2(p + 2);
			dsi.offset = BE4(p + 4);
			dsi.length = BE4(p + 8);
		}
		switch (dsi.command) {
		case 1: //DSICloseSession
		case 2: //DSICommand
		case 3: //DSIGetStatus
		case 4: //DSIOpenSession
		case 5: //DSITickle
		case 6: //DSIWrite
		case 8: //DSIAttention
			_NOP
			break;
		default:
			_NOP
			return -1; // DSIヘッダではない
		}

		if (dsi.flags != 0 && dsi.flags != 1) {
			return -1;
		}

		if (stream->size() < 0x10 + dsi.length) {
			if (dsi.length > 1000000) {
				return -1; // もしかしてDSIヘッダのトレースに失敗した？
			}
			return 0;
		}

		if (direction) {
			if (dsi.flags == 0) { // request

				unsigned char afp_command = 0;
				{
					unsigned char const *p = stream->peek(0x11);
					if (!p) {
						return -1;
					}
					afp_command = p[0x10];
				}

				switch (afp_command) {

				case 0:
				case 255:
					return -1;

				case AFP_FPLogin:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						unsigned char const *p;
						int l;
						p = afp + 1;
						if (p > end) {
							return 0;
						}
						l = *p++;
						if (p + l > end) {
							return 0;
						}
						std::string afpver((char const *)p, (char const *)p + l);
						p += l;
						if (p > end) {
							return 0;
						}
						l = *p++;
						if (p + l > end) {
							return 0;
						}
						std::string uam((char const *)p, (char const *)p + l);
						p += l;
						if (p > end) {
							return 0;
						}
						l = *p++;
						std::string name;
						if (p + l > end) {
							name = "<unknown>";
						} else {
							name.assign((char const *)p, (char const *)p + l);
						}

						session->afp_session.temp.request_id = dsi.request_id;
						session->afp_session.temp.command = afp_command;
						session->afp_session.temp.name1 = sjis_string_to_ustring(name);
					}
					break;

				case AFP_FPLoginExt:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 4 > end) {
							return 0;
						}
						unsigned char const *p;
						p = afp + 4;

						if (p + 1 > end || p + 1 + *p > end) {
							return 0;
						}
						std::string afpver(p + 1, p + 1 + *p);
						p += 1 + *p;

						if (p + 1 > end || p + 1 + *p > end) {
							return 0;
						}
						std::string uam(p + 1, p + 1 + *p);
						p += 1 + *p;

						ustring name;
						if (!get_user_name_string(p, end, &name, 0)) {
							break;
						}

						session->afp_session.temp.request_id = dsi.request_id;
						session->afp_session.temp.command = afp_command;
						session->afp_session.temp.name1 = name;
					}
					break;

				case AFP_LoginCont:
					{
						session->afp_session.temp.request_id = dsi.request_id;
						session->afp_session.temp.command = afp_command;
					}
					break;

				case AFP_FPLogout:
					{
						if (!session->afp_session.login_name.empty()) {
							// FPLogout request に対して、server shutdownやclose sessionで応答するサーバがあるので、
							// FPLogout replyを待たずにログアウトは成功するものとみなす。
							{
								AutoLock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									get_processor()->on_logout(ei);
								}
							}
							session->afp_session = session_data_t::afp_session_t(); // cleanup
							session->afp_session.temp.clear();
							set_afp_flag(session, false);
						}
					}
					break;

				case AFP_FPOpenVol:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						unsigned char const *p = afp;
						if (p + 4 > end) {
							return 0;
						}
						unsigned short bitmap = BE2(p + 2);
						p += 4;

						if (p + 1 > end) {
							return 0;
						}
						int l = *p++;
						if (p + l > end) {
							return 0;
						}
						ustring name = get_mac_utf8_string((char const *)p, l);

						session->afp_session.temp.request_id = dsi.request_id;
						session->afp_session.temp.command = afp_command;
						session->afp_session.temp.name1 = name;
					}
					break;

				case AFP_FPOpenFork:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 12 > end) {
							return 0;
						}
						unsigned short vid = BE2(afp + 2);
						unsigned long did = BE4(afp + 4);
						unsigned short bitmap = BE2(afp + 8);
						unsigned short access_mode = BE2(afp + 10);
						ustring name;
						if (get_path_string(afp + 12, end, &name, 0)) {
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = session->afp_session.make_path(sid, vid, did, name);
						}
					}
					break;

				case AFP_FPCloseFork:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 4 > end) {
							return 0;
						}
						afp_fid_t fork = BE2(afp + 2);
						session->afp_session.temp.request_id = dsi.request_id;
						session->afp_session.temp.command = afp_command;
						session->afp_session.temp.fid = fork;
					}
					break;

				case AFP_FPCopyFile:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 14 > end) {
							return 0;
						}
						unsigned short src_vid = BE2(afp + 2);
						unsigned long src_did = BE4(afp + 4);
						unsigned short dst_vid = BE2(afp + 8);
						unsigned long dst_did = BE4(afp + 10);
						unsigned char const *p = afp + 14;
						ustring srcpath;
						ustring dstpath;
						ustring newname;
						if (get_path_string(p, end, &srcpath, &p)) {
							if (get_path_string(p, end, &dstpath, &p)) {
								if (get_path_string(p, end, &newname, &p)) {
									if (newname.empty()) {
										newname = srcpath;
									}
								}
							}
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = session->afp_session.make_path(sid, src_vid, src_did, srcpath);
							session->afp_session.temp.name2 = session->afp_session.make_path(sid, dst_vid, dst_did, newname);
						}
					}
					break;

				case AFP_FPCreateDir:
				case AFP_FPCreateFile:
				case AFP_FPDelete:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 8 > end) {
							return 0;
						}
						unsigned short vid = BE2(afp + 2);
						unsigned long did = BE4(afp + 4);
						ustring name;

						if (get_path_string(afp + 8, end, &name, 0)) {

							bool isdir = false;

							afp_volume_info_t *volume = session->afp_session.get_volume_info(vid);
							if (volume) {
								afp_dir_item_t item;
								item.parent_did = did;
								item.name = name;
								std::set<afp_dir_item_t>::iterator it = volume->is_dir_table.find(item);
								if (it != volume->is_dir_table.end()) {
									if (afp_command == AFP_FPDelete) {
										volume->is_dir_table.erase(it);
									}
									isdir = true;
								}
							}

							session->afp_session.temp.vid = vid;
							session->afp_session.temp.did = did;
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = name;//session->afp_session.make_path(sid, vid, did, name);
							session->afp_session.temp.isdir = isdir;

						}
					}
					break;

				case AFP_FPMoveAndRename:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 12 > end) {
							return 0;
						}
						unsigned short vid = BE2(afp + 2);
						unsigned long src_did = BE4(afp + 4);
						unsigned long dst_did = BE4(afp + 8);
						unsigned char const *p;
						p = afp + 0x0c;
						ustring srcpath;
						ustring dstpath;
						ustring newname;
						if (get_path_string(p, end, &srcpath, &p)) {
							if (get_path_string(p, end, &dstpath, &p)) {
								if (get_path_string(p, end, &newname, 0)) {
									if (newname.empty()) {
										newname = srcpath;
									}
								}
							}
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = session->afp_session.make_path(sid, vid, src_did, srcpath);
							session->afp_session.temp.name2 = session->afp_session.make_path(sid, vid, dst_did, newname);
						}
					}
					break;

				case AFP_FPGetFileDirParams:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						unsigned char const *p;
						if (afp + 12 > end) {
							return 0;
						}
						unsigned short vid = BE2(afp + 2);
						unsigned long did = BE4(afp + 4);
						unsigned short filebitmap = BE2(afp + 8);
						unsigned short dirbitmap = BE2(afp + 10);
						ustring name;
						if (get_path_string(afp + 12, end, &name, &p)) {
							session->afp_session.temp.vid = vid;
							session->afp_session.temp.did = did;
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = name;
						}
					}
					break;

				case AFP_FPExchangeFiles:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 12 > end) {
							return 0;
						}
						unsigned short vid = BE2(afp + 2);
						unsigned long src_did = BE4(afp + 4);
						unsigned long dst_did = BE4(afp + 8);
						unsigned char const *p = afp + 12;
						ustring srcpath;
						ustring dstpath;
						if (get_path_string(p, end, &srcpath, &p)) {
							if (get_path_string(p, end, &dstpath, &p)) {
								session->afp_session.temp.request_id = dsi.request_id;
								session->afp_session.temp.command = afp_command;
								session->afp_session.temp.name1 = session->afp_session.make_path(sid, vid, src_did, srcpath);
								session->afp_session.temp.name2 = session->afp_session.make_path(sid, vid, dst_did, dstpath);
							}
						}
					}
					break;

				case AFP_FPRead:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						if (!afp_peek_stream(0x10 + 12, stream, &ptr, &end, &afp)) {
							return -1;
						}
						afp_fid_t fork = BE2(afp + 2);
						unsigned long offset = BE4(afp + 4);
						unsigned long length = BE4(afp + 8);
						std::map<afp_fid_t, afp_fork_data_t>::const_iterator it = session->afp_session.forkmap.find(fork);
						if (it != session->afp_session.forkmap.end()) {
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = it->second.name;
							session->afp_session.temp.fid = fork;
							session->afp_session.temp.length = length;
						}
					}
					break;

				case AFP_FPReadExt:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						if (!afp_peek_stream(0x10 + 20, stream, &ptr, &end, &afp)) {
							return -1;
						}
						afp_fid_t fork = BE2(afp + 2);
						unsigned long long offset = BE8(afp + 4);
						unsigned long long length = BE8(afp + 12);
						std::map<afp_fid_t, afp_fork_data_t>::const_iterator it = session->afp_session.forkmap.find(fork);
						if (it != session->afp_session.forkmap.end()) {
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = it->second.name;
							session->afp_session.temp.fid = fork;
							session->afp_session.temp.length = length;
						}
					}
					break;

				case AFP_FPWrite:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						if (!afp_peek_stream(0x10 + 12, stream, &ptr, &end, &afp)) {
							return -1;
						}
						afp_fid_t fork = BE2(afp + 2);
						unsigned long offset = BE4(afp + 4);
						unsigned long length = BE4(afp + 8);
						std::map<afp_fid_t, afp_fork_data_t>::const_iterator it = session->afp_session.forkmap.find(fork);
						if (it != session->afp_session.forkmap.end()) {
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = it->second.name;
							session->afp_session.temp.fid = fork;
							session->afp_session.temp.length = length;
						}
					}
					break;

				case AFP_FPWriteExt:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						if (!afp_peek_stream(0x10 + 20, stream, &ptr, &end, &afp)) {
							return -1;
						}
						afp_fid_t fork = BE2(afp + 2);
						unsigned long long offset = BE8(afp + 4);
						unsigned long long length = BE8(afp + 12);
						std::map<afp_fid_t, afp_fork_data_t>::const_iterator it = session->afp_session.forkmap.find(fork);
						if (it != session->afp_session.forkmap.end()) {
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = it->second.name;
							session->afp_session.temp.fid = fork;
							session->afp_session.temp.length = length;
						}
					}
					break;

				case AFP_FPRename:
					{
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *afp;
						afp_peek_stream(stream, &ptr, &end, &afp);

						if (afp + 8 > end) {
							return 0;
						}
						unsigned short vid = BE2(afp + 2);
						unsigned long did = BE4(afp + 4);
						ustring oldname;
						ustring newname;
						unsigned char const *p = afp + 8;
						if (get_path_string(p, end, &oldname, &p)) {
							if (get_path_string(p, end, &newname, &p)) {
								//
							}
							session->afp_session.temp.request_id = dsi.request_id;
							session->afp_session.temp.command = afp_command;
							session->afp_session.temp.name1 = session->afp_session.make_path(sid, vid, did, oldname);
							session->afp_session.temp.name2 = session->afp_session.make_path(sid, vid, did, newname);
						}
					}
					break;

				default:
					break;
				}
			} else {
				return -1;
			}

		} else { // !direction

			if (dsi.flags == 1) { // response

				signed long dsi_error_code;
				{
					unsigned char const *p = stream->peek(8);
					dsi_error_code = BE4(p + 4);
				}

				if (dsi.request_id == session->afp_session.temp.request_id) {

					switch (session->afp_session.temp.command) {

					case 0:
						return -1;

					case AFP_FPLogin:
					case AFP_LoginCont:
					case AFP_FPLoginExt:
						session->afp_session.login_name = session->afp_session.temp.name1;
						if (dsi_error_code == -5001) { // -5001: logincont
								//
						} else {
							if (dsi_error_code == 0) {
								{
									AutoLock lock(get_processor()->mutex());
									{
										file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
										ei->user_name = session->afp_session.login_name;
										get_processor()->on_login(ei);
									}
								}
							} else if (dsi_error_code < 0) {
								//case -5019: // parameter error
								//case -5023: // user not authenticated
								{
									AutoLock lock(get_processor()->mutex());
									{
										file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
										ei->user_name = session->afp_session.login_name;
										get_processor()->on_login_failed(ei);
									}
								}
								session->afp_session.login_name.clear();
							}
							session->afp_session.temp.clear();
						}
						break;

					case AFP_FPOpenVol:
						if (dsi_error_code == 0) {
							unsigned char const *ptr;
							unsigned char const *end;
							unsigned char const *afp;
							afp_peek_stream(stream, &ptr, &end, &afp);

							if (afp + 4 > end) {
								return 0;
							}
							unsigned short bitmap = BE2(afp + 0);
							if (bitmap & 0x0020) {
								afp_vid_t vid = BE2(afp + 2);
								std::map<afp_vid_t, afp_volume_info_t>::iterator it = session->afp_session.volumemap.find(vid);
								if (it == session->afp_session.volumemap.end()) {
									it = session->afp_session.volumemap.insert(session->afp_session.volumemap.end(), std::pair<afp_vid_t, afp_volume_info_t>(vid, afp_volume_info_t()));
								}
								it->second.name = session->afp_session.temp.name1;
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPOpenFork:
						if (dsi_error_code == 0) {
							unsigned char const *ptr;
							unsigned char const *end;
							unsigned char const *afp;
							afp_peek_stream(stream, &ptr, &end, &afp);

							if (afp + 4 > end) {
								return 0;
							}
							afp_fid_t fork = BE2(afp + 2);
							afp_fork_data_t f;
							f.name = session->afp_session.temp.name1;
							session->afp_session.forkmap[fork] = f;
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPCloseFork:
						if (dsi_error_code == 0) {
							afp_fid_t fork = session->afp_session.temp.fid;
							std::map<afp_fid_t, afp_fork_data_t>::iterator it = session->afp_session.forkmap.find(fork);
							if (it != session->afp_session.forkmap.end()) {
								if (it->second.readed > 0) {
									{
										AutoLock lock(get_processor()->mutex());
										{
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
											ei->user_name = session->afp_session.login_name;
											ei->text1 = it->second.name;
											ei->size = it->second.readed;
											get_processor()->on_read(ei);
										}
									}
								}
								if (it->second.written > 0) {
									{
										AutoLock lock(get_processor()->mutex());
										{
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
											ei->user_name = session->afp_session.login_name;
											ei->text1 = it->second.name;
											ei->size = it->second.written;
											get_processor()->on_write(ei);
										}
									}
								}
								session->afp_session.forkmap.erase(it);
							}
						}
						break;

					case AFP_FPCopyFile:
						if (dsi_error_code == 0) {
							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.temp.name1;
									ei->text2 = session->afp_session.temp.name2;
									get_processor()->on_copy(ei);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPCreateDir:
						if (dsi_error_code == 0) {
							unsigned char const *ptr;
							unsigned char const *end;
							unsigned char const *afp;
							if (!afp_peek_stream(0x10 + 4, stream, &ptr, &end, &afp)) {
								return 0;
							}

							// 新しいIDを登録
							afp_did_t id = BE4(afp);
							session->afp_session.insert_dir_entry(session->afp_session.temp.vid, session->afp_session.temp.did, id, session->afp_session.temp.name1, true, _packet_number);

							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.make_path(sid, session->afp_session.temp.vid, session->afp_session.temp.did, session->afp_session.temp.name1);
									get_processor()->on_create_dir(ei);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPCreateFile:
						if (dsi_error_code == 0) {
							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.make_path(sid, session->afp_session.temp.vid, session->afp_session.temp.did, session->afp_session.temp.name1);
									get_processor()->on_create_file(ei);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPDelete:
						if (dsi_error_code == 0) {
							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.make_path(sid, session->afp_session.temp.vid, session->afp_session.temp.did, session->afp_session.temp.name1);
									if (session->afp_session.temp.isdir) {
										get_processor()->on_delete_dir(ei);
									} else {
										get_processor()->on_delete_file(ei);
									}
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPMoveAndRename:
						if (dsi_error_code == 0) {
							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.temp.name1;
									ei->text2 = session->afp_session.temp.name2;
									get_processor()->on_rename(ei);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPExchangeFiles:
						if (dsi_error_code == 0) {
							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.temp.name1;
									ei->text2 = session->afp_session.temp.name2;
									get_processor()->on_exchange(ei);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPGetFileDirParams:
						if (dsi_error_code == 0) {
							unsigned char const *ptr;
							unsigned char const *end;
							unsigned char const *afp;
							afp_peek_stream(stream, &ptr, &end, &afp);

							if (afp + 6 > end) {
								return 0;
							}
							unsigned long did = 0; // directory id
							unsigned long fid = 0; // file id
							bool isdir = (afp[4] & 0x80) != 0;
							unsigned char const *p;
							p = afp + 6;

							if (isdir) {

								unsigned short dirbitmap = BE2(afp + 2);
								if (dirbitmap & 0x0001) { // directory attributes
									p += 2;
								}
								if (dirbitmap & 0x0002) { // directory id
									if (p + 4 > end) {
										return 0;
									}
									did = BE4(p);
									p += 4;
								}
								if (dirbitmap & 0x0004) { // creation date
									p += 4;
								}
								if (dirbitmap & 0x0008) { // modification date
									p += 4;
								}
								if (dirbitmap & 0x0010) { // backup date
									p += 4;
								}
								if (dirbitmap & 0x0020) { // finder info
									p += 0x20;
								}
								unsigned short longnameoffset = 0;
								if (dirbitmap & 0x0040) { // long name
									if (p + 2 > end) {
										return 0;
									}
									longnameoffset = BE2(p);
									p += 2;
								}
								if (dirbitmap & 0x0080) { // short name
									break; // not supported
								}
								if (dirbitmap & 0x0100) { // file id
									if (p + 4 > end) {
										return 0;
									}
									fid = BE4(p);
									p += 4;
								}
								if (dirbitmap & 0x0200) { // offspring
									p += 2;
								}
								if (dirbitmap & 0x0400) { // owner id
									p += 4;
								}
								if (dirbitmap & 0x0800) { // group id
									p += 4;
								}
								if (dirbitmap & 0x1000) { // access rights
									p += 4;
								}
								if (dirbitmap & 0x2000) { // utf-8 name
									if (p + 2 > end) {
										return 0;
									}
									unsigned short offset = BE2(p);
									p = afp + 6 + offset;
									if (p + 6 > end) {
										return 0;
									}

									unsigned long unicodehint = BE4(p);
									p += 4;

									int l = BE2(p);
									p += 2;

									if (p + l > end) {
										return 0;
									}

									ustring name = get_mac_utf8_string((char const *)p, l);
									session->afp_session.insert_dir_entry(session->afp_session.temp.vid, session->afp_session.temp.did, fid, name, true, _packet_number);
								} else if (dirbitmap & 0x0040) { // long name
									p = afp + 6 + longnameoffset;
									if (p + 1 > end) {
										return 0;
									}
									int l = p[0];
									if (p + 1 + l > end) {
										return 0;
									}
									ustring name = sjis_string_to_ustring((char const *)p + 1, l);
									session->afp_session.insert_dir_entry(session->afp_session.temp.vid, did, session->afp_session.temp.did, name, true, _packet_number);
								} else {
									session->afp_session.insert_dir_entry(session->afp_session.temp.vid, session->afp_session.temp.did, fid, session->afp_session.temp.name1, true, _packet_number);
								}

							} else { // !isdir

								unsigned short filebitmap = BE2(afp + 0);
								if (filebitmap & 0x0001) { // directory attributes
									p += 2;
								}
								if (filebitmap & 0x0002) { // directory id
									if (p + 4 > end) {
										return 0;
									}
									did = BE4(p);
									p += 4;
								}
								if (filebitmap & 0x0004) { // creation date
									p += 4;
								}
								if (filebitmap & 0x0008) { // modification date
									p += 4;
								}
								if (filebitmap & 0x0010) { // backup date
									p += 4;
								}
								if (filebitmap & 0x0020) { // finder info
									p += 0x20;
								}
								unsigned short longnameoffset = 0;
								if (filebitmap & 0x0040) { // long name
									if (p + 2 > end) {
										return 0;
									}
									longnameoffset = BE2(p);
									p += 2;
								}
								if (filebitmap & 0x0080) { // short name
									break; // not supported
								}
								if (filebitmap & 0x0100) { // file id
									if (p + 4 > end) {
										return 0;
									}
									fid = BE4(p);
									p += 4;
								}
								if (filebitmap & 0x0200) { // data fork size
									p += 4; // ?
								}
								if (filebitmap & 0x0400) { // resource fork size
									p += 8; // ?
								}
								if (filebitmap & 0x0800) { // extended data fork size
									p += 8;
								}
								if (filebitmap & 0x1000) { // launch limit
									_NOP
								}
								if (filebitmap & 0x2000) { // utf-8 name
									if (p + 2 > end) {
										return 0;
									}
									unsigned short offset = BE2(p);
									p = afp + 6 + offset;
									if (p + 6 > end) {
										return 0;
									}

									unsigned long unicodehint = BE4(p);
									p += 4;

									int l = BE2(p);
									p += 2;

									if (p + l > end) {
										return 0;
									}

									ustring name = get_mac_utf8_string((char const *)p, l);
									session->afp_session.insert_dir_entry(session->afp_session.temp.vid, session->afp_session.temp.did, fid, name, false, _packet_number);
								} else if (filebitmap & 0x0040) { // long name
									p = afp + 6 + longnameoffset;
									if (p + 1 > end) {
										return 0;
									}
									int l = p[0];
									if (p + 1 + l > end) {
										return 0;
									}
									ustring name = sjis_string_to_ustring((char const *)p + 1, l);
									session->afp_session.insert_dir_entry(session->afp_session.temp.vid, did, session->afp_session.temp.did, name, false, _packet_number);
								} else {
									session->afp_session.insert_dir_entry(session->afp_session.temp.vid, session->afp_session.temp.did, fid, session->afp_session.temp.name1, false, _packet_number);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPRead:
					case AFP_FPReadExt:
						if (dsi_error_code == 0) {
							afp_fork_data_t *f = session->afp_session.find_fork_data(session->afp_session.temp.fid);
							if (f) {
								f->readed += session->afp_session.temp.length;
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPWrite:
					case AFP_FPWriteExt:
						if (dsi_error_code == 0) {
							afp_fork_data_t *f = session->afp_session.find_fork_data(session->afp_session.temp.fid);
							if (f) {
								f->written += session->afp_session.temp.length;
							}
						}
						session->afp_session.temp.clear();
						break;

					case AFP_FPRename:
						if (dsi_error_code == 0) {
							{
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_AFP, _packet_number);
									ei->user_name = session->afp_session.login_name;
									ei->text1 = session->afp_session.temp.name1;
									ei->text2 = session->afp_session.temp.name2;
									get_processor()->on_rename(ei);
								}
							}
						}
						session->afp_session.temp.clear();
						break;

					//case :
					default:
						_NOP
						break;
					}
				}
			} else {
				_NOP
				return -1;
			}
		}

		if (dsi.length == 0) {
			return -1;
		}

		return 0x10 + dsi.length;
	}

	int PacketAnalyzerThread::on_dsi_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream)
	{
		while (!stream->empty()) {
			int n = on_a_dsi_packet(sid, data, direction, stream);
			if (n == -1) {
				return -1;
			}
			if (n == 0) {
				break;
			}
			stream->consume(n);
		}
		return 0;
	}

} // namespace ContentsWatcher


