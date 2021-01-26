// PacketAnalyzerThreadのSMB解析処理

#include "cw_analizer.h"
#include "common/text.h"
#include "common/combinepath.h"
#include "common/eq.h"
#include "cifs.h"


int gnKRB5_Seq_Number = 0;		// global seq number for KRB5

static inline ustring sjis_string_to_ustring(char const *ptr, size_t len)
{
    return soramimi::jcode::convert(soramimi::jcode::SJIS, ptr, len);
}





namespace ContentsWatcher {
	
	void replace_backslash_to_slash(ustring *str)
	{
		if (!ucs::wcschr(str->c_str(), '\\')) {
			return;
		}
		size_t n = str->size();
		std::vector<uchar_t> vec(n);
		for (size_t i = 0; i < n; i++) {
			uchar_t c = str->at(i);
			if (c == '\\') {
				c = '/';
			}
			vec[i] = c;
		}
		str->assign(&vec[0], n);
	}

	// (added by kitamura)
	static inline bool my_ismbblead(int c)
	{
		return (c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xfc);
	}

	static ustring smb_get_string(unsigned char const *ptr, unsigned char const *end, unsigned short flags2 = 0x8000)
	{
		if (flags2 & 0x8000) { // unicode
			ustringbuffer sb;
			while (ptr + 1 < end) {
				uchar_t c = LE2(ptr);
				if (c == 0) {
					break;
				}
				sb.put(LE2(ptr));
				ptr += 2;
			}
			return sb.str();
		} else {
			ustringbuffer sb;
			while (ptr < end) {
				uchar_t c = *ptr;
				if (c == 0) {
					break;
				}


				// (added by kitamura)
				if ((ptr+1 < end) && (my_ismbblead(c))) {
					c = (c << 8) | ptr[1];
					c = soramimi::jcode::jistoucs(soramimi::jcode::jmstojis(c));
					ptr++;
				}


				sb.put(c);
				ptr += 1;
			}
			return sb.str();
		}
	}

	static inline ustring smb_get_string(unsigned char const *ptr, size_t len, unsigned short flags2 = 0x8000)
	{
		return smb_get_string(ptr, ptr + len * 2, flags2);
	}

	// smb

	inline smb_file_state_map_t *session_data_t::smb_session_t::get_file_state_map(smb_uid_t uid)
	{
		smb_user_data_map_t::iterator it = user_data_map.find(uid);
		if (it == user_data_map.end()) {
			return 0;
		}
		return &it->second->file_state_map;
	}

	inline std::map<smb_tid_t, smb_tree_t> *session_data_t::smb_session_t::get_tree_map(smb_uid_t uid, unsigned short tid)
	{
		smb_user_data_map_t::iterator it1 = user_data_map.find(uid);
		if (it1 == user_data_map.end()) {
			return 0;
		}
		return &it1->second->tree_map;
	}

	inline smb_file_state_t *session_data_t::smb_session_t::find_file_state_map(smb_uid_t uid, smb_fid_t const &fid)
	{
		smb_file_state_map_t *map = get_file_state_map(uid);
		if (map) {
			smb_file_state_map_t::iterator it = map->find(fid);
			if (it != map->end()) {
				return &it->second;
			}
		}
		return 0;
	}

	inline smb_tree_t const *session_data_t::smb_session_t::get_tree(smb_uid_t uid, unsigned short tid)
	{
		std::map<smb_tid_t, smb_tree_t> *map = get_tree_map(uid, tid);
		if (map) {
			std::map<unsigned short, smb_tree_t>::const_iterator it = map->find(tid);
			if (it != map->end()) {
				return &it->second;
			}
		}
		return 0;
	}

	ustring session_data_t::smb_session_t::make_path_(smb_tree_t const *tree, ustring const &path)
	{
		if (!tree) {
			return path;
		}
		uchar_t const *p = path.c_str();
		if (*p == '\\' || *p == '/') {
			p++;
		}
		return CombinePath(tree->name.c_str(), p, '/');
	}

	ustring session_data_t::smb_session_t::make_path(smb_tree_t const *tree, ustring const &path)
	{
		ustring newpath = make_path_(tree, path);
		replace_backslash_to_slash(&newpath);
		return newpath;
	}

	ustring session_data_t::smb_session_t::make_path(smb_uid_t uid, unsigned short tid, ustring const &path, bool *ipc)
	{
		ustring newpath;
		*ipc = false;
		smb_tree_t const *tree = get_tree(uid, tid);
		if (tree) {
			if (tree->name.size() >= 5) {
				if (eq(tree->name.c_str() + tree->name.size() - 5, (uchar_t const *)L"\\IPC$")) {
					*ipc = true;
				}
			}
			newpath = make_path_(tree, path);
		} else {
			char tmp[100];
			sprintf(tmp, "<TID:%u>", tid);
			newpath = string_to_ustring(tmp);
			newpath += path;
		}
		replace_backslash_to_slash(&newpath);
		return newpath;
	}

	// smb2

	inline smb_tree_t const *session_data_t::smb2_session_t::get_tree(smb2_tid_t tid)
	{
		std::map<smb2_tid_t, smb_tree_t>::const_iterator it = tree_map.find(tid);
		return it == tree_map.end() ? 0 : &it->second;
	}

	ustring session_data_t::smb2_session_t::make_path_(smb_tree_t const *tree, ustring const &path)
	{
		if (!tree) {
			return path;
		}
		uchar_t const *p = path.c_str();
		if (*p == '\\' || *p == '/') {
			p++;
		}
		return CombinePath(tree->name.c_str(), p, '/');
	}

	ustring session_data_t::smb2_session_t::make_path(smb_tree_t const *tree, ustring const &path)
	{
		ustring newpath = make_path_(tree, path);
		replace_backslash_to_slash(&newpath);
		return newpath;
	}

	ustring session_data_t::smb2_session_t::make_path(smb2_tid_t tid, ustring const &path, bool *ipc)
	{
		ustring newpath;
		*ipc = false;
		smb_tree_t const *tree = get_tree(tid);
		if (tree) {
			if (tree->name.size() >= 5) {
				if (eq(tree->name.c_str() + tree->name.size() - 5, (uchar_t const *)L"\\IPC$")) {
					*ipc = true;
				}
			}
			newpath = make_path_(tree, path);
		} else {
			char tmp[100];
			sprintf(tmp, "<TID:%u>", tid);
			newpath = string_to_ustring(tmp);
			newpath += path;
		}
		replace_backslash_to_slash(&newpath);
		return newpath;
	}

	//

	enum {
		SMB_OP_CREATE_DIRECTORY          = 0x00, // フォルダの作成
		SMB_OP_DELETE_DIRECTORY          = 0x01, // フォルダの削除
		SMB_OP_OPEN                      = 0x02, // ファイルのオープン
		SMB_OP_CREATE                    = 0x03, // ファイルのオープン／作成
		SMB_OP_CLOSE                     = 0x04, // ファイルのクローズ
		SMB_OP_FLUSH                     = 0x05, // ファイルのフラッシュ
		SMB_OP_DELETE                    = 0x06, // ファイルの削除
		SMB_OP_RENAME                    = 0x07, // ファイルの名前変更
		SMB_OP_QUERY_INFORMATION         = 0x08, // ファイル情報の問い合わせ
		SMB_OP_SET_INFORMATION           = 0x09, // ファイル情報の設定
		SMB_OP_READ                      = 0x0a, // ファイルの読み出し
		SMB_OP_WRITE                     = 0x0b, // ファイルの書き込み
		SMB_OP_LOCK_BYTE_RANGE           = 0x0c, // バイト範囲のロック
		SMB_OP_UNLOCK_BYTE_RANGE         = 0x0d, // バイト範囲のアンロック
		SMB_OP_CREATE_TEMPORARY          = 0x0e, // 一時ファイルの作成
		SMB_OP_CREATE_NEW                = 0x0f, // 新規ファイルの作成
		SMB_OP_CHECK_DIRECTORY           = 0x10, // フォルダの検査
		SMB_OP_PROCESS_EXIT              = 0x11, // 終了
		SMB_OP_SEEK                      = 0x12, // シーク
		SMB_OP_LOCK_AND_READ             = 0x13, // ロックと読み出し
		SMB_OP_WRITE_AND_UNLOCK          = 0x14, // 書き込みとアンロック
		SMB_OP_READ_RAW                  = 0x1a, // ロー（直の）読み出し
		SMB_OP_READ_MPX                  = 0x1b, // マルチプレックス（多重化）読み出し
		SMB_OP_READ_MPX_SECONDARY        = 0x1c, // マルチプレックス（多重化）読み出し（継続）
		SMB_OP_WRITE_RAW                 = 0x1d, // ロー書き込み
		SMB_OP_WRITE_MPX                 = 0x1e, // マルチプレックス（多重化）書き込み
		SMB_OP_WRITE_COMPLETE            = 0x20, // 書き込み完了
		SMB_OP_SET_INFORMATION2          = 0x22, // 情報（属性）の設定
		SMB_OP_QUERY_INFORMATION2        = 0x23, // 情報（属性）の取得
		SMB_OP_LOCKING_ANDX              = 0x24, // ロック（＋後続コマンド）
		SMB_OP_TRANSACTION               = 0x25, // トランザクション
		SMB_OP_TRANSACTION_SECONDARY     = 0x26, // トランザクション（継続）
		SMB_OP_IOCTL                     = 0x27, // IOCTL
		SMB_OP_IOCTL_SECONDARY           = 0x28, // IOCTL（継続）
		SMB_OP_COPY                      = 0x29, // コピー
		SMB_OP_MOVE                      = 0x2a, // 移動
		SMB_OP_ECHO                      = 0x2b, // エコー
		SMB_OP_WRITE_AND_CLOSE           = 0x2c, // 書き込みとクローズ
		SMB_OP_OPEN_ANDX                 = 0x2d, // オープン（＋後続コマンド）
		SMB_OP_READ_ANDX                 = 0x2e, // 読み出し（＋後続コマンド）
		SMB_OP_WRITE_ANDX                = 0x2f, // 書き込み（＋後続コマンド）
		SMB_OP_CLOSE_AND_TREE_DISC       = 0x31, // クローズとツリー接続の解除
		SMB_OP_TRANSACTION2              = 0x32, // トランザクション2
		SMB_OP_TRANSACTION2_SECONDARY    = 0x33, // トランザクション2（継続）
		SMB_OP_FIND_CLOSE2               = 0x34, // クローズ2
		SMB_OP_FIND_NOTIFY_CLOSE         = 0x35, // 通知とクローズ
		SMB_OP_TREE_CONNECT              = 0x70, // ツリー接続
		SMB_OP_TREE_DISCONNECT           = 0x71, // ツリー切断
		SMB_OP_NEGOTIATE                 = 0x72, // プロトコル・バージョンのネゴシエーション
		SMB_OP_SESSION_SETUP_ANDX        = 0x73, // セッション・セットアップ（＋後続コマンド）
		SMB_OP_LOGOFF_ANDX               = 0x74, // ログオフ（＋後続コマンド）
		SMB_OP_TREE_CONNECT_ANDX         = 0x75, // ツリー接続（＋後続コマンド）
		SMB_OP_QUERY_INFORMATION_DISK    = 0x80, // ディスク情報の取得
		SMB_OP_SEARCH                    = 0x81, // 検索
		SMB_OP_FIND                      = 0x82, // 検索
		SMB_OP_FIND_UNIQUE               = 0x83, // ユニーク検索
		SMB_OP_NT_TRANSACT               = 0xa0, // トランザクション
		SMB_OP_NT_TRANSACT_SECONDARY     = 0xa1, // トランザクション（継続）
		SMB_OP_NT_CREATE_ANDX            = 0xa2, // （＋後続コマンド）
		SMB_OP_NT_CANCEL                 = 0xa4, // キャンセル
		SMB_OP_NT_RENAME                 = 0xa5, // 名前変更
		SMB_OP_OPEN_PRINT_FILE           = 0xc0, // スプール・ファイルのオープン
		SMB_OP_WRITE_PRINT_FILE          = 0xc1, // スプール・ファイルへの書き込み
		SMB_OP_CLOSE_PRINT_FILE          = 0xc2, // スプール・ファイルのクローズ
		SMB_OP_GET_PRINT_QUEUE           = 0xc3, // スプール・ファイル情報の取得
	};

