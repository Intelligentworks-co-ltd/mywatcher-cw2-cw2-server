#ifndef __CW_H
#define __CW_H

#include "config.h"
#include "common/uchar.h"
#include "common/misc.h"
#include "common/netaddr.h"
#include "common/container.h"

#include <map>
#include <set>
#include <list>
#include <deque>

#include <time.h>

#ifdef WIN32
#define _NOP _asm nop
#else
#define _NOP
#endif


namespace ContentsWatcher {
	struct exclusion_path_entry_t {
		ip_address_t server_address;
		ustring pattern;
		int type;
	};
	struct exclusion_pattern_entry_t {
		ustring pattern;
		enum Type {
			CONTAIN = 1,
			STARTS = 2,
			ENDS = 3,
			COMPLETE = 4,
		} type;
	};

	struct ExclusionInfo {
		time_t mtime; // last update time
		bool cmd_read;
		std::vector<exclusion_path_entry_t> file_and_dir_map;
		std::vector<exclusion_pattern_entry_t> pattern_map;
		ExclusionInfo()
		{
			mtime = 0;
			cmd_read = false;
		}
	};

	namespace target_protocol {
		enum flags {
			AFP = 1,
			SMB = 2,
			NFS = 4,
		};
	}

	struct TargetServersInfo {
		time_t mtime;
		std::map<ip_address_t, int> map;
		TargetServersInfo()
		{
			mtime = 0;
		}
	};

	struct client_server_t {
		struct {
			ip_address_t addr;
			unsigned short port;
		} client, server; // リクエストする側、レスポンスを返す側

		int compare(client_server_t const &r) const;

		bool operator < (client_server_t const &r) const
		{
			return compare(r) < 0;
		}

		bool operator == (client_server_t const &r) const
		{
			return compare(r) == 0;
		}

		bool operator != (client_server_t const &r) const
		{
			return compare(r) != 0;
		}

		client_server_t()
		{
			client.port = 0;
			server.port = 0;
		}
	};

	struct binary_handle_t {
		std::vector<unsigned char> data;
		binary_handle_t()
		{
		}
		binary_handle_t(unsigned char const *ptr, unsigned char const *end)
		{
			assign(ptr, end);
		}
		void assign(unsigned char const *ptr, unsigned char const *end)
		{
			data.clear();
			data.insert(data.end(), ptr, end);
		}
		bool operator < (binary_handle_t const &right) const;
		std::string tostring() const;
	};

	typedef binary_handle_t nfs_file_handle_t;

	typedef unsigned long rpc_program_number_t;
	enum rpc_program_number {
		RPC_PORTMAP = 100000,
		RPC_NFS = 100003,
		RPC_MOUNT = 100005,
	};

	enum protocol_type_t {
		PROTOCOL_UNKNOWN = 0,
		PROTOCOL_HTTP,
		PROTOCOL_SMB, // smb 1 or 2
		PROTOCOL_SMB1,
		PROTOCOL_SMB2,
		PROTOCOL_DSI,
		PROTOCOL_RPC_PORTMAP,
		PROTOCOL_RPC_NFS,
		PROTOCOL_RPC_MOUNT,
		PROTOCOL_NETBIOS_DG,
	};

	// smb

	typedef unsigned short smb_tid_t; // tree id
	typedef unsigned short smb_pid_t; // process id
	typedef unsigned short smb_uid_t; // user id
	typedef unsigned short smb_mid_t; // multiplex id

	struct smb_request_id_t {
		smb_pid_t pid;
		smb_mid_t mid;
		smb_request_id_t()
		{
			pid = 0;
			mid = 0;
		}
		int compare(smb_request_id_t const &r) const;
		bool operator < (smb_request_id_t const &r) const
		{
			return compare(r) < 0;
		}
		bool operator == (smb_request_id_t const &r) const
		{
			return compare(r) == 0;
		}
	};

	// smb2

	typedef unsigned long smb2_tid_t;
	typedef unsigned long smb2_pid_t;
	typedef unsigned long long smb2_sid_t;

	typedef container<binary_handle_t> smb2_fid_t;

	struct smb2_request_id_t {
		unsigned long long seq;
		smb2_pid_t pid;
		smb2_sid_t sid;
		smb2_request_id_t()
		{
			seq = 0;
			pid = 0;
			sid = 0;
		}
		int compare(smb2_request_id_t const &r) const;
		bool operator < (smb2_request_id_t const &r) const
		{
			return compare(r) < 0;
		}
		bool operator == (smb2_request_id_t const &r) const
		{
			return compare(r) == 0;
		}
	};

	typedef unsigned short smb_fid_t;

	struct smb_file_flags_t {
		bool opened;
		bool delete_on_close;
		bool am_delete;
		smb_file_flags_t();
	};

