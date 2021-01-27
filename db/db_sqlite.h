
#ifndef __db_sqlite_h
#define __db_sqlite_h

#include "database.h"


class DB_DriverCore_SQLite : public DB_Abstract_DriverCore {
public:
	virtual dbmode_t get_dbmode() const
	{
		return DBMODE_SQLITE;
	}
	virtual void connect(DB_Parameter const *dbparam, db_handle_t *handle) const;
	virtual void disconnect(db_handle_t *handle) const;
	virtual DB_Abstract_StatementCore *new_statement_core(DB_Statement *st) const;

	static double get_datetime_value(int year, int month, int day, int hour, int minute, int second);

	virtual bool is_valid_handle(db_handle_t const *handle) const
	{
		return handle->value != 0;
	}
};

class DB_StatementCore_SQLite : public DB_Abstract_StatementCore {
private:
	std::string _error_message;
	bool _sqlite_prefetched;

	sqlite3_stmt *_st;

private:
	DB_StatementCore_SQLite(DB_StatementCore_SQLite const &r);
	void operator = (DB_StatementCore_SQLite const &r);
public:
	DB_StatementCore_SQLite(DB_Statement *s)
		: DB_Abstract_StatementCore(s)
	{
		_st = 0;
		_sqlite_prefetched = false;
	}
	~DB_StatementCore_SQLite()
	{
		close();
	}
private:
	sqlite3_stmt *get_sqlite_stmt()
	{
		assert(get_dbmode() == DBMODE_SQLITE);
		return _st;
	}
public:
	dbmode_t get_dbmode() const
	{
		return DBMODE_SQLITE;
	}

	virtual void clear_error_message()
	{
		_error_message.clear();
	}

	virtual std::string get_error_message();

	void begin_transaction();

	virtual void prepare(char const *sql);
	std::string const &column_name(int i);
	virtual void bind_parameter(int column, DBVARIANT *param);
	virtual void execute();
	virtual void unbind();
	virtual void finalize();
	virtual void close();
	virtual bool fetch(std::vector<DBVARIANT> *result);

private:
	bool fetch_();
	void execute_();
public:
};



#endif