	enum {
		SMB2_OP_NEGPROT = 0x00,
		SMB2_OP_SESSSETUP = 0x01,
		SMB2_OP_LOGOFF = 0x02,
		SMB2_OP_TCON = 0x03,
		SMB2_OP_TDIS = 0x04,
		SMB2_OP_CREATE = 0x05,
		SMB2_OP_CLOSE = 0x06,
		SMB2_OP_FLUSH = 0x07,
		SMB2_OP_READ = 0x08,
		SMB2_OP_WRITE = 0x09,
		SMB2_OP_LOCK = 0x0a,
		SMB2_OP_IOCTL = 0x0b,
		SMB2_OP_CANCEL = 0x0c,
		SMB2_OP_KEELALIVE = 0x0d,
		SMB2_OP_FIND = 0x0e,
		SMB2_OP_NOTIFY = 0x0f,
		SMB2_OP_GETINFO = 0x10,
		SMB2_OP_SETINFO = 0x11,
		SMB2_OP_BREAK = 0x12,
	};

	enum {
		SMB2_FILE_BASIC_INFO = 0x04,
		SMB2_FILE_RENAME_INFO = 0x0a,
		SMB2_FILE_DISPOSITION_INFO = 0x0d,
		SMB2_FILE_POSITION_INFO = 0x0e,
		SMB2_FILE_MODE_INFO = 0x10,
		SMB2_FILE_ALLOCATION_INFO = 0x13,
		SMB2_FILE_ENDOFFILE_INFO = 0x14,
		SMB2_FILE_PIPE_INFO = 0x17,
	};

	namespace smb {
		//namespace session_setup_andx_req {
		//	enum {
		//		WordCount           =    0, //            1
		//		AndXCommand         =    1, //            1
		//		AndXReserved        =    2, //            1
		//		AndXOffset          =    3, //            2
		//		MaxBufferSize       =    5, //            2
		//		MaxMpxCount         =    7, //            2
		//		VcNumber            =    9, //            2
		//		SessionKey          =   11, //            4
		//		PasswordLength      =   15, //            2
		//		Reserved            =   17, //            4
		//		Capabilities        =   21, //            4
		//		ByteCount           =   25, //            2
		//		//AccountPassword[]       *            Variable
		//		//AccountName[]           *            Variable
		//		//PrimaryDomain[]         *            Variable
		//		//NativeOS[]              *            Variable
		//		//NativeLANMan[]          *            Variable
		//	};
		//}
		namespace nt_create_andx_req {
			enum {
				WordCount           =  0, //              1
				AndXCommand         =  1, //              1
				AndXReserved        =  2, //              1
				AndXOffset          =  3, //              2
				Reserved            =  5, //              1
				NameLength          =  6, //              2
				Flags               =  8, //              4
				RootDirectory       = 12, //              4
				DesiredAccess       = 16, //              4
				AllocationSiz       = 20, //              8
				FileAttribute       = 28, //              4
				ShareAccess         = 32, //              4
				CreateDisposi       = 36, //              4
				CreateOptions       = 40, //              4
				Impersonation       = 44, //              4
				SecurityFlags       = 48, //              1
				ByteCount           = 49, //              2
				// reserved?        = 51, //              1
				Filename            = 52, //           Variable		(adjust by kitamura) <-NG, and back to '52'
			};
		}

		namespace nt_create_andx_res {
			enum {
				WordCount           =  0, //              1
				AndXCommand         =  1, //              1
				AndXReserved        =  2, //              1
				AndXOffset          =  3, //              2
				OpLockLevel         =  5, //              1
				Fid                 =  6, //              2
				CreateAction        =  8, //              4
				CreationTime        = 12, //              8
				LastAccessTim       = 20, //              8
				LastWriteTime       = 28, //              8
				ChangeTime          = 36, //              8
				FileAttribute       = 44, //              4
				AllocationSiz       = 48, //              8
				EndOfFile           = 56, //              8
				FileType            = 64, //              2
				DeviceState         = 66, //              2
				Directory           = 68, //              1
				ByteCount           = 69, //              2
				Buffer              = 71, //           Variable
			};
		}

		namespace read_adnx_req {
			enum {
				WordCount           =  0, //              1
				ANDXCommand         =  1, //              1
				ANDXReserved        =  2, //              1
				ANDXOffset          =  3, //              2
				Fid                 =  5, //              2
				Offset              =  7, //              4
				MaxReturnCount      = 11, //              2
				MinCount            = 13, //              2
				MaxReturnCountHig   = 15, //              4
				Remaining           = 19, //              2
				OffsetHigh          = 21, //              4
				ByteCount           = 25, //              2
			};
		}

		namespace read_adnx_res {
			enum {
				WordCount           =  0, //              1
				ANDXCommand         =  1, //              1
				ANDXReserved        =  2, //              1
				ANDXOffset          =  3, //              2
				Remaining           =  5, //              2
				DataCompactionMod   =  7, //              2
				//Reserved            =  9, //              2
				DataLength          = 11, //              2
				DataOffset          = 13, //              2
				DataLengthHigh      = 15, //              4
				//Reserved            = 19, //              6
				ByteCount           = 25, //              2
				//Pad                 = 25, //           Variable
				//Data[DataLength]    = * , //           Variable
			};
		}

		namespace write_adnx_req {
			enum {
				WordCount           =  0, //              1
				ANDXCommand         =  1, //              1
				ANDXReserved        =  2, //              1
				ANDXOffset          =  3, //              2
				Fid                 =  5, //              2
				Offset              =  7, //              4
				Reserved            = 11, //              4
				WriteMode           = 15, //              2
				CountBytesRemai     = 17, //              2
				DataLengthHigh      = 19, //              2
				DataLength          = 21, //              2
				DataOffset          = 23, //              2
				OffsetHigh          = 25, //              4
				ByteCount           = 29, //              2
				Pad                 = 31, //          Variable
				//Data[DataLengt]     *   //          Variable
			};
		}

		namespace write_adnx_res {
			enum {
				WordCount          =  0, //               1
				ANDXCommand        =  1, //               1
				ANDXReserved       =  2, //               1
				ANDXOffset         =  3, //               2
				Count              =  5, //               2
				Remaining          =  7, //               2
				CountHigh          =  9, //               2
				Reserved           = 11, //               2
				ByteCount          = 13, //               2
			};
		}

		namespace delete_req {
			enum {
				WordCount           = 0, //               1
				SearchAttribut      = 1, //               2
				ByteCount           = 3, //               2
				BufferFormat        = 5, //               1
				FileName            = 6, //            Variable
			};
		}
		namespace delete_directory_req {
			enum {
				WordCount           = 0, //               1
				ByteCount           = 1, //               2
				BufferFormat        = 3, //               1
				DirectoryName       = 4, //            Variable
			};
		}
		namespace rename_req {
			enum {
				WordCount           = 0, //               1
				SearchAttributes    = 1, //               2
				ByteCount           = 3, //               2
				BufferFormat1       = 5, //               1
				OldFileName         = 6, //            Variable
				//BufferFormat2     = *, //               1
				//NewFileName       = *, //            Variable
			};
		}
		namespace tree_connect_andx {
			enum {
               WordCount            = 0, //               1
               AndXCommand          = 1, //               1
               AndXReserved         = 2, //               1
               AndXOffset           = 3, //               2
               Flags                = 5, //               2
               PasswordLength       = 7, //               2
               ByteCount            = 9, //               2
               Password             = 11, //           Variable
               //Path[]               *         Variable
               //Service[]            *         Variable
			};
		}
	}

