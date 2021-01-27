#include "../config.h"
#ifdef DB_SUPPORT_SQLITE

#ifdef WIN32
#include <winsock2.h>
#endif

#include "db_sqlite.h"
#include "../sqlite/sqlite3.h"
#include "../common/calendar.h"

DB_Abstract_DriverCore *new_DB_DriverCore_SQLite()
{
	return new DB_DriverCore_SQLite();
}

DB_Abstract_StatementCore *new_DB_StatementCore_SQLite(DB_Statement *s)
{
	return new DB_StatementCore_SQLite(s);
}

///////////////////////////////////////////////////////////////////////////////
// DB_DriverCore_SQLite

DB_Abstract_StatementCore *DB_DriverCore_SQLite::new_statement_core(DB_Statement *st) const
{
	return new_DB_StatementCore_SQLite(st);
}

void DB_DriverCore_SQLite::connect(DB_Parameter const *dbparam, db_handle_t *handle) const
{
	sqlite3 *db = 0;

	int rc;
	rc = sqlite3_open(dbparam->text.c_str(), &db);
	if (rc) {
		sqlite3_close(db);
		char tmp[1000];
		sprintf(tmp, "cannot open database '%s'", dbparam->text.c_str());
		throw DBEXCEPTION(tmp);
	}

	handle->value = db;
}

static inline sqlite3 *DBHANDLE(db_handle_t *handle)
{
	return (sqlite3 *)handle->value;
}

void DB_DriverCore_SQLite::disconnect(db_handle_t *handle) const
{
	if (DBHANDLE(handle)) {
		int x = sqlite3_close(DBHANDLE(handle));
		if (x == SQLITE_BUSY) {
			throw DBEXCEPTION("SQLITE_BUSY");
		}
	}
}


double DB_DriverCore_SQLite::get_datetime_value(int year, int month, int day, int hour, int minute, int second)
{
	double t = ToJDN(year, month, day);
	return (((t * 24 + hour) * 60 + minute) * 60 + second) * 1000;
}


///////////////////////////////////////////////////////////////////////////////
// DB_StatementCore_SQLite

std::string DB_StatementCore_SQLite::get_error_message()
{
	return _error_message;
}

void DB_StatementCore_SQLite::execute_()
{
	int s;
	do {
		s = sqlite3_step(_st);
		if (s == SQLITE_ROW) {
			_sqlite_prefetched = true;
			break;
		}
	} while (s == SQLITE_BUSY);
}

void DB_StatementCore_SQLite::begin_transaction()
{
	close();
	prepare("begin");
	execute();
	finalize();
}

void DB_StatementCore_SQLite::finalize()
{
	if (_st) {
		sqlite3_finalize(_st);
		_st = 0;
	}
	_sqlite_prefetched = false;
}

void DB_StatementCore_SQLite::close()
{
	finalize();
}

void DB_StatementCore_SQLite::bind_parameter(int column, DBVARIANT *param)
{
	int ctype = 0;
	int dtype = 0;
	switch (param->datatype) {
	case DBVARIANT::DT_INTEGER:
		sqlite3_bind_int(_st, column + 1, param->get_integer());
		break;
	case DBVARIANT::DT_TINYINT:
		sqlite3_bind_int(_st, column + 1, param->get_integer());
		break;
	case DBVARIANT::DT_LONGLONG:
		sqlite3_bind_int64(_st, column + 1, param->get_longlong());
		break;
	case DBVARIANT::DT_STRING:
		{
			std::string str = param->get_string();
			sqlite3_bind_text(_st, column + 1, str.c_str(), (int)str.size(), SQLITE_TRANSIENT);
		}
		break;
	case DBVARIANT::DT_DATETIME:
		{
			SQL_TIMESTAMP_STRUCT const *p = (SQL_TIMESTAMP_STRUCT const *)&param->buffer[0];
			double t = DB_DriverCore_SQLite::get_datetime_value(p->year, p->month, p->day, p->hour, p->minute, p->second);
			sqlite3_bind_double(_st, column + 1, t);
		}
		break;
	case DBVARIANT::DT_VOID:
		assert(0);
		return;
	default:
		break;
	}
}

void DB_StatementCore_SQLite::prepare(const char *sql)
{
	finalize();
	sqlite3 *db = DBHANDLE(get_connection()->get_handle());
	int rc = sqlite3_prepare_v2(db, sql, -1, &_st, 0);
	if (rc != SQLITE_OK) {
		char const *m = sqlite3_errmsg(db);
		throw DBEXCEPTION(m);
	}
	int n = sqlite3_column_count(_st);
	_statement->_fields.clear();
	_statement->_fields.resize(n);
	for (int i = 0; i < n; i++) {
		_statement->_fields[i].name = sqlite3_column_name(_st, i);
	}
}

std::string const &DB_StatementCore_SQLite::column_name(int i)
{
	return _statement->_fields[i].name;
}

void DB_StatementCore_SQLite::execute()
{
	execute_();
}

void DB_StatementCore_SQLite::unbind()
{
	sqlite3_reset(_st);
}

static bool sqlite_stmt_fetch_row(sqlite3_stmt *st, std::vector<DBVARIANT> *result)
{
	int i, n;
	n = sqlite3_column_count(st);
	result->resize(n);
	for (i = 0; i < n; i++) {
		result->at(i).setup(DBMODE_SQLITE);
		int type = sqlite3_column_type(st, i);
		switch (type) {
		case SQLITE_INTEGER:
			result->at(i).assign_integer(sqlite3_column_int(st, i));
			break;
		case SQLITE_FLOAT:
			result->at(i).assign_double(sqlite3_column_double(st, i));
			break;
		case SQLITE_TEXT:
			{
				char const *p = (char const *)sqlite3_column_text(st, i);
				int n = sqlite3_column_bytes(st, i);
				result->at(i).assign_string(std::string(p, n));
			}
			break;
		case SQLITE_NULL:
			result->at(i).clear();
			break;
		}
	}
	return true;
}

bool DB_StatementCore_SQLite::fetch_()
{
	execute_();
	return _sqlite_prefetched;
}

bool DB_StatementCore_SQLite::fetch(std::vector<DBVARIANT> *result)
{
	if (!_sqlite_prefetched) {
		execute_();
		if (!_sqlite_prefetched) {
			return false;
		}
	}
	_sqlite_prefetched = false;

	if (!result) {
		return true;
	}

	return sqlite_stmt_fetch_row(get_sqlite_stmt(), result);
}

#else // #ifdef DB_SUPPORT_SQLITE

#ifdef WIN32
#include <winsock2.h>
#endif

#include "database.h"

DB_Abstract_DriverCore *new_DB_DriverCore_SQLite()
{
	return 0;
}

DB_Abstract_StatementCore *new_DB_StatementCore_SQLite(DB_Statement *s)
{
	return 0;
}

#endif