	struct smb_file_state_t {
		bool isipc;
		bool isdirectory;
		ustring path;
		smb_file_flags_t flags;
		unsigned long long readed;
		unsigned long long written;
		smb_file_state_t()
			: isdirectory(false)
		{
			isipc = false;
			isdirectory = false;
			readed = 0;
			written = 0;
		}
		smb_file_state_t(bool isdir, ustring path)
			: isdirectory(isdir)
			, path(path)
		{
			isipc = false;
			isdirectory = false;
			readed = 0;
			written = 0;
		}
	};

	typedef std::map<smb_fid_t, smb_file_state_t> smb_file_state_map_t;

	struct smb_tree_t {
		ustring name;
	};

	struct smb_user_data_t {
		ustring name;
		smb_file_state_map_t file_state_map;
		std::map<smb_tid_t, smb_tree_t> tree_map;
	};

	typedef std::map<smb_uid_t, container<smb_user_data_t> > smb_user_data_map_t;

	typedef std::map<smb2_sid_t, ustring> smb2_user_name_map_t;


	// afp

	typedef unsigned short afp_vid_t;
	typedef unsigned long afp_did_t;
	typedef unsigned short afp_fid_t;

	struct afp_dir_item_t {
		afp_did_t parent_did;
		ustring name;
		unsigned long long packetnumber;
		int compare(afp_dir_item_t const &r) const
		{
			if (parent_did < r.parent_did) {
				return -1;
			}
			if (parent_did > r.parent_did) {
				return 1;
			}
			if (name < r.name) {
				return -1;
			}
			if (name > r.name) {
				return 1;
			}
			return 0;
		}
		bool operator < (afp_dir_item_t const &r) const
		{
			return compare(r) < 0;
		}
	};

	struct afp_file_attribute_t {
		bool isdir;
		afp_file_attribute_t()
		{
			isdir = false;
		}
	};

	typedef std::map<afp_did_t, afp_dir_item_t> afp_dir_map_t;

	struct afp_volume_info_t {
		ustring name;
		std::list<afp_dir_map_t> dir_map_table;
		std::set<afp_dir_item_t> is_dir_table;
	};

	struct afp_fork_data_t {
		ustring name;
		unsigned long long readed;
		unsigned long long written;
		afp_fork_data_t()
		{
			readed = 0;
			written = 0;
		}
	};

	// nfs

	struct nfs_file_item_t {
		std::string name;
		nfs_file_item_t()
		{
		}
		nfs_file_item_t(std::string name, nfs_file_handle_t handle)
		{
			this->name = name;
		}
		bool operator < (nfs_file_item_t const &r) const
		{
			return name < r.name;
		}
	};

	// smb

	struct smb2_tree_entry_t {
		smb2_sid_t sid;
		ustring path;
	};

	namespace ProtocolMask {
		enum {
			SMB1 = 0x00000001,
			SMB2 = 0x00000002,
			AFP  = 0x00000004,
			NFS  = 0x00000008,
		};
	}

	struct _client_data_t {
		mutable time_t lastupdate;
		ustring domain;
		ustring username;
		ustring hostname;
		_client_data_t()
		{
			lastupdate = 0;
		}
		void touch() const
		{
			lastupdate = time(0);
		}
	};

	struct session_data_t {

		unsigned long protocol_mask;

		session_data_t()
		{
			protocol_mask = 0;
		}

		struct rpc_session_t {
			unsigned long xid;
			unsigned long program;
			unsigned long procedure;
			rpc_program_number_t param_program_number;
			std::string param_directory;
			std::string param_credential;
		} rpc_session;

		struct smb_session_t {
			struct tmp_t {
				int command;
				smb_request_id_t request_id;
				smb_fid_t fid;
				unsigned long action;
				ustring path1;
				ustring path2;
				struct {
					ustring domain;
					ustring username;
					ustring hostname;
				} sessionsetup;
				bool isdirectory;
				ip_address_t client_addr;
				unsigned long length;
				void init_()
				{
					fid = 0;
					action = 0;
					isdirectory = false;
					length = 0;
				}
				tmp_t()
					: command(0)
				{
					init_();
				}
				tmp_t(int c, smb_request_id_t const &r)
					: command(c)
					, request_id(r)
				{
					init_();
				}
				void clear()
				{
					*this = tmp_t();
				}
			};

			std::deque<tmp_t> temp;
			std::map<smb_request_id_t, ustring> temp_tree_cache;

			smb_user_data_map_t user_data_map;

			inline smb_file_state_map_t *get_file_state_map(smb_uid_t uid);
			inline std::map<smb_tid_t, smb_tree_t> *get_tree_map(smb_uid_t uid, unsigned short tid);
			inline smb_file_state_t *find_file_state_map(smb_uid_t uid, smb_fid_t const &fid);

			inline smb_tree_t const *get_tree(smb_uid_t uid, unsigned short tid);
			ustring make_path_(smb_tree_t const *tree, ustring const &path);
			ustring make_path(smb_tree_t const *tree, ustring const &path);
			ustring make_path(smb_uid_t uid, unsigned short tid, ustring const &path, bool *ipc);

		} smb_session;

