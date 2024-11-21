#include "../config.h"
#ifdef DB_SUPPORT_MYSQL


#ifndef __db_mysql_h
#define __db_mysql_h

#include "database.h"

class DB_DriverCore_MySQL : public DB_Abstract_DriverCore {
public:
	virtual dbmode_t get_dbmode() const
	{
		return DBMODE_MYSQL;
	}
	virtual void init();
	virtual void connect(DB_Parameter const *dbparam, db_handle_t *handle) const;
	virtual void disconnect(db_handle_t *handle) const;
	virtual DB_Abstract_StatementCore *new_statement_core(DB_Statement *st) const;
	virtual bool is_valid_handle(db_handle_t const *handle) const
	{
		return handle->value != 0;
	}
};

class DB_StatementCore_MySQL : public DB_Abstract_StatementCore {
private:
	std::string _error_message;

	MYSQL_STMT *_st;

	std::vector<MYSQL_BIND> _mysql_parameters;

private:
	DB_StatementCore_MySQL(DB_StatementCore_MySQL const &r);
	void operator = (DB_StatementCore_MySQL const &r);
public:
	DB_StatementCore_MySQL(DB_Statement *s)
		: DB_Abstract_StatementCore(s)
	{
		_st = 0;
	}
	~DB_StatementCore_MySQL()
	{
		close();
	}
private:
	MYSQL_STMT *get_mysql_stmt()
	{
		assert(get_dbmode() == DBMODE_MYSQL);
		return _st;
	}
	bool fetch_row_(std::vector<DBVARIANT> *result);
public:
	dbmode_t get_dbmode() const
	{
		return DBMODE_MYSQL;
	}

	virtual void clear_error_message()
	{
		_error_message.clear();
	}

	virtual std::string get_error_message();

	virtual void prepare(char const *sql);
	virtual void begin_transaction();
	virtual void reserve_parameter(int count)
	{
		_mysql_parameters.reserve(count);
	}
	virtual void bind_parameter(int column, DBVARIANT *param);
	virtual void execute();
	virtual void unbind();
	virtual void finalize();
	virtual void close();
	virtual bool fetch(std::vector<DBVARIANT> *result);

	virtual unsigned long long get_affected_rows();

	void mysql_stmt_bind_result_(MYSQL_BIND * bnd);

public:

	//

	//unsigned long long x_mysql_insert_id()
	//{
	//	return mysql_insert_id(get_connection()->get_mysql_handle());
	//}
};




#endif
#endif // #ifdef DB_SUPPORT_MYSQL
