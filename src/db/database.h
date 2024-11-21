
#ifndef __cws_database_h
#define __cws_database_h

#include "../config.h"

#include <string>
#include <assert.h>
#include <vector>

#include "../common/container.h"


#ifdef DB_SUPPORT_SQLITE
	#include "../sqlite/sqlite3.h"
#else
	typedef void sqlite3;
#endif



#ifdef DB_SUPPORT_MYSQL
#else
	typedef void MYSQL;
#endif



#ifdef DB_SUPPORT_ODBC
	#include <sql.h>
	#include <sqlext.h>
#else
	typedef long SQLLEN;
	typedef void *HDBC;
	struct SQL_TIMESTAMP_STRUCT {
		unsigned short year;
		short month;
		short day;
		short hour;
		short minute;
		short second;
		unsigned int fraction;
	};
	typedef long SQLINTEGER;
#endif




#include <time.h>

#include "../common/logger.h"
#include "../common/mutex.h"
#include "../common/exception.h"
#define DBEXCEPTION(STR) DBException(STR, __FILE__, __LINE__)



class DBException : public Exception {
public:
	DBException(std::string const &str, char const *file, int line)
		: Exception(str, file, line)
	{
	}
	//DBException(ustring const &str, char const *file, int line, int thread_id, char const *sql)
	//	: Exception(str, file, line)
	//{
	//}
};




enum dbmode_t {
	DBMODE_UNKNOWN,
	DBMODE_SQLITE,
	DBMODE_MYSQL,
	DBMODE_ODBC,
};

// 接続情報
class DB_Parameter {
public:
	dbmode_t dbmode;
	std::string host;
	std::string user;
	std::string pass;
	std::string name;
	std::string text;
	//HENV henv;
	//DB_Parameter()
	//{
	//	henv = 0;
	//}
	//~DB_Parameter()
	//{
	//	if (henv) {
	//		SQLFreeEnv(henv);
	//	}
	//}
};

// バインド用パラメータ
class DBVARIANT {
	friend class DB_StatementCore_SQLite;
	friend class DB_StatementCore_MySQL;
	friend class DB_StatementCore_ODBC;
public:
	enum DataType {
		DT_VOID,
		DT_INTEGER,
		DT_TINYINT,
		DT_LONGLONG,
		DT_DOUBLE,
		DT_STRING,
		DT_DATETIME,
	};
private:
	dbmode_t dbmode;
	DataType datatype;
	unsigned char _isnull;
	union {
		unsigned long mysql;
		SQLLEN odbc;
		unsigned long sqlite;
	} length;
	std::vector<unsigned char> buffer;
private:
	template <typename T> T *allocate_buffer()
	{
		size_t len = sizeof(T);
		buffer.resize(len);
		switch (dbmode) {
		case DBMODE_SQLITE: length.sqlite = (unsigned long)len; break;
		case DBMODE_MYSQL:  length.mysql = (unsigned long)len;  break;
		case DBMODE_ODBC:   length.odbc = (SQLLEN)len;          break;
		}
		return (T *)&buffer[0];
	}
public:
	DBVARIANT()
	{
		dbmode = DBMODE_UNKNOWN;
		datatype = DT_VOID;
		_isnull = true;
	}

	DBVARIANT(dbmode_t dbm)
	{
		dbmode = dbm;
		datatype = DT_VOID;
		_isnull = true;
	}

	void clear()
	{
		buffer.clear();
		datatype = DT_VOID;
		_isnull = true;
	}

	void setup(dbmode_t dbm)
	{
		dbmode = dbm;
		datatype = DT_VOID;
		buffer.clear();
		_isnull = true;
	}

	bool isnull() const
	{
		return _isnull != 0;
	}

	void assign_string(char const *str, size_t len);
	void assign_string(uchar_t const *str, size_t len);
	void assign_string(ustring const &str);
	void assign_string_eucjp(ustring const &str);
	void assign_string(char const *str, char const *end)
	{
		assign_string(str, end - str);
	}
	void assign_string(char const *str)
	{
		assign_string(str, str ? strlen(str) : 0);
	}
	void assign_string(std::string const &str)
	{
		assign_string(str.c_str(), str.size());
	}
	void assign_datetime(struct tm const *t);
	void assign_datetime(time_t t);
	void assign_gm_datetime(std::string const &str);
	void assign_lo_datetime(std::string const &str);
	void assign_integer(long n);
	void assign_tinyint(char n);
	void assign_double(double n);
	void assign_longlong(long long n);

	int get_integer() const;
	double get_double() const;
	long long get_longlong() const;
	std::string get_string() const;
};

//

class DB_Statement;
class DB_Abstract_StatementCore;
class DB_Connection;

union db_handle_t {
	//MYSQL *mysql;
	//HDBC hdbc;
	//sqlite3 *sqlite;
	void *value;
};

class DB_Abstract_DriverCore {
public:
	virtual ~DB_Abstract_DriverCore()
	{
	}
	virtual dbmode_t get_dbmode() const = 0;
	virtual void connect(DB_Parameter const *dbparam, db_handle_t *handle) const = 0;
	virtual void disconnect(db_handle_t *handle) const = 0;
	virtual DB_Abstract_StatementCore *new_statement_core(DB_Statement *st) const = 0;
	virtual void init() {}
	virtual void term() {}
	virtual bool is_valid_handle(db_handle_t const *handle) const = 0;
};