	// smb

	int smb_request_id_t::compare(smb_request_id_t const &r) const
	{
		if (pid < r.pid) {
			return -1;
		}
		if (pid > r.pid) {
			return 1;
		}
		//if (uid < r.uid) {
		//	return -1;
		//}
		//if (uid > r.uid) {
		//	return 1;
		//}
		if (mid < r.mid) {
			return -1;
		}
		if (mid > r.mid) {
			return 1;
		}
		return 0;
	}

	int smb2_request_id_t::compare(smb2_request_id_t const &r) const
	{
		if (pid < r.pid) {
			return -1;
		}
		if (pid > r.pid) {
			return 1;
		}
		if (sid < r.sid) {
			return -1;
		}
		if (sid > r.sid) {
			return 1;
		}
		if (seq < r.seq) {
			return -1;
		}
		if (seq > r.seq) {
			return 1;
		}
		return 0;
	}

	smb_file_flags_t::smb_file_flags_t()
	{
		opened = false;
		delete_on_close = false;
	}

	int PacketAnalyzerThread::on_udp138_packet(packet_capture_data_t *data, bool direction, blob_t *stream)
	{
		unsigned char const *ptr = stream->c_str();
		unsigned char const *end = ptr + stream->size();

		unsigned char const *srcptr = ptr + 0x0e;
		if (srcptr > end) {
			return 0;
		}
		int srclen = *srcptr++;
		if (srcptr + srclen > end) {
			return 0;
		}
		unsigned char const *dstptr = srcptr + srclen + 1;
		if (dstptr > end) {
			return 0;
		}
		int dstlen = *dstptr++;
		if (dstptr + dstlen > end) {
			return 0;
		}
		unsigned char const *smb = dstptr + dstlen + 1;
		if (smb + 4 > end) {
			return 0;
		}
		if (memcmp(smb, "\xffSMB", 4) != 0) {
			return -1;
		}

		unsigned char const *ipaddr = ptr + 4;

		std::string srcname = decode_netbios_name(srcptr, srclen);
		std::string dstname = decode_netbios_name(dstptr, dstlen);

		ipv4addr_t addr = (ipaddr[0] << 24) | (ipaddr[1] << 16) | (ipaddr[2] << 8) | ipaddr[3];

		log_printf(data->time_us, "[BROWSE] %s \"%s\"\n"
			, addr.tostring().c_str()
			, srcname.c_str()
			);

		return -1;
	}

	static bool NTSTATUS_IS_SUCCESS(unsigned long s)
	{
		return s == 0;
	}

	void PacketAnalyzerThread::smb_create_action(int smbversion, client_server_t const *sid, packet_capture_data_t *data, ustring const &username, unsigned long action, bool isdirectory, ustring const &path)
	{
		file_access_event_item_t::protocol_t protocol = file_access_event_item_t::P_SMB;
		if (smbversion == 2) {
			protocol = file_access_event_item_t::P_SMB2;
		}

		switch (action) {
		case 1: // The file existed and was opend
		case 0x8001:
			// nothing to do
			break;
		case 2: // The file did not exist but was created
		case 0x8002:
			{
				AutoLock lock(get_processor()->mutex());
				{
					file_access_event_item_t ei(sid, data, protocol, _packet_number);
					ei->user_name = username;
					ei->text1 = path;
					if (isdirectory) {
						get_processor()->on_create_dir(ei);
					} else {
						get_processor()->on_create_file(ei);
					}
				}
			}
			break;
		case 3: // The file existed and was truncated
		case 0x8003:
			{
				AutoLock lock(get_processor()->mutex());
				{
					file_access_event_item_t ei(sid, data, protocol, _packet_number);
					ei->user_name = username;
					ei->text1 = path;
					get_processor()->on_file_access(ei);
				}
			}
			break;
		case 4: // delete file or directory
		case 0x8004:
			{
				AutoLock lock(get_processor()->mutex());
				{
					if (isdirectory) {
						file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
						ei->user_name = username;
						ei->text1 = path;
						get_processor()->on_delete_dir(ei);
					} else {
						file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
						ei->user_name = username;
						ei->text1 = path;
						get_processor()->on_delete_file(ei);
					}
				}
			}
			break;
		}
	}

	void set_smb1_flag(session_data_t *session, bool f)
	{
		if (f) {
			session->protocol_mask |= ProtocolMask::SMB1;
		} else {
			session->protocol_mask &= ~ProtocolMask::SMB1;
		}
	}

	void readstring_u(unsigned char const *ptr, unsigned char const *end, unsigned char const **next_p, ustring *out)
	{
		ustringbuffer sb;
		unsigned char const *p;
		for (p = ptr; p + 1 < end && *(uchar_t const *)p; p += 2) {
			sb.put(LE2(p));
		}
		*out = sb.str();
		*next_p = p + 2;
	}

	void readstring_a(unsigned char const *ptr, unsigned char const *end, unsigned char const **next_p, ustring *out)
	{
		std::string s;
		unsigned char const *p;
		for (p = ptr; p < end && *p; p++);
		s.assign((char const *)ptr, (char const *)p);
		*out = utf8_string_to_ustring(s);
		*next_p = p + 1;
	}

	static inline void smb_peek_stream(blob_t *stream, unsigned char const **ptr, unsigned char const **end, unsigned char const **smb, unsigned char const **smbdata)
	{
		unsigned char const *p = stream->c_str();
		*ptr = p;
		*end = p + stream->size();
		*smb = p + 4;
		*smbdata = p + 36;
	}

	static inline bool smb_peek_stream(size_t len, blob_t *stream, unsigned char const **ptr, unsigned char const **end, unsigned char const **smb, unsigned char const **smbdata)
	{
		assert(len > 36);
		unsigned char const *p = stream->peek(len);
		if (!p) {
			return false;
		}
		*ptr = p;
		*end = p + len;
		*smb = p + 4;
		*smbdata = p + 36;
		return true;
	}

	ustring PacketAnalyzerThread::smb_get_user_name(session_data_t const *session, smb_uid_t uid) const
	{
		smb_user_data_map_t::const_iterator it = session->smb_session.user_data_map.find(uid);
		if (it == session->smb_session.user_data_map.end()) {
			return ustring();
		}
		return it->second->name;
	}

	bool PacketAnalyzerThread::cmd_tree_connect_request(session_data_t *session, smb_request_id_t const &request_id, unsigned char const *smbdata, unsigned char const *end, unsigned short flags2)
	{
		if (smbdata + smb::tree_connect_andx::Password > end) {
			return false;
		}
		unsigned char wordcount = smbdata[smb::tree_connect_andx::WordCount];
		unsigned char andxcommand = smbdata[smb::tree_connect_andx::AndXCommand];
		unsigned char andxreserved = smbdata[smb::tree_connect_andx::AndXReserved];
		unsigned short andxoffset = LE2(smbdata + smb::tree_connect_andx::AndXOffset);
		unsigned short flags = LE2(smbdata + smb::tree_connect_andx::Flags);
		unsigned short passwordlength = LE2(smbdata + smb::tree_connect_andx::PasswordLength);
		unsigned short bytecount = LE2(smbdata + smb::tree_connect_andx::ByteCount);
		unsigned char const *p = smbdata + smb::tree_connect_andx::Password;
		if (passwordlength > bytecount) {
			return false;
		}
		if (p + bytecount > end) {
			return false;
		}


		// if UNICODE then "p" must to adjust 16bit boundary (even-address)
		p += passwordlength;
		if ((flags2 & 0x8000) && ((int)p & 1)) {
			p++;
		}
		ustring path = smb_get_string(p, end, flags2);



		if (path.size() >= 5) {
			if (eq(path.c_str() + path.size() - 5, (uchar_t const *)L"\\IPC$") == 0) {
				_NOP
			}
		}
		session->smb_session.temp_tree_cache[request_id] = path;
		return true;
	}

	void PacketAnalyzerThread::cmd_tree_connect_response(session_data_t *session, smb_request_id_t const &request_id, smb_uid_t user_id, smb_tid_t tree_id)
	{
		std::map<smb_request_id_t, ustring>::const_iterator it = session->smb_session.temp_tree_cache.find(request_id);
		if (it == session->smb_session.temp_tree_cache.end()) {
			return;
		}
		smb_tree_t tree;
		tree.name = it->second;
		session->smb_session.user_data_map[user_id]->tree_map[tree_id] = tree;
#if 0
		log_printf(0, "[SMB] C=%s S=%s tree_connect_andx \"%s\" tid=%u (#%u)"
			, sid->client.addr.tostring().c_str()
			, sid->server.addr.tostring().c_str()
			, utf8_ustring_to_string(it->second).c_str()
			, tree_id
			, _packet_number
			);
#endif
	}

	static unsigned char const *find_ntlmssp(unsigned char const *ptr, unsigned char const *end)
	{
		unsigned char const *p = ptr;
		while (p + 8 <= end) {
			if (memcmp(p, "NTLMSSP\0", 8) == 0) {
				return p;
			}
			p++;
		}
		return 0;
	}



