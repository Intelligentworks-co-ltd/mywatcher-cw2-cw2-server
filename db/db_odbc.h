#include "../config.h"
#ifdef DB_SUPPORT_ODBC

#ifndef __db_odbc_h
#define __db_odbc_h

#include "database.h"


class DB_DriverCore_ODBC : public DB_Abstract_DriverCore {
private:
	HENV henv;
public:
	DB_DriverCore_ODBC()
	{
		henv = 0;
	}
	virtual dbmode_t get_dbmode() const
	{
		return DBMODE_ODBC;
	}
	virtual void init();
	virtual void term();
	virtual void connect(DB_Parameter const *dbparam, db_handle_t *handle) const;
	virtual void disconnect(db_handle_t *handle) const;
	virtual DB_Abstract_StatementCore *new_statement_core(DB_Statement *st) const;
	virtual bool is_valid_handle(db_handle_t const *handle) const
	{
		return handle->value != 0;
	}
};

class DB_StatementCore_ODBC : public DB_Abstract_StatementCore {
private:
	std::string _error_message;

	HSTMT _st;

private:
	DB_StatementCore_ODBC(DB_StatementCore_ODBC const &r);
	void operator = (DB_StatementCore_ODBC const &r);
public:
	DB_StatementCore_ODBC(DB_Statement *s)
		: DB_Abstract_StatementCore(s)
	{
		_st = 0;
	}
	~DB_StatementCore_ODBC()
	{
		close();
	}
private:
	HSTMT get_odbc_stmt()
	{
		assert(get_dbmode() == DBMODE_ODBC);
		return _st;
	}
public:
	dbmode_t get_dbmode() const
	{
		return DBMODE_ODBC;
	}

	virtual void clear_error_message()
	{
		_error_message.clear();
	}

	virtual std::string get_error_message();

	void begin_transaction();

	virtual void prepare(char const *sql);
	virtual void bind_parameter(int column, DBVARIANT *param);
	virtual void execute();
	virtual void unbind();
	virtual void finalize();
	virtual void close();
	virtual bool fetch(std::vector<DBVARIANT> *result);

};


#endif
#endif // #ifdef DB_SUPPORT_ODBC