		struct smb2_session_t {
			struct tmp_t {
				unsigned char infolevel;
				smb2_request_id_t request_id;
				ustring path;
				smb2_fid_t fid;
				bool isdirectory;
				bool delete_on_close;
				bool am_write; // Access Mask
				bool am_delete; // Access Mask
				unsigned long sharemask; // Share Mask
				struct {
					ustring domain;
					ustring username;
					ustring hostname;
				} sessionsetup;
				void init_()
				{
					infolevel = 0;
					isdirectory = false;
					delete_on_close = false;
					am_delete = false;
				}
				tmp_t()
				{
					init_();
				}
				tmp_t(smb2_request_id_t const &r)
					: request_id(r)
				{
					init_();
				}
				void clear()
				{
					*this = tmp_t();
				}
			};
			std::deque<tmp_t> temp;

			std::deque<smb2_tree_entry_t> temp_tree_cache;

			std::map<smb2_tid_t, smb_tree_t> tree_map;

			std::map<smb2_fid_t, smb_file_state_t> file_state_map;

			smb2_user_name_map_t user_name_map;

			smb_file_state_t *find_file_state_map(smb2_fid_t const &fid)
			{
				std::map<smb2_fid_t, smb_file_state_t>::iterator it = file_state_map.find(fid);
				return (it == file_state_map.end()) ? 0 : &it->second;
			}

			inline smb_tree_t const *get_tree(smb2_tid_t tid);
			ustring make_path_(smb_tree_t const *tree, ustring const &path);
			ustring make_path(smb_tree_t const *tree, ustring const &path);
			ustring make_path(smb2_tid_t tid, ustring const &path, bool *ipc);

			void insert(tmp_t const &t)
			{
				temp.push_front(t);
				while (temp.size() > 10) {
					temp.pop_back();
				}
			}

		} smb2_session;

		struct afp_session_t {
			struct tmp_t {
				unsigned short command;
				unsigned short request_id;
				afp_vid_t vid;
				afp_did_t did;
				afp_fid_t fid;
				unsigned long long length;
				ustring name1;
				ustring name2;
				bool isdir;
				tmp_t()
				{
					command = 0;
					request_id = 0;
					vid = 0;
					did = 0;
					fid = 0;
					length = 0;
					isdir = false;
				}
				void clear()
				{
					*this = tmp_t();
				}
			} temp;

			afp_session_t()
			{
			}

			ustring login_name;

			std::map<afp_vid_t, afp_volume_info_t> volumemap;
			std::map<afp_fid_t, afp_fork_data_t> forkmap;

			afp_volume_info_t *get_volume_info(afp_vid_t vid)
			{
				std::map<afp_vid_t, afp_volume_info_t>::iterator it = volumemap.find(vid);
				if (it == volumemap.end()) {
					it = volumemap.insert(volumemap.end(), std::pair<afp_vid_t, afp_volume_info_t>(vid, afp_volume_info_t()));
				}
				return &it->second;
			}

			afp_fork_data_t *find_fork_data(afp_fid_t fid)
			{
				std::map<afp_fid_t, afp_fork_data_t>::iterator it = forkmap.find(fid);
				if (it == forkmap.end()) {
					return 0;
				}
				return &it->second;
			}

			afp_dir_item_t *find_did(afp_volume_info_t *v, afp_did_t did);
			ustring get_folder(client_server_t const *sid, afp_vid_t vid, afp_did_t did);
			ustring make_path(client_server_t const *sid, afp_vid_t vid, afp_did_t did, ustring const &name);
			void insert_dir_entry(afp_vid_t vid, afp_did_t did, afp_did_t fid, ustring const &name, bool isdir, unsigned long long packetnumber);

		} afp_session;

		struct nfs_session_t {
			std::map<nfs_file_handle_t, nfs_file_item_t> pathmap;
			struct {
				std::string path;
				nfs_file_handle_t current_file_handle;
				std::string sub_item_name;
			} temp;
		} nfs_session;

	};

	struct packet_capture_data_t {

		time_t lastupdate;

		microsecond_t time_us;

		mac_address_t client_mac_address;
		
		bool FIN_received;	// receive FIN bit active packet

		struct stream_buffer_t {
			bool sequence_is_valid;
			unsigned long sequence;
			container<blob_t> buffer;
			stream_buffer_t()
			{
				sequence_is_valid = false;
				sequence = 0;
			}
		} client, server;

		packet_capture_data_t();

		session_data_t session_data;


		session_data_t *get_session_data()
		{
			return &session_data;
		}
		session_data_t const *get_session_data() const
		{
			return &session_data;
		}
	};

} // namespace ContentsWatcher

void get_title_text(std::vector<std::string> *lines);

#endif