	// (2010/8/16, added by kitamura)
	// find the KRB5 pattern in the Security Blob in SessionSetupAndX Request packet
	static bool is_KRB5_and_CIFS(unsigned char const *ptr, unsigned char const *end)
	{
		bool bKRB5 = false;
		bool bCIFS = false;

		unsigned char const *p;

		/*
		KRB5 OID: 1.2.840.113554.1.2.2 (KRB5 - Kerberos 5)
		krb5_tok_id: KRB5_AP_REQ (0x0001)

		06 09 2a 86 48 86 f7 12 01 02 02 01 00
		=====+==========================+=====
		 BER + KRB5 OID                 + krb5_tok_id

		"\x06\x09\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01\x00"	<-13 bytes
		*/
		p = ptr;
		while (p + 13 <= end) {
			if (memcmp(p, "\x06\x09\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01\x00", 13) == 0) {
				bKRB5 = true;
				break;
			}
			p++;
		}

		/*
		Name: cifs

		1b 04 63 69 66 73
		=====+===========
		 BER + cifs

		"\x1b\x04cifs"  <-6 bytes
		*/
		p = ptr;
		while (p + 6 <= end) {
			if (memcmp(p, "\x1b\x04\x63\x69\x66\x73", 6) == 0) {
				bCIFS = true;
				break;
			}
			p++;
		}

		// return
		if (bKRB5 && bCIFS) {
			return true;
		}
		else {
			return false;
		}
	}

	// (2010/8/16, added by kitamura)
	// generate KRB5 dummy user name for ADWatcher
	static void generateKRB5UserName(microsecond_t time_us, char* pchName){
 
		// generate "Jul  3, 2010 16:21:02" format date string
		time_t tt = (time_us / 1000000);
		struct tm *ptm = localtime(&tt);
		char chTime[32];

		strftime(chTime, sizeof(chTime), "%b %e, %Y %H:%M:%S", ptm);
		// strftime(chTime, sizeof(chTime), "%b %d, %Y %H:%M:%S", ptm);

		// generate KRB5 dummy name
		char chName[32];
		sprintf(chName, "KRB5|%s|%03X", chTime, gnKRB5_Seq_Number);
		strcpy(pchName, chName);

		// increment global "gnKRB5_Seq_Number"
		gnKRB5_Seq_Number++;
		if (gnKRB5_Seq_Number > 0xfff) {	// range (0 <= gnKRB5_Seq_Number >= 0xfff)
			gnKRB5_Seq_Number = 0;
		}
	}