class DB_Driver {
private:
	DB_Abstract_DriverCore *_driver_core;
public:
	DB_Driver();
	~DB_Driver();
	void create(DB_Parameter const *dbparam);
	void dispose();
	dbmode_t get_dbmode() const
	{
		assert(_driver_core);
		return _driver_core->get_dbmode();
	}
	void connect(DB_Parameter const *dbparam, DB_Connection *conn) const;
	void disconnect(DB_Connection *conn) const;
	bool is_valid_handle(db_handle_t const *handle) const
	{
		return _driver_core ? _driver_core->is_valid_handle(handle) : false;
	}
};

class DB_Connection {
	friend class DB_Connection_Container;
	friend class DB_Driver;
	friend class DB_Statement;
	friend class DB_StatementCore_SQLite;
	friend class DB_StatementCore_MySQL;
	friend class DB_StatementCore_ODBC;
public: // for debug
	DB_Driver const *_driver;
	db_handle_t _handle;
private:
	void clear()
	{
		memset(&_handle, 0, sizeof(db_handle_t));
		_driver = 0;
	}
	DB_Connection(DB_Connection const &r);
	void operator = (DB_Connection const &r);
public:
	~DB_Connection()
	{
		disconnect();
	}

	DB_Connection()
	{
		memset(&_handle, 0, sizeof(db_handle_t));
		_driver = 0;
	}

public:
	void connect(DB_Driver const *driver, DB_Parameter const *dbparam);
	void disconnect();

	dbmode_t get_dbmode() const
	{
		return _driver ? _driver->get_dbmode() : DBMODE_UNKNOWN;
	}

	db_handle_t *get_handle()
	{
		return &_handle;
	}


	bool is_valid_handle() const
	{
		return _driver ? _driver->is_valid_handle(&_handle) : false;
	}

};

class DB_Connection_Container {
private:
	container<DB_Connection> _entity;
public:
	DB_Connection *get_connection()
	{
		return &*_entity;
	}
	void connect(DB_Driver const *driver, DB_Parameter *dbparam)
	{
		DB_Connection_Container newobj;
		newobj._entity.create();
		newobj._entity->connect(driver, dbparam);
		*this = newobj;
	}
};


class DB_Abstract_StatementCore {
protected:
	DB_Statement *_statement;
public:
	virtual ~DB_Abstract_StatementCore()
	{
	}
	DB_Abstract_StatementCore(DB_Statement *s)
	{
		_statement = s;
	}
	DB_Connection *get_connection();

	virtual void clear_error_message() = 0;
	virtual std::string get_error_message() = 0;
	virtual void prepare(char const *sql) = 0;
	virtual void reserve_parameter(int count) {}
	virtual void bind_parameter(int column, DBVARIANT *param) = 0;
	virtual void execute() = 0;
	virtual void finalize() = 0;
	virtual void unbind() = 0;
	virtual void close() = 0;
	virtual bool fetch(std::vector<DBVARIANT> *result) = 0;
	virtual void begin_transaction() = 0;
	void commit_transaction();
	void rollback_transaction();
	virtual unsigned long long get_affected_rows() = 0;
};

class DB_Statement {
	friend class DB_Abstract_StatementCore;
	friend class DB_StatementCore_ODBC;
	friend class DB_StatementCore_MySQL;
	friend class DB_StatementCore_SQLite;
private:
	DB_Connection *_db;

	Logger *_logger;

	DB_Abstract_StatementCore *_statement_core;

private:
	DB_Statement(DB_Statement const &r);
	void operator = (DB_Statement const &r);
public:
	DB_Statement()
	{
		_db = 0;
		_statement_core = 0;
	}
	~DB_Statement()
	{
		close();
	}

public:
	dbmode_t get_dbmode() const
	{
		return _db ? _db->get_dbmode() : DBMODE_UNKNOWN;
	}

	void clear_error_message()
	{
		assert(_statement_core);
		return _statement_core->clear_error_message();
	}

	std::string get_error_message()
	{
		assert(_statement_core);
		return _statement_core->get_error_message();
	}

	void begin_transaction(DB_Connection *db, Logger *logger);
	void commit_transaction();
	void rollback_transaction();

	void reserve_parameter(int count);
	void bind_parameter(int column, DBVARIANT *param);

	void prepare(char const *sql);
	void execute();
	void finalize();
	void unbind();
	void close();
	bool fetch();

	unsigned long long get_affected_rows();

	bool query_get_integer(char const *sql, int *result_p);
	bool query_get_string(char const *sql, std::string *result_p);

public:
	struct Field {
		std::string name;
	};
protected:
	std::vector<Field> _fields;
	std::vector<DBVARIANT> _column_values;
public:

	int column_count() const
	{
		return (int)_fields.size();
	}

	std::string const &column_name(int i) const
	{
		return _fields[i].name;
	}

	DBVARIANT const &column_value(int col)
	{
		return _column_values[col];
	}

	std::vector<DBVARIANT> const *get_row() const
	{
		return &_column_values;
	}



public:

#ifdef DB_SUPPORT_MYSQL
	unsigned long long x_mysql_insert_id();
#endif

public:
	operator dbmode_t () const
	{
		assert(_db);
		return _db->get_dbmode();
	}
};


DB_Abstract_DriverCore *new_DB_DriverCore_SQLite();
DB_Abstract_DriverCore *new_DB_DriverCore_MySQL();
DB_Abstract_DriverCore *new_DB_DriverCore_ODBC();

DB_Abstract_StatementCore *new_DB_StatementCore_SQLite(DB_Statement *s);
DB_Abstract_StatementCore *new_DB_StatementCore_MySQL(DB_Statement *s);
DB_Abstract_StatementCore *new_DB_StatementCore_ODBC(DB_Statement *s);

#endif

