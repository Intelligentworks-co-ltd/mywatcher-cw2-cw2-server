
// cw_output.h: パケットの解析結果をデータベース等の出力する

#ifndef __CWS_OUTPUT_H
#define __CWS_OUTPUT_H

#include "db/database.h"
#include "common/calendar.h"
#include "common/misc.h"
#include "common/thread.h"
#include "common/mutex.h"
#include "common/event.h"
#include "common/container.h"
#include <string>
#include <deque>

#include "cw.h"

class Logger;

struct csvwriter_t {
	std::string _basedir;
	std::string _header;
	YMD _date;
	int _file_number;
	int _line_number;
	FILE *_fp_csv;
	csvwriter_t()
		: _fp_csv(0)
		, _file_number(0)
		, _line_number(0)
	{
	}
	~csvwriter_t()
	{
		close();
	}
	void close();
	void determinate_counters();
	void setbasedir(std::string const &dir)
	{
		_basedir = dir;
	}
	void setheader(std::string const &header)
	{
		_header = header;
	}
	std::string get_dir(YMD const &ymd);
	void write(time_t timestamp, std::string const &line);
};


namespace ContentsWatcher {

	struct file_access_event_item_t {

		enum protocol_t {
			P_UNKNOWN,
			P_SMB,
			P_SMB2,
			P_NFS,
			P_AFP,
		};

		enum operation_t {
			OP_LOGIN,
			OP_OPEN,
			OP_CREATE_FILE,
			OP_CREATE_DIR,
			OP_TRUNCATE,
			OP_WRITE,
			OP_READ,
			OP_DELETE_FILE,
			OP_DELETE_DIR,
			OP_MOVE,
			OP_COPY,
		};

		struct entity_t {
			microsecond_t time_us;
			unsigned long long packetnumber;
			protocol_t protocol;
			std::string command;
			ip_address_t client_addr;
			ip_address_t server_addr;
			ustring user_name;
			ustring text1;
			ustring text2;
			mac_address_t mac_address;
			unsigned long long size;
			entity_t()
			{
				packetnumber = 0;
				time_us = 0;
				protocol = P_UNKNOWN;
				size = 0;
			}
		};

		container<entity_t> entity;

		file_access_event_item_t(client_server_t const *sid, packet_capture_data_t *data, protocol_t prot, unsigned long long packetnumber)
		{
			entity.create();
			entity->time_us = data->time_us;
			entity->client_addr = sid->client.addr;
			entity->server_addr = sid->server.addr;
			entity->mac_address = data->client_mac_address;
			entity->protocol = prot;
			entity->packetnumber = packetnumber;
		}

		entity_t const * operator -> () const
		{
			return &*entity;
		}

		entity_t * operator -> ()
		{
			return &*entity;
		}

	};

	class EventProcessor : public Thread {
	private:
		Logger *_logger;
		Mutex _mutex;
		Event _event;
		std::list<file_access_event_item_t> _queue;
		DB_Parameter db_parameter;
		DB_Driver db_driver;

		bool _archive_csv_enable;
		std::string _archive_csv_base_dir;

		csvwriter_t _csvwriter;

		void check_db_connection(DB_Connection *db);

		void writecsv(unsigned long long id, file_access_event_item_t const &item);

		void autodelete(DB_Connection *db);

		virtual void run();

		void query_user_table();

		struct alert_target_row_t {
			int alert_id;
			ustring unixpath;
			bool act_mkdir;
			bool act_rmdir;
			bool act_create;
			bool act_unlink;
			bool act_read;
			bool act_write;
			bool act_rename;
			bool act_move;
			bool act_copy;
			bool act_osxfile;
			bool is_fldr;
			bool act_mknew;
			std::string server;
			bool act_open;
			bool act_mnt;
			bool act_umnt;
			bool act_symlink;
			bool act_mknod;
			bool act_link;
			bool act_setattr;
		};

		typedef std::vector<alert_target_row_t> alert_target_list_t;


		enum alert_type_t {
			USERNAME,
			IPADDRESS,
			MACADDRESS,
		};

		struct alert_user_t {
			alert_type_t type;
			ustring text;
			int compare(alert_user_t const &r) const
			{
				if (type < r.type) {
					return -1;
				}
				if (type > r.type) {
					return 1;
				}
				if (text < r.text) {
					return -1;
				}
				if (text > r.text) {
					return 1;
				}
				return 0;
			}
			bool operator < (alert_user_t const &r) const
			{
				return compare(r) < 0;
			}
		};

		std::map<ustring, alert_target_list_t> _alert_target_map;
		std::set<alert_user_t> _alert_user_flags;
		time_t _alert_target_map_last_update_time;

		void make_alert_user_flags_(DB_Statement *st);
		void make_alert_target_map_(DB_Statement *st);
		void make_alert_info(DB_Statement *st);

		bool check_alert_user(DB_Statement *st, file_access_event_item_t const &item);

		void update_alert(DB_Statement *st, file_access_event_item_t const &item, unsigned long long log_id, char const *cmd);

		void setup_sqlite();

		void emit_event(std::string const &command, file_access_event_item_t &item);

	public:
		EventProcessor();
		~EventProcessor();
		void setup(Logger *logger);
		void open();
		void close();
		virtual void start();
		virtual void stop();
		void on_file_access(file_access_event_item_t &item);
		void on_login(file_access_event_item_t &item);
		void on_login_failed(file_access_event_item_t &item);
		void on_logout(file_access_event_item_t &item);
		void on_create_file(file_access_event_item_t &item);
		void on_create_dir(file_access_event_item_t &item);
		void on_exchange(file_access_event_item_t &item);
		void on_rename(file_access_event_item_t &item);
		void on_copy(file_access_event_item_t &item);
		void on_move_and_rename(file_access_event_item_t &item);
		void on_delete_file(file_access_event_item_t &item);
		void on_delete_dir(file_access_event_item_t &item);
		void on_read(file_access_event_item_t &item);
		void on_write(file_access_event_item_t &item);

		void enable_archive_csv(bool enable, std::string const &archive_csv_base_dir)
		{
			_archive_csv_enable = enable;
			_archive_csv_base_dir = archive_csv_base_dir;
		}

		Mutex *mutex()
		{
			return &_mutex;
		}
	};

} // namespace ContentsWatcher

#endif