	int PacketAnalyzerThread::on_smb_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream)
	{
if (_packet_number == 9) {
	_NOP
}

		if (!get_option()->ignore_target_servers) {
			if (!(target_protocol_flags & target_protocol::SMB)) {
				return -1; // 監視対象ではない
			}
		}

		session_data_t *session = data->get_session_data();
		set_smb1_flag(session, true);

		unsigned short netbioslength;
		unsigned char smbcommand;
		unsigned long ntstatus;
		unsigned char flags1;
		unsigned short flags2;
		smb_tid_t tree_id;
		smb_uid_t user_id;
		smb_request_id_t request_id;

		{
			unsigned char const *p = stream->peek(36);
			if (!p) {
				return 0;
			}

			netbioslength = BE2(p + 2);

			if (p[0] != 0) { // session message 以外は破棄
				// 0x85: session keep-alive
				_NOP
				return -1;
			}

			if ((size_t)netbioslength + 4 > stream->size()) {
				_NOP
				return 0;
			}

			//p = stream->c_str();
			//unsigned char const *end = p + stream->size();

			// smb header

			unsigned char const *smb = p + 4;

			smbcommand = smb[4];
			ntstatus = LE4(smb + 5);
			flags1 = smb[9];
			flags2 = LE2(smb + 10);

			tree_id = LE2(smb + 24);

			request_id.pid = LE2(smb + 26);
			user_id = LE2(smb + 28);
			request_id.mid = LE2(smb + 30);
		}

		//unsigned char const *const smbdata = smb + 32;

		if (direction) { // client to server

			switch (smbcommand) {

			case SMB_OP_LOGOFF_ANDX:
				{
					ustring username;

					smb_user_data_map_t::iterator it = session->smb_session.user_data_map.find(user_id);
					if (it != session->smb_session.user_data_map.end()) {
						username = it->second->name;
						session->smb_session.user_data_map.erase(it);
					}
					if (session->smb_session.user_data_map.empty()) {
						set_smb1_flag(session, false);
					}

					if (! username.empty()) {
						AutoLock lock(get_processor()->mutex());
						{
							file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
							ei->user_name = username;
							get_processor()->on_logout(ei);
						}
					}
				}
				break;

			case SMB_OP_SESSION_SETUP_ANDX:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					unsigned char wordcount = smbdata[0];
					if (wordcount == 13) {
						unsigned char const *p = smbdata + wordcount * 2 + 1;
						if (p + 2 > end) return -1;
						unsigned char andxcommand = smbdata[1];
						unsigned short andxoffset = LE2(smbdata + 3);

						unsigned short bc = LE2(p); // ByteCount
						if (p + 2 + bc > end) return -1;
						p += 2;
						unsigned short ansipwlen = LE2(smbdata + 0x0f); // ANSI Password Length
						unsigned short unicodepwlen = LE2(smbdata + 0x11); // Unicode Password Length
						p += ansipwlen;
						p += unicodepwlen;

						ustring user_name;
						ustring domain_name;
						ustring nativeos;
						ustring nativelm;
						if (flags2 & 0x8000) { // unicode string
							if ((int)p & 1) { // 奇数なら1進める
								p++;
							}
							readstring_u(p, end, &p, &user_name);
							readstring_u(p, end, &p, &domain_name);
							readstring_u(p, end, &p, &nativeos);
							readstring_u(p, end, &p, &nativelm);
						} else {
							readstring_a(p, end, &p, &user_name);
							readstring_a(p, end, &p, &domain_name);
							readstring_a(p, end, &p, &nativeos);
							readstring_a(p, end, &p, &nativelm);
						}

						session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
						session->smb_session.temp.push_front(temp);
						if (!user_name.empty()) {
							session->smb_session.temp.front().sessionsetup.domain = domain_name;
							session->smb_session.temp.front().sessionsetup.username = user_name;
							session->smb_session.temp.front().sessionsetup.hostname.clear();
						}

						// and x
						if (andxcommand == SMB_OP_TREE_CONNECT_ANDX) {
							if (!cmd_tree_connect_request(session, request_id, smb + andxoffset, end, flags2)) {
								return 0;
							}
						}

					} else if (wordcount == 12) {
						unsigned char const *p = smbdata + wordcount * 2 + 1;
						if (p + 10 > end) return -1;
						unsigned short bc = LE2(p); // ByteCount
						unsigned char const *ntlmssp = find_ntlmssp(p + 2, end);
						if (!ntlmssp) {

							// test KRB5 AP-REQ to cifs server
							if (! is_KRB5_and_CIFS(p+2, end)) {
								return -1;
							}

							// make dummy user name for ADWatcher
							char user_name[32];

							generateKRB5UserName(data->time_us, user_name);

							// insert session_data_t::smb_session_t::tmp_t record to the temp
							session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
							session->smb_session.temp.push_front(temp);
							session->smb_session.temp.front().sessionsetup.domain.clear();
							session->smb_session.temp.front().sessionsetup.username = sjis_string_to_ustring(user_name, strlen(user_name));
							session->smb_session.temp.front().sessionsetup.hostname.clear();

							break;	// exit

						}



						if (ntlmssp + 12 > end) return -1;
						unsigned long msgtype = LE4(ntlmssp + 8);
enum NTLM_MESSAGE_TYPE {
	NTLMSSP_INITIAL = 0,
	NTLMSSP_NEGOTIATE = 1,
	NTLMSSP_CHALLENGE = 2,
	NTLMSSP_AUTH = 3,
	NTLMSSP_UNKNOWN = 4,
	NTLMSSP_DONE = 5,
};
						if (msgtype == NTLMSSP_AUTH) {
							if (ntlmssp + 0x58 > end) return -1;

							size_t l;

							unsigned char const *lanmanres = ntlmssp + 0x0c;
							if (ntlmssp + LE4(lanmanres + 4) + LE2(lanmanres) > end) {
								return -1;
							}

							unsigned char const *ntlmres = lanmanres + 8;
							if (ntlmssp + LE4(ntlmres + 4) + LE2(ntlmres) > end) {
								return -1;
							}

							unsigned char const *domname = ntlmres + 8;
							p = ntlmssp + LE4(domname + 4);
							l = LE2(domname);
							if (p + l > end) {
								return -1;
							}
							ustring domain_name = smb_get_string(p, l / 2, flags2);

							unsigned char const *username = domname + 8;
							p = ntlmssp + LE4(username + 4);
							l = LE2(username);
							if (p + l > end) {
								return -1;
							}
							ustring user_name = smb_get_string(p, l / 2, flags2);

							unsigned char const *hostname = username + 8;
							p = ntlmssp + LE4(hostname + 4);
							l = LE2(hostname);
							if (p + l > end) {
								return -1;
							}
							ustring host_name = smb_get_string(p, l / 2, flags2);

							unsigned char const *sesskey = hostname + 8;
							if (sesskey + 4 > end) {
								return -1;
							}

							unsigned long flags = LE4(sesskey + 8);

							session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
							session->smb_session.temp.push_front(temp);
							if (!user_name.empty()) {
								session->smb_session.temp.front().sessionsetup.domain = domain_name;
								session->smb_session.temp.front().sessionsetup.username = user_name;
								session->smb_session.temp.front().sessionsetup.hostname = host_name;
							}
						} else {
							return -1;
						}
					} else {
						return -1;
					}
				}

				break;

			case SMB_OP_CLOSE:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					if (smbdata + 3 > end) {
						return -1;
					}
					smb_fid_t fid = LE2(smbdata + 1);
					smb_file_state_t *fs = session->smb_session.find_file_state_map(user_id, fid);
					if (fs) {
						session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
						temp.path1 = fs->path;
						temp.fid = fid;
						session->smb_session.temp.push_front(temp);
					}
				}
				break;

			case SMB_OP_TRANSACTION2:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					if (smbdata + 32 > end) {
						return 0;
					}
					unsigned char wordcount = smbdata[0];
					unsigned short totalparamcount = LE2(smbdata + 1);
					unsigned short totaldatacount = LE2(smbdata + 3);
					unsigned short maxparamcount = LE2(smbdata + 5);
					unsigned short maxdatacount = LE2(smbdata + 7);
					unsigned char maxsetupcount = smbdata[9];
					unsigned short flags = LE2(smbdata + 11);
					unsigned long timeout = LE4(smbdata + 13);
					unsigned short paramcount = LE2(smbdata + 19);
					unsigned short paramoffset = LE2(smbdata + 21);
					unsigned short datacount = LE2(smbdata + 23);
					unsigned short dataoffset = LE2(smbdata + 25);
					unsigned long setupcount = LE2(smbdata + 27);
					unsigned short subcommand = LE2(smbdata + 29);
					unsigned short bytecount = LE2(smbdata + 31);
					if (smb + paramoffset + paramcount > end) {
						return 0;
					}
					if (smb + dataoffset + datacount > end) {
						return 0;
					}
					unsigned char const *const t2param = smb + paramoffset;
					unsigned char const *const t2data = smb + dataoffset;
					switch (subcommand) {
					case 8: // SET_FILE_INFO
						if (paramcount >= 4 && datacount >= 1) {
							smb_fid_t fid = LE2(t2param);
							unsigned short infolevel = LE2(t2param + 2);
							if (infolevel == cifs::SMB_INFO_PASSTHROUGH + cifs::FileDispositionInformation || infolevel == cifs::SetFileDispositionInfo) {
								if (t2data[0] & 1) { // Delete on Close
									smb_file_state_t *fs = session->smb_session.find_file_state_map(user_id, fid);
									if (fs) {
										if (fs->flags.opened) {
											fs->flags.delete_on_close = true;
										}
									}
								}
							}
						}
						break;
					}
					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_OPEN:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					if (smbdata + 9 > end) {
						return 0;
					}
					unsigned char wordcount = smbdata[0];
					if (wordcount == 2) {
						unsigned char const *p = smbdata + wordcount * 2 + 1;
						unsigned short bytecount = LE2(p);
						if (bytecount < 1) {
							return -1;
						}

						unsigned short attr = LE2(smbdata + 3);

						p += 2; // skip byte count;
						p++; // skip buffer format
						ustring path = smb_get_string(p, end, flags2);

						session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
						temp.path1 = path;
						if (attr & 0x0010) {
							temp.isdirectory = true;
						} else {
							temp.isdirectory = false;
						}
						session->smb_session.temp.push_front(temp);
					}
				}
				break;

			case SMB_OP_CREATE:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					if (smbdata + 9 > end) {
						return 0;
					}
					unsigned char wordcount = smbdata[0];
					if (wordcount == 3) {
						unsigned char const *p = smbdata + wordcount * 2 + 1;
						unsigned short bytecount = LE2(p);
						if (bytecount < 1) {
							return -1;
						}

						unsigned short attr = LE2(smbdata + 1);

						p += 2; // skip byte count;
						p++; // skip buffer format
						ustring path = smb_get_string(p, end, flags2);

						session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
						temp.path1 = path;
						if (attr & 0x0010) {
							temp.isdirectory = true;
						} else {
							temp.isdirectory = false;
						}
						session->smb_session.temp.push_front(temp);
					}
				}
				break;

			case SMB_OP_NT_CREATE_ANDX:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					unsigned long option = LE4(smbdata + smb::nt_create_andx_req::CreateOptions);
					unsigned short filenamelen = LE2(smbdata + smb::nt_create_andx_req::ByteCount);
					unsigned char const *filenameptr = smbdata + smb::nt_create_andx_req::Filename;
					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.path1 = smb_get_string(filenameptr, filenamelen / 2, flags2);
					if (option & 0x00000001) {
						temp.isdirectory = true;
					} else {
						temp.isdirectory = false;
					}
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_READ_ANDX:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					if (!smb_peek_stream(36 + 27, stream, &ptr, &end, &smb, &smbdata)) {
						return -1;
					}
					smb_fid_t fid = LE2(smbdata + smb::read_adnx_req::Fid);
					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.fid = fid;
					temp.length = 0;
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_WRITE_ANDX:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					if (!smb_peek_stream(36 + 31, stream, &ptr, &end, &smb, &smbdata)) {
						return -1;
					}
					smb_fid_t fid = LE2(smbdata + smb::write_adnx_req::Fid);
					unsigned long len = (unsigned long)(LE2(smbdata + smb::write_adnx_req::DataLengthHigh) << 16) | LE2(smbdata + smb::write_adnx_req::DataLength);
					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.fid = fid;
					temp.length = len;
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_CREATE_DIRECTORY:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					if (!smb_peek_stream(36 + 4, stream, &ptr, &end, &smb, &smbdata)) {
						return -1;
					}
					unsigned short len = LE2(smbdata + 1);
					if (len < 1) {
						return -1;
					}
					unsigned char const *left = smbdata + 3;
					unsigned char const *right = smbdata + 3 + len;
					left++;
					ustring path = smb_get_string(left, right);

					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.path1 = path;
					temp.isdirectory = true;
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_DELETE_DIRECTORY:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					unsigned short filenamelen = LE2(smbdata + smb::delete_directory_req::ByteCount);
					unsigned char const *filenameptr = smbdata + smb::delete_directory_req::DirectoryName;
					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.path1 = smb_get_string(filenameptr, filenameptr + filenamelen);
					temp.isdirectory = true;
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_DELETE:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					unsigned short bytecount = LE2(smbdata + 3);
					ustring path = smb_get_string(smbdata + 6, smbdata + 6 + bytecount, flags2);
					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.path1 = path;
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_RENAME:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					unsigned short srcnamelen = LE2(smbdata + smb::rename_req::ByteCount);
					unsigned char const *p = smbdata + smb::rename_req::OldFileName;
					unsigned char const *srcptr = p;
					unsigned char const *srcend = p;
					unsigned char const *dstptr = end;
					unsigned char const *dstend = end;
					while (p + 2 < end) {
						if (p[0] == 0 && p[1] == 0) {
							srcend = p;
							dstptr = p + 4;
							break;
						}
						p += 2;
					}
					while (dstptr + 1 < dstend && dstend[-1] == 0 && dstend[-2] == 0) {
						dstend -= 2;
					}
					ustring srcname = smb_get_string(srcptr, (srcend - srcptr) / 2, flags2);
					ustring dstname = smb_get_string(dstptr, (dstend - dstptr) / 2, flags2);

					session_data_t::smb_session_t::tmp_t temp(smbcommand, request_id);
					temp.path1 = srcname;
					temp.path2 = dstname;
					session->smb_session.temp.push_front(temp);
				}
				break;

			case SMB_OP_TREE_CONNECT_ANDX:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					if (!cmd_tree_connect_request(session, request_id, smbdata, end, flags2)) {
						return 0;
					}
				}
				break;

			default:
				break;

			}

			while (session->smb_session.temp.size() > 10) {
				session->smb_session.temp.pop_back();
			}

		} else { // !direction

			session_data_t::smb_session_t::tmp_t *temp = 0;
			for (std::deque<session_data_t::smb_session_t::tmp_t>::iterator it = session->smb_session.temp.begin(); it != session->smb_session.temp.end(); it++) {
				if (smbcommand == it->command && request_id == it->request_id) {
					temp = &*it;
					break;
				}
			}

			switch (smbcommand) {

			case SMB_OP_SESSION_SETUP_ANDX:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

					if (smbdata + 5 > end) {
						return -1;
					}

					unsigned char andxcommand = smbdata[1];
					unsigned short andxoffset = LE2(smbdata + 3);

					if (temp) {
						session->smb_session.user_data_map[user_id]->name = temp->sessionsetup.username;
						log_printf(data->time_us, "[SMB] C=%s S=%s (session setup) D=%s U=%s H=%s\n"
							, sid->client.addr.tostring().c_str()
							, sid->server.addr.tostring().c_str()
							, utf8_ustring_to_string(temp->sessionsetup.domain).c_str()
							, utf8_ustring_to_string(temp->sessionsetup.username).c_str()
							, utf8_ustring_to_string(temp->sessionsetup.hostname).c_str()
							);
					}

					if ((temp) && (! temp->sessionsetup.username.empty())) {
						AutoLock lock(get_processor()->mutex());
						{
							file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
							ei->user_name = temp->sessionsetup.username;
							get_processor()->on_login(ei);
						}
					}

					// and x
					if (andxcommand == SMB_OP_TREE_CONNECT_ANDX) {
						cmd_tree_connect_response(session, request_id, user_id, tree_id);
					}
				}
				break;

			case SMB_OP_TRANSACTION2:
				break;

			case SMB_OP_TREE_CONNECT_ANDX:
				cmd_tree_connect_response(session, request_id, user_id, tree_id);
				break;

			case SMB_OP_OPEN:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) {
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *smb;
						unsigned char const *smbdata;
						smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

						if (smbdata + 2 > end) {
							return -1;
						}
						smb_fid_t fid = LE2(smbdata + 1);
						bool isdirectory = (smbdata[smb::nt_create_andx_res::Directory] != 0);
						temp->fid = fid;
						temp->action = 2;
						temp->isdirectory = isdirectory;
						temp->client_addr = sid->client.addr;

						bool ipc = false;
						ustring path = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc);

						smb_file_state_map_t *fsmap = session->smb_session.get_file_state_map(user_id);
						if (fsmap) {
							smb_file_state_t *fs = &(*fsmap)[fid];
							fs->isipc = ipc;
							fs->isdirectory = isdirectory;
							fs->path = path;
							fs->flags.opened = true;
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_CREATE:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) {
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *smb;
						unsigned char const *smbdata;
						smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

						if (smbdata + 2 > end) {
							return -1;
						}
						smb_fid_t fid = LE2(smbdata + 1);
						bool isdirectory = (smbdata[smb::nt_create_andx_res::Directory] != 0);
						temp->fid = fid;
						temp->action = 1;
						temp->isdirectory = isdirectory;
						temp->client_addr = sid->client.addr;

						bool ipc = false;
						ustring path = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc);

						smb_file_state_map_t *fsmap = session->smb_session.get_file_state_map(user_id);
						if (fsmap) {
							smb_file_state_t *fs = &(*fsmap)[fid];
							fs->isipc = ipc;
							fs->isdirectory = isdirectory;
							fs->path = path;
							fs->flags.opened = true;
						}

						if (!ipc) {
							ustring username;
							smb_user_data_map_t::const_iterator it = session->smb_session.user_data_map.find(user_id);
							if (it != session->smb_session.user_data_map.end()) {
								username = it->second->name;
							}
							smb_create_action(1, sid, data, username, temp->action, isdirectory, path);
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_NT_CREATE_ANDX:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) {
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *smb;
						unsigned char const *smbdata;
						smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

						smb_fid_t fid = LE2(smbdata + smb::nt_create_andx_res::Fid);
						unsigned long action = LE4(smbdata + smb::nt_create_andx_res::CreateAction);
						bool isdirectory = (smbdata[smb::nt_create_andx_res::Directory] != 0);
						temp->fid = fid;
						temp->action = action;
						temp->isdirectory = isdirectory;
						temp->client_addr = sid->client.addr;

						bool ipc = false;
						ustring path = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc);

						smb_file_state_map_t *fsmap = session->smb_session.get_file_state_map(user_id);
						if (fsmap) {
							smb_file_state_t *fs = &(*fsmap)[fid];
							fs->isipc = ipc;
							fs->isdirectory = isdirectory;
							fs->path = path;
							fs->flags.opened = true;
						}

						if (!ipc) {
							ustring username;
							smb_user_data_map_t::const_iterator it = session->smb_session.user_data_map.find(user_id);
							if (it != session->smb_session.user_data_map.end()) {
								username = it->second->name;
							}
							smb_create_action(1, sid, data, username, action, isdirectory, path);
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_CLOSE:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus) && temp->fid > 0) {
						smb_file_state_t *filestate = session->smb_session.find_file_state_map(user_id, temp->fid);
						if (filestate && !filestate->isipc) {
							ustring path = filestate->path;


							// when analyzing packet start within SMB-session, "IPC$" info is not setup correctly
							// so "IPC$/srvsvc", "IPC$/wkssvc", "IPC$/spoolss" Read/Write log is generated
							// to prevent these logs, add these code here  (by kitamura)
							if ((end_with(path.c_str(), (uchar_t const *)L">/srvsvc")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/SRVSVC")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/wkssvc")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/WKSSVC")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/spoolss")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/SPOOLSS"))) {
								; // NOP
							}
							else {
								if (filestate->readed > 0) {
									{
										AutoLock lock(get_processor()->mutex());
										{
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
											ei->user_name = smb_get_user_name(session, user_id);
											ei->text1 = path;
											ei->size = filestate->readed;
											get_processor()->on_read(ei);
										}
									}
								}
								if (filestate->written > 0) {
									{
										AutoLock lock(get_processor()->mutex());
										{
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
											ei->user_name = smb_get_user_name(session, user_id);
											ei->text1 = path;
											ei->size = filestate->written;
											get_processor()->on_write(ei);
										}
									}
								}
								if (filestate->flags.delete_on_close) { // deleted
									{
										AutoLock lock(get_processor()->mutex());
										{
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
											ei->user_name = smb_get_user_name(session, user_id);
											ei->text1 = path;
											if (filestate->isdirectory) {
												// see SMB_OP_DELETE_DIRECTORY
											} else {
												get_processor()->on_delete_file(ei);
											}
										}
									}
								}
							}


						}


						// remove file info
						smb_file_state_map_t *fsmap = session->smb_session.get_file_state_map(user_id);
						if (fsmap) {
							smb_file_state_map_t::iterator it = fsmap->find(temp->fid);
							if (it != fsmap->end()) {
								fsmap->erase(it);
							}
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_READ_ANDX:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus) && temp->fid > 0) {
						unsigned char const *ptr;
						unsigned char const *end;
						unsigned char const *smb;
						unsigned char const *smbdata;
						smb_peek_stream(stream, &ptr, &end, &smb, &smbdata);

						if (smbdata + 0x1b > end) {
							return 0;
						}
						smb_fid_t fid = temp->fid;
						smb_file_state_t *filestate = session->smb_session.find_file_state_map(user_id, fid);
						if (filestate) {
							unsigned long long len = (LE4(smbdata + smb::read_adnx_res::DataLengthHigh) << 16) | LE2(smbdata + smb::read_adnx_res::DataLength);
							filestate->readed += len;
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_WRITE_ANDX:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus) && temp->fid > 0) {
						smb_fid_t fid = temp->fid;
						smb_file_state_t *filestate = session->smb_session.find_file_state_map(user_id, fid);
						if (filestate) {
							unsigned long len = temp->length;
							filestate->written += len;
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_CREATE_DIRECTORY:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) { // success
						if (temp->isdirectory) {
							bool ipc = false;
							ustring path = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc);
							if (!ipc) {
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
									ei->user_name = smb_get_user_name(session, user_id);
									ei->text1 = path;
									get_processor()->on_create_dir(ei);
								}
							}
						}
					}
				}
				break;

			case SMB_OP_DELETE_DIRECTORY:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) { // success
						if (temp->isdirectory) {
							bool ipc = false;
							ustring path = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc);
							if (!ipc) {
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
									ei->user_name = smb_get_user_name(session, user_id);
									ei->text1 = path;
									get_processor()->on_delete_dir(ei);
								}
							}
						}
					}
					temp->clear();
				}
				break;

			case SMB_OP_DELETE:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) { // success
						bool ipc = false;
						ustring path = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc);
						if (!ipc) {
							AutoLock lock(get_processor()->mutex());
							{
								file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
								ei->text1 = path;
								ei->user_name = smb_get_user_name(session, user_id);
								get_processor()->on_delete_file(ei);
							}
						}
					}
				}
				break;

			case SMB_OP_RENAME:
				if (temp) {
					if (NTSTATUS_IS_SUCCESS(ntstatus)) { // success
						bool ipc1 = false;
						bool ipc2 = false;
						{
							AutoLock lock(get_processor()->mutex());
							{
								file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB, _packet_number);
								ei->user_name = smb_get_user_name(session, user_id);
								ei->text1 = session->smb_session.make_path(user_id, tree_id, temp->path1, &ipc1);
								ei->text2 = session->smb_session.make_path(user_id, tree_id, temp->path2, &ipc2);
								if (!ipc1 && !ipc2) {
									get_processor()->on_rename(ei);
								}
							}
						}
					}
					temp->clear();
				}
				break;

			}
		}

		return netbioslength + 4;
	}

	ustring PacketAnalyzerThread::smb2_get_user_name(session_data_t const *session, smb2_sid_t sid) const
	{
		smb2_user_name_map_t::const_iterator it = session->smb2_session.user_name_map.find(sid);
		if (it == session->smb2_session.user_name_map.end()) {
			return ustring();
		}
		return it->second;
	}

	void set_smb2_flag(session_data_t *session, bool f)
	{
		if (f) {
			session->protocol_mask |= ProtocolMask::SMB2;
		} else {
			session->protocol_mask &= ~ProtocolMask::SMB2;
		}
	}

	inline static void smb2_peek_stream(blob_t *stream, unsigned char const **ptr, unsigned char const **end, unsigned char const **smb, unsigned char const **smbdata)
	{
		unsigned char const *p = stream->c_str();
		*ptr = p;
		*end = p + stream->size();
		*smb = p + 4;
		*smbdata = p + 4 + LE2(p + 8);
	}

	inline static bool smb2_peek_stream(size_t len, blob_t *stream, unsigned char const **ptr, unsigned char const **end, unsigned char const **smb, unsigned char const **smbdata)
	{
		unsigned char const *p = stream->peek(len);
		if (!p) {
			return false;
		}
		*ptr = p;
		*end = p + stream->size();
		*smb = p + 4;
		*smbdata = p + 4 + LE2(p + 8);
		return true;
	}

	int PacketAnalyzerThread::on_smb2_packet(client_server_t const *sid, packet_capture_data_t *data, bool direction, blob_t *stream)
	{
//if (_packet_number == 22765) {
//	_asm nop
//}

		if (!get_option()->ignore_target_servers) {
			if (!(target_protocol_flags & target_protocol::SMB)) {
				return -1; // 監視対象ではない
			}
		}

		session_data_t *session = data->get_session_data();
		set_smb2_flag(session, true);

		unsigned short smb_header_length;
		unsigned short smbcommand;
		unsigned long ntstatus;
		smb2_request_id_t request_id;
		smb2_tid_t tree_id;
		{
			unsigned char const *p = stream->peek(38);
			if (!p) {
				return 0;
			}

			p = stream->c_str();

			unsigned short netbioslength = BE2(p + 2);

			if ((size_t)netbioslength + 4 > stream->size()) {
				return 0;
			}

			p = stream->c_str();
			unsigned char const *end = p + stream->size();

			// smb header

			unsigned char const *const smb = p + 4;

			smb_header_length = LE2(smb + 4);
			smbcommand = LE2(smb + 0x0c);
			ntstatus = LE4(smb + 0x08);
			request_id.seq = LE8(smb + 0x18);
			request_id.pid = LE4(smb + 0x20);
			request_id.sid = LE8(smb + 0x28);
			tree_id = LE2(smb + 0x24);

			//unsigned char const *const smbdata = smb + smb_header_length;
		}

		if (direction) { // client to server

			switch (smbcommand) {

			case SMB2_OP_LOGOFF:
				{
					ustring username;

					smb2_user_name_map_t::iterator it = session->smb2_session.user_name_map.find(request_id.sid);
					if (it != session->smb2_session.user_name_map.end()) {
						username = it->second;
						session->smb2_session.user_name_map.erase(it);
					}
					if (session->smb2_session.user_name_map.empty()) {
						set_smb2_flag(session, false);
					}

					if (! username.empty()) {
						AutoLock lock(get_processor()->mutex());
						{
							file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
							ei->user_name = username;
							get_processor()->on_logout(ei);
						}
					}
				}
				break;

			case SMB2_OP_SESSSETUP:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (request_id.sid != 0 && smbdata[0] == 25) { //length24とフラグ1の加算25固定値？

						// Session Setup Request Fixed Part Length: 24
						const int offset_securityblob = 24;

						//smbdata[0]からresponseTokenまでのoffset
						const unsigned char *pResponseToken;
						pResponseToken = static_cast<const unsigned char *>(static_cast<const void *>(
							strstr(static_cast<const char *>(static_cast<const void *>(smbdata + offset_securityblob)),
									"NTLMSSP")));
						if (pResponseToken == NULL) {
							return 0;
						}

						ustring domain_name;
						ustring user_name;
						ustring host_name;
						const unsigned char *p;
						
						// 28, 32などはpResponseTokenからのそれぞれの値へのオフセット
						int length_domain_name = LE2(pResponseToken + 28);
						int offset_domain_name = LE4(pResponseToken + 32);
						p = pResponseToken+ offset_domain_name;
						readstring_u(p, p + length_domain_name, &p, &domain_name);

						int length_user_name = LE2(pResponseToken + 36);
						int offset_user_name = LE4(pResponseToken + 40);
						p = pResponseToken + offset_user_name;
						readstring_u(p, p + length_user_name, &p, &user_name);

						int length_host_name = LE2(pResponseToken + 44);
						int offset_host_name = LE4(pResponseToken + 48);
						p = pResponseToken + offset_host_name;
						readstring_u(p, p + length_host_name, &p, &host_name);

						session_data_t::smb2_session_t::tmp_t temp(request_id);
						session->smb2_session.insert(temp);
						if (!user_name.empty()) {
							session->smb2_session.temp.front().sessionsetup.domain = domain_name;
							session->smb2_session.temp.front().sessionsetup.username = user_name;
							session->smb2_session.temp.front().sessionsetup.hostname.clear();
						}
					}
				}
				break;

			case SMB2_OP_TCON: // TreeConnect
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smbdata[0] == 9) {
						if (smbdata + 8 > end) {
							return 0;
						}
						unsigned char const *p = smb + LE2(smbdata + 4); // offset
						unsigned short n = LE2(smbdata + 6); // length
						if (p + n > end) {
							return 0;
						}
						ustring path = smb_get_string(p, p + n);
						session_data_t::smb2_session_t::tmp_t temp(request_id);
						session->smb2_session.insert(temp);

						smb2_tree_entry_t e;
						e.sid = request_id.sid;
						e.path = path;
						session->smb2_session.temp_tree_cache.push_front(e);
						while (session->smb2_session.temp_tree_cache.size() > 100) {
							session->smb2_session.temp_tree_cache.pop_back();
						}
					}
				}
				break;

			case SMB2_OP_CREATE:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb; // SMB2ヘッダ先頭
					unsigned char const *smbdata; // SMB2ヘッダ直後のリクエスト本体先頭
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smbdata + 0x30 > end) {
						return 0;
					}
					unsigned char const *p = smbdata;

					unsigned long accessmask = LE4(p + 0x18);
					unsigned long createoptions = LE4(p + 0x28);
					unsigned short offset = LE2(p + 0x2c);
					unsigned short length = LE2(p + 0x2e);
					if (smb + offset + length > end) {
						return 0;
					}
					ustring path = smb_get_string(smb + offset, length / 2);

					unsigned int chainoffset = LE4(smb + 0x14);
					if (chainoffset) { // compounded requestの場合、次のリクエストを読む
						unsigned short smb2_2nd_command = LE2(smb + chainoffset + 0x0c);
						if (smb2_2nd_command == SMB2_OP_SETINFO) {
							unsigned char smb2_2nd_infolevel =
								*(smb + chainoffset + 64 + 3);
							if (smb2_2nd_infolevel ==  SMB2_FILE_RENAME_INFO) {
								bool ipc = false;
								path = session->smb2_session.make_path(tree_id, path, &ipc);
								unsigned int new_name_len = LE4(smb + chainoffset + 64 + 48);
								ustring new_name = smb_get_string(smb + chainoffset + 64 +48 + 4 , length / 2);
								new_name = session->smb2_session.make_path(tree_id, new_name, &ipc);
								AutoLock lock(get_processor()->mutex());
								{
									file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
									ei->user_name = smb2_get_user_name(session, request_id.sid);
									ei->text1 = path;
									ei->text2 = new_name;
									get_processor()->on_rename(ei);
								}
								break;
							}
						}
					}
					if (path.size() > 0) {
						session_data_t::smb2_session_t::tmp_t temp(request_id);
						temp.infolevel = 0;
						temp.delete_on_close = false;
						temp.am_delete = false;
						temp.path = path;
						if (createoptions & 1) { // directory
							temp.isdirectory = true;
						} else {
							temp.isdirectory = false;
						}
						if (createoptions & 0x1000) { // delete_on_close
							temp.delete_on_close = true;
						} else {
							temp.delete_on_close = false;
						}
						if (accessmask & 0x10000) { // delete on Access Mask
							temp.am_delete = true;
						} else {
							temp.am_delete = false;
						}
						session->smb2_session.insert(temp);
					}
				}
				break;

			case SMB2_OP_CLOSE:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smbdata + 0x18 > end) {
						return 0;
					}

					session_data_t::smb2_session_t::tmp_t temp(request_id);
					temp.fid->assign(smbdata + 0x08, smbdata + 0x18);
					session->smb2_session.insert(temp);
				}
				break;

			case SMB2_OP_READ:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smb + 0x30 > end) {
						return 0;
					}
					session_data_t::smb2_session_t::tmp_t temp(request_id);
					temp.fid->assign(smbdata + 0x10, smbdata + 0x20);
					session->smb2_session.insert(temp);
				}
				break;

			case SMB2_OP_WRITE:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					if (!smb2_peek_stream(smb_header_length + 0x30, stream, &ptr, &end, &smb, &smbdata)) {
						return -1;
					}
					session_data_t::smb2_session_t::tmp_t temp(request_id);
					temp.fid->assign(smbdata + 0x10, smbdata + 0x20);
					session->smb2_session.insert(temp);
				}
				break;

			case SMB2_OP_SETINFO:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smbdata + 0x20 > end) {
						return 0;
					}
					unsigned char infolevel = smbdata[3];
					unsigned long length = LE4(smbdata + 4);
					unsigned short offset = LE2(smbdata + 8);
					binary_handle_t fid(smbdata + 0x10, smbdata + 0x20);
					switch (infolevel) {
					case SMB2_FILE_RENAME_INFO:
						{
							if (smbdata + 0x32 > end) {
								return 0;
							}
							unsigned short l = LE2(smbdata + 0x30);
							unsigned char const *p = smbdata + 0x34;
							if (p + l > end) {
								return 0;
							}
							ustring path = smb_get_string(p, l / 2);
							bool ipc = false;
							path = session->smb2_session.make_path(tree_id, path, &ipc);
							session_data_t::smb2_session_t::tmp_t temp(request_id);
							*temp.fid = fid;
							temp.infolevel = infolevel;
							temp.path = path;
							session->smb2_session.insert(temp);
						}
						break;
					case SMB2_FILE_DISPOSITION_INFO:
						{
							smb2_fid_t fid;
							fid->assign(smbdata + 0x10, smbdata + 0x20);
							smb_file_state_t *state = session->smb2_session.find_file_state_map(fid);
							if (state) {
								if (smbdata + 0x21 > end) {
									return 0;
								}
								unsigned char info = smbdata[0x20];
								if (info & 1) { // delete on close
									session_data_t::smb2_session_t::tmp_t temp(request_id);
									temp.fid = fid;
									temp.infolevel = infolevel;
									temp.delete_on_close = true;
									session->smb2_session.insert(temp);
								}
							}
						}
						break;
					}
				}
				break;
			}
		} else { // !direction

			session_data_t::smb2_session_t::tmp_t *temp = 0;

			std::deque<session_data_t::smb2_session_t::tmp_t>::iterator it;
			for (it = session->smb2_session.temp.begin(); it != session->smb2_session.temp.end(); it++) {
				if (it->request_id == request_id) {
					temp = &*it;
					break;
				}
			}

			switch (smbcommand) {

			case SMB2_OP_SESSSETUP:
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					unsigned long ntstatus = LE4(smb + 8);

					if (temp) {
						session->smb2_session.user_name_map[request_id.sid] = temp->sessionsetup.username;
						// for DEBUG
						/*
						log_printf(data->time_us, "[SMB2] C=%s S=%s (session setup) D=%s U=%s H=%s NTSTATUS=%lx\n"
							, sid->client.addr.tostring().c_str()
							, sid->server.addr.tostring().c_str()
							, utf8_ustring_to_string(temp->sessionsetup.domain).c_str()
							, utf8_ustring_to_string(temp->sessionsetup.username).c_str()
							, utf8_ustring_to_string(temp->sessionsetup.hostname).c_str()
							, ntstatus
							);
						*/
					}

					if ((ntstatus == 0xc000006d) && (temp) && (! temp->sessionsetup.username.empty())) {
						// NT_STATUS_LOGON_FAILURE
						AutoLock lock(get_processor()->mutex());
						{
							file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
							ei->user_name = temp->sessionsetup.username;
							get_processor()->on_login_failed(ei);
						}
					} else if ((ntstatus == 0x00000000) && (temp) && (! temp->sessionsetup.username.empty())) {
						// STATUS_SUCCESS
						AutoLock lock(get_processor()->mutex());
						{
							file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
							ei->user_name = temp->sessionsetup.username;
							get_processor()->on_login(ei);
						}
					} else {
						return -1;
					}
				}
				break;

			case SMB2_OP_TCON: // TreeConnect
				{
					std::deque<smb2_tree_entry_t>::iterator it;
					for (it = session->smb2_session.temp_tree_cache.begin(); it != session->smb2_session.temp_tree_cache.end(); it++) {
						if (it->sid == request_id.sid) {
							smb_tree_t tree;
							tree.name = it->path;
							session->smb2_session.tree_map[tree_id] = tree;
#if 0
log_printf(0, "[SMB] C=%s S=%s tree_connect_andx \"%s\" tid=%u"
	, sid->client.addr.tostring().c_str()
	, sid->server.addr.tostring().c_str()
	, utf8_ustring_to_string(it->second).c_str()
	, tree_id
	);
#endif
							session->smb2_session.temp_tree_cache.erase(it);
							break;
						}
					}
				}
				break;

			case SMB2_OP_CREATE:
				if (temp) {
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smbdata + 0x50 > end) {
						return 0;
					}

					unsigned long action = LE4(smbdata + 4);
					bool isdirectory = temp->isdirectory;
					bool delete_on_close = temp->delete_on_close;
					bool am_delete = temp->am_delete;

					bool ipc = false;
					ustring path = session->smb2_session.make_path(tree_id, temp->path, &ipc);

					smb2_fid_t fid;
					fid->assign(smbdata + 0x40, smbdata + 0x50);
					smb_file_state_t *fs = &session->smb2_session.file_state_map[fid];
					fs->isipc = ipc;
					fs->isdirectory = isdirectory;
					fs->flags.delete_on_close = delete_on_close;
					fs->flags.am_delete = am_delete;
					fs->path = path;
					fs->flags.opened = true;

					ustring username = smb2_get_user_name(session, request_id.sid);
					if (am_delete && !delete_on_close) {
						smb_create_action(2, sid, data, username, 4, isdirectory, path);
					} else {
						smb_create_action(2, sid, data, username, action, isdirectory, path);
					}
					temp->clear();
				}
				break;

			case SMB2_OP_CLOSE:
				if (temp) {
					ustring path;
					std::map<smb2_fid_t, smb_file_state_t>::iterator it = session->smb2_session.file_state_map.find(temp->fid);
					if (it != session->smb2_session.file_state_map.end()) {
						smb_file_state_t *filestate = &it->second;
						if (!filestate->isipc) {
							ustring path = filestate->path;


							// when analyzing packet start within SMB-session, "IPC$" info is not setup correctly
							// so "IPC$/srvsvc", "IPC$/wkssvc", "IPC$/spoolss" Read/Write log is generated
							// to prevent these logs, add these code here  (by kitamura)
							if ((end_with(path.c_str(), (uchar_t const *)L">/srvsvc")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/SRVSVC")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/wkssvc")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/WKSSVC")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/spoolss")) ||
								(end_with(path.c_str(), (uchar_t const *)L">/SPOOLSS"))) {
								; // NOP
							} else {
								AutoLock lock(get_processor()->mutex());
								{
									if (filestate->readed > 0) {
										file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
										ei->user_name = smb2_get_user_name(session, request_id.sid);
										ei->text1 = path;
										ei->size = filestate->readed;
										get_processor()->on_read(ei);
									}
									if (filestate->written > 0) {
										file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
										ei->user_name = smb2_get_user_name(session, request_id.sid);
										ei->text1 = path;
										ei->size = filestate->written;
										get_processor()->on_write(ei);
									}
									if (filestate->flags.delete_on_close) {
										if (filestate->isdirectory) {
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
											ei->user_name = smb2_get_user_name(session, request_id.sid);
											ei->text1 = path;
											get_processor()->on_delete_dir(ei);
										} else {
											file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
											ei->user_name = smb2_get_user_name(session, request_id.sid);
											ei->text1 = path;
											get_processor()->on_delete_file(ei);
										}
									}
								}
							}
						}
						session->smb2_session.file_state_map.erase(it);
					}
					temp->clear();
				}
				break;

			case SMB2_OP_READ:
				if (temp) {
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					if (!smb2_peek_stream(smb_header_length + 8, stream, &ptr, &end, &smb, &smbdata)) {
						return -1;
					}
					smb_file_state_t *filestate = session->smb2_session.find_file_state_map(temp->fid);
					if (filestate) {
						unsigned long len = LE4(smbdata + 4);
						filestate->readed += len;
					}
					temp->clear();
				}
				break;

			case SMB2_OP_WRITE:
				if (temp) {
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smb + 0x30 > end || smbdata + 8 > end) {
						return 0;
					}
					smb_file_state_t *filestate = session->smb2_session.find_file_state_map(temp->fid);
					if (filestate) {
						unsigned long len = LE4(smbdata + 4);
						filestate->written += len;
					}
					temp->clear();
				}
				break;

			case SMB2_OP_SETINFO:
				if (temp) {
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					unsigned long ntstatus = LE4(smb + 8);
					smb_file_state_t *filestate = session->smb2_session.find_file_state_map(temp->fid);
					if (filestate) {
						switch (temp->infolevel) {
						case SMB2_FILE_RENAME_INFO:
							if (NTSTATUS_IS_SUCCESS(ntstatus)) {
								{
									AutoLock lock(get_processor()->mutex());
									{
										file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
										ei->user_name = smb2_get_user_name(session, request_id.sid);
										ei->text1 = filestate->path;
										ei->text2 = temp->path;
										get_processor()->on_rename(ei);
									}
								}
							}
							break;
						case SMB2_FILE_DISPOSITION_INFO:
							if (NTSTATUS_IS_SUCCESS(ntstatus)) {
								filestate->flags.delete_on_close = temp->delete_on_close;
							}
							break;
						}
					}
					temp->clear();
				}
				break;
			case SMB2_OP_NOTIFY:
#if 0
				{
					unsigned char const *ptr;
					unsigned char const *end;
					unsigned char const *smb;
					unsigned char const *smbdata;
					smb2_peek_stream(stream, &ptr, &end, &smb, &smbdata);
					if (smb + 0x30 > end || smbdata + 8 > end) {
						return 0;
					}

					unsigned long ntstatus;
					ntstatus = LE4(smb + 8);
					if (NTSTATUS_IS_SUCCESS(ntstatus)) {
					} else {
						// other status codes
						// STATUS_PENDING: 0x103
						return 0;
					}

					unsigned short blob_offset; // SMB2ヘッダ先頭からのオフセット
					unsigned short blob_length;

					blob_offset = LE2(smbdata + 2);
					blob_length = LE2(smbdata + 4);

					// 古いファイル名のnotify info構造から、
					// 新しいファイル名のnotify info構造へのオフセット
					unsigned long notify_info_offset;
					notify_info_offset = LE4(smb + blob_offset);

					if (notify_info_offset == 0) {
						// 新しい名前へのオフセットがないということは、
						// 新しいファイル名がないということで、
						// リネーム以外の何か
						return 0;
					}

					const unsigned char* p = smb + blob_offset + 12;
					unsigned long old_name_len = LE4(smb + blob_offset + 8);
					ustring old_name = smb_get_string(p, p + old_name_len, 0x8000);

					// for DEBUG
					log_printf(0, "[SMB2] SMB2_OP_NOTIFY old name: %s",
					utf8_ustring_to_string(old_name).c_str());

					p = smb + blob_offset + notify_info_offset + 12;
					unsigned long new_name_len = LE4(smb + blob_offset + 12 + 8);
					ustring new_name = smb_get_string(p, p + new_name_len, 0x8000);

					// for DEBUG
					log_printf(0, "[SMB2] SMB2_OP_NOTIFY new name: %s",
					utf8_ustring_to_string(new_name).c_str());

					AutoLock lock(get_processor()->mutex());
					{
						file_access_event_item_t ei(sid, data, file_access_event_item_t::P_SMB2, _packet_number);
						ei->user_name = smb2_get_user_name(session, request_id.sid);
						ei->text1 = old_name;
						ei->text2 = new_name;
						get_processor()->on_rename(ei);
					}


					/*
					smb_file_state_t *filestate = session->smb2_session.find_file_state_map(temp->fid);
					if (filestate) {
						unsigned long len = LE4(smbdata + 4);
						filestate->written += len;
					}
					temp->clear();
					*/
				}
#endif
				break;
			}
		}

		return -1;
	}

} // namespace ContentsWatcher
