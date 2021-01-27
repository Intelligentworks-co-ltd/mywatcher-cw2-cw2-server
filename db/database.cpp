
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <stdlib.h>
#include "database.h"
#include <sstream>
#include "../common/text.h"
#include "../common/datetime.h"
#include "../common/calendar.h"
#include "../common/DateTimeParser.h"

#ifdef DB_SUPPORT_ODBC
#include <sql.h>
#include <sqlext.h>
#endif

#ifdef DB_SUPPORT_MYSQL
#include <mysql.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// DBVARIANT

void DBVARIANT::assign_string(char const *str, size_t len)
{
	_isnull = false;
	switch (dbmode) {
	case DBMODE_MYSQL:
		{
			buffer.resize(len + 1);
			char *p = (char *)&buffer[0];
			if (str) {
				memcpy(p, str, len);
			} else {
				memset(p, 0, len);
			}
			p[len] = 0;
			length.mysql = (unsigned long)len;
		}
		break;
	case DBMODE_ODBC:
		{
			buffer.resize(len + 1);
			char *p = (char *)&buffer[0];
			if (str) {
				memcpy(p, str, len);
			} else {
				memset(p, 0, len);
			}
			buffer[len] = 0;
			length.odbc = (SQLINTEGER)len;
		}
		break;
	case DBMODE_SQLITE:
		{
			buffer.resize(len + 1);
			char *p = (char *)&buffer[0];
			if (str) {
				memcpy(p, str, len);
			} else {
				memset(p, 0, len);
			}
			buffer[len] = 0;
			length.sqlite = (unsigned long)len;
		}
		break;
	default:
		break;
	}
	datatype = DT_STRING;
}

void DBVARIANT::assign_string(uchar_t const *str, size_t len)
{
	assign_string(utf8_ustring_to_string(str, len));
}

void DBVARIANT::assign_string(ustring const &str)
{
	assign_string(utf8_ustring_to_string(str));
}

void DBVARIANT::assign_string_eucjp(ustring const &str)
{
	std::stringstream ss;
	soramimi::jcode::convert(&ss, soramimi::jcode::EUCJP, str.c_str(), str.size());
	assign_string(ss.str());
}

void DBVARIANT::assign_datetime(struct tm const *t)
{
	_isnull = false;
	if (t) {
		switch (dbmode) {
#ifdef DB_SUPPORT_MYSQL
	case DBMODE_MYSQL:
		{
			buffer.resize(sizeof(MYSQL_TIME));
			MYSQL_TIME *p = (MYSQL_TIME *)&buffer[0];
			p->neg = false;
			p->time_type = MYSQL_TIMESTAMP_DATETIME;
			p->year = t->tm_year + 1900;
			p->month = t->tm_mon + 1;
			p->day = t->tm_mday;
			p->hour = t->tm_hour;
			p->minute = t->tm_min;
			p->second = t->tm_sec;
			p->second_part = 0;
			length.mysql = sizeof(MYSQL_TIME);
		}
		break;
#endif
#ifdef DB_SUPPORT_ODBC
	case DBMODE_ODBC:
		{
			buffer.resize(sizeof(SQL_TIMESTAMP_STRUCT));
			SQL_TIMESTAMP_STRUCT *p = (SQL_TIMESTAMP_STRUCT *)&buffer[0];
			p->year = t->tm_year + 1900;
			p->month = t->tm_mon + 1;
			p->day = t->tm_mday;
			p->hour = t->tm_hour;
			p->minute = t->tm_min;
			p->second = t->tm_sec;
			p->fraction = 0;
			length.odbc = sizeof(SQL_TIMESTAMP_STRUCT);
		}
		break;
#endif
#ifdef DB_SUPPORT_SQLITE
	case DBMODE_SQLITE:
		{
			buffer.resize(sizeof(SQL_TIMESTAMP_STRUCT));
			SQL_TIMESTAMP_STRUCT *p = (SQL_TIMESTAMP_STRUCT *)&buffer[0];
			p->year = t->tm_year + 1900;
			p->month = t->tm_mon + 1;
			p->day = t->tm_mday;
			p->hour = t->tm_hour;
			p->minute = t->tm_min;
			p->second = t->tm_sec;
			p->fraction = 0;
			length.sqlite = sizeof(SQL_TIMESTAMP_STRUCT);
		}
		break;
#endif
	default:
		break;
		}
	} else {
		_isnull = true;
	}
	datatype = DT_DATETIME;
}

void DBVARIANT::assign_datetime(time_t t)
{
	struct tm tm;
	localtime_r(&t, &tm);
	assign_datetime(&tm);
}

void DBVARIANT::assign_gm_datetime(std::string const &str)
{
	struct tm tm;
	parse_utc_to_tm(str.c_str(), &tm);
	assign_datetime(&tm);
}

void DBVARIANT::assign_lo_datetime(std::string const &str)
{
	soramimi::calendar::DateTime dt;
	soramimi::calendar::Parse(str.c_str(), &dt);
	struct tm tm;
	tm.tm_sec = dt.second;       /* seconds after the minute - [0,59] */
	tm.tm_min = dt.minute;       /* minutes after the hour - [0,59] */
	tm.tm_hour = dt.hour;        /* hours since midnight - [0,23] */
	tm.tm_mday = dt.day;         /* day of the month - [1,31] */
	tm.tm_mon = dt.month - 1;    /* months since January - [0,11] */
	tm.tm_year = dt.year - 1900; /* years since 1900 */
	tm.tm_wday = 0;              /* days since Sunday - [0,6] */
	tm.tm_yday = 0;              /* days since January 1 - [0,365] */
	tm.tm_isdst = 0;             /* daylight savings time flag */
	assign_datetime(&tm);
}

void DBVARIANT::assign_integer(long n)
{
	*allocate_buffer<long>() = n;
	datatype = DT_INTEGER;
	_isnull = false;
}

void DBVARIANT::assign_tinyint(char n)
{
	*allocate_buffer<char>() = n;
	datatype = DT_TINYINT;
	_isnull = false;
}

void DBVARIANT::assign_longlong(long long n)
{
	*allocate_buffer<long long>() = n;
	datatype = DT_LONGLONG;
	_isnull = false;
}

void DBVARIANT::assign_double(double n)
{
	*allocate_buffer<double>() = n;
	datatype = DT_DOUBLE;
	_isnull = false;
}

int DBVARIANT::get_integer() const
{
	switch (datatype) {
	case DT_INTEGER:
		return (int)*(long *)&buffer[0];
	case DT_LONGLONG:
		return (int)*(long long *)&buffer[0];
	case DT_DOUBLE:
		return (int)*(double *)&buffer[0];
	case DT_STRING:
		return atoi((char *)&buffer[0]);
	case DT_DATETIME:
		switch (dbmode) {
#ifdef DB_SUPPORT_MYSQL
		case DBMODE_MYSQL:
			{
				MYSQL_TIME const *p = (MYSQL_TIME const *)&buffer[0];
				struct tm t;
				t.tm_year = p->year - 1900;
				t.tm_mon = p->month - 1;
				t.tm_mday = p->day;
				t.tm_hour = p->hour;
				t.tm_min = p->minute;
				t.tm_sec = p->second;
				return (int)mktime(&t);
			}
#endif
#if defined(DB_SUPPORT_ODBC) || defined(DB_SUPPORT_SQLITE)
		case DBMODE_ODBC:
		case DBMODE_SQLITE:
			{
				SQL_TIMESTAMP_STRUCT const *p = (SQL_TIMESTAMP_STRUCT const *)&buffer[0];
				struct tm t;
				t.tm_year = p->year - 1900;
				t.tm_mon = p->month - 1;
				t.tm_mday = p->day;
				t.tm_hour = p->hour;
				t.tm_min = p->minute;
				t.tm_sec = p->second;
				return (int)mktime(&t);
			}
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}

double DBVARIANT::get_double() const
{
	switch (datatype) {
	case DT_INTEGER:
		return *(long *)&buffer[0];
	case DT_LONGLONG:
		return (double)*(long long *)&buffer[0];
	case DT_DOUBLE:
		return *(double *)&buffer[0];
	case DT_STRING:
		return atof((char *)&buffer[0]);
	case DT_DATETIME:
		switch (dbmode) {
#ifdef DB_SUPPORT_MYSQL
		case DBMODE_MYSQL:
			{
				MYSQL_TIME const *p = (MYSQL_TIME const *)&buffer[0];
				int year = p->year;
				int month = p->month;
				int day = p->day;
				int hour = p->hour;
				int minute = p->minute;
				int second = p->second;
				double t = ToJDN(year, month, day);
				t = (((t * 24 + hour) * 60 + minute) * 60 + second) * 1000;
				return t;
			}
#endif
#if defined(DB_SUPPORT_ODBC) || defined(DB_SUPPORT_SQLITE)
		case DBMODE_ODBC:
		case DBMODE_SQLITE:
			{
				SQL_TIMESTAMP_STRUCT const *p = (SQL_TIMESTAMP_STRUCT const *)&buffer[0];
				int year = p->year;
				int month = p->month;
				int day = p->day;
				int hour = p->hour;
				int minute = p->minute;
				int second = p->second;
				double t = ToJDN(year, month, day);
				t = (((t * 24 + hour) * 60 + minute) * 60 + second) * 1000;
				return t;
			}
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}

long long DBVARIANT::get_longlong() const
{
	switch (datatype) {
	case DT_INTEGER:
		return *(long *)&buffer[0];
	case DT_LONGLONG:
		return *(long long *)&buffer[0];
	case DT_DOUBLE:
		return (long long)*(double *)&buffer[0];
	case DT_STRING:
		return misc::atoi64((char *)&buffer[0]);
	case DT_DATETIME:
		return get_integer();
	default:
		break;
	}
	return 0;
}

std::string to_string(long n)
{
	char tmp[100];
	sprintf(tmp, "%d", (int)n);
	return tmp;
}

std::string to_string(long long n)
{
	char tmp[100];
	sprintf(tmp, "%lld", n);
	return tmp;
}

std::string to_string(double n)
{
	char tmp[100];
	sprintf(tmp, "%f", n);
	return tmp;
}

#ifdef DB_SUPPORT_MYSQL
std::string to_string(MYSQL_TIME const *p)
{
	char tmp[100];
	sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u"
		, p->year
		, p->month
		, p->day
		, p->hour
		, p->minute
		, p->second
		);
	return tmp;
}
#endif

#if defined(DB_SUPPORT_ODBC) || defined(DB_SUPPORT_SQLITE)
std::string to_string(SQL_TIMESTAMP_STRUCT const *p)
{
	char tmp[100];
	sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u"
		, p->year
		, p->month
		, p->day
		, p->hour
		, p->minute
		, p->second
		);
	return tmp;
}
#endif

std::string DBVARIANT::get_string() const
{
	switch (datatype) {
	case DT_INTEGER:
		return to_string(*(long *)&buffer[0]);
	case DT_LONGLONG:
		return to_string(*(long long *)&buffer[0]);
	case DT_DOUBLE:
		return to_string(*(double *)&buffer[0]);
	case DT_STRING:
		{
			char const *left = (char *)&buffer[0];
			char const *right = left + buffer.size();
			while (left < right && right[-1] == 0) {
				right--;
			}
			return std::string(left, right);
		}
	case DT_DATETIME:
		switch (dbmode) {
#ifdef DB_SUPPORT_MYSQL
		case DBMODE_MYSQL:
			return to_string((MYSQL_TIME const *)&buffer[0]);
#endif
#ifdef DB_SUPPORT_ODBC
		case DBMODE_ODBC:
			return to_string((SQL_TIMESTAMP_STRUCT const *)&buffer[0]);
#endif
#ifdef DB_SUPPORT_SQLITE
		case DBMODE_SQLITE:
			return to_string((SQL_TIMESTAMP_STRUCT const *)&buffer[0]);
#endif
		default:
			break;
		}
		break;
	default:
		break;
	}
	return std::string();
}

///////////////////////////////////////////////////////////////////////////////

DB_Driver::DB_Driver()
{
	_driver_core = 0;
}

DB_Driver::~DB_Driver()
{
	dispose();
}

void DB_Driver::create(DB_Parameter const *dbparam)
{
	dispose();
	switch (dbparam->dbmode) {
	case DBMODE_SQLITE:   _driver_core = new_DB_DriverCore_SQLite(); break;
	case DBMODE_MYSQL:    _driver_core = new_DB_DriverCore_MySQL();  break;
	case DBMODE_ODBC:     _driver_core = new_DB_DriverCore_ODBC();   break;
	}
}

void DB_Driver::dispose()
{
	delete _driver_core;
	_driver_core = 0;
}

void DB_Driver::connect(DB_Parameter const *dbparam, DB_Connection *conn) const
{
	if (!_driver_core) {
		throw DBEXCEPTION("invalid database driver");
	}
	_driver_core->connect(dbparam, &conn->_handle);
}

void DB_Driver::disconnect(DB_Connection *conn) const
{
	if (_driver_core && conn) {
		_driver_core->disconnect(&conn->_handle);
	}
}


///////////////////////////////////////////////////////////////////////////////
// DB_Connection

void DB_Connection::connect(DB_Driver const *driver, DB_Parameter const *dbparam)
{
	disconnect();

	_driver = driver;

	_driver->connect(dbparam, this);
}

void DB_Connection::disconnect()
{
	if (_driver) {
		_driver->disconnect(this);
	}
	clear();
}

///////////////////////////////////////////////////////////////////////////////
// DB_Statement

void DB_Statement::begin_transaction(DB_Connection *db, Logger *logger)
{
	close();

	//_logsender = logsender;
	_logger = logger;

	if (!db) {
		throw DBEXCEPTION("database handle is null");
	}

	_db = db;

	switch (db->get_dbmode()) {

	case DBMODE_MYSQL:
		_statement_core = new_DB_StatementCore_MySQL(this);
		break;

	case DBMODE_ODBC:
		_statement_core = new_DB_StatementCore_ODBC(this);
		break;

	case DBMODE_SQLITE:
		_statement_core = new_DB_StatementCore_SQLite(this);
		break;

	default:
		throw DBEXCEPTION("Unknown database type");
	}

	if (!_statement_core) {
		throw DBEXCEPTION("Unsupported database type");
	}

	_statement_core->begin_transaction();
}

void DB_Statement::commit_transaction()
{
	assert(_statement_core);
	_statement_core->commit_transaction();
}

void DB_Statement::rollback_transaction()
{
	assert(_statement_core);
	_statement_core->rollback_transaction();
}

void DB_Statement::finalize()
{
	assert(_statement_core);
	_statement_core->finalize();
}

void DB_Statement::unbind()
{
	assert(_statement_core);
	_statement_core->unbind();
}

void DB_Statement::close()
{
	_column_values.clear();
	if (_statement_core) {
		_statement_core->close();
		delete _statement_core;
		_statement_core = 0;
	}
}

void DB_Statement::reserve_parameter(int count)
{
	assert(_statement_core);
	_statement_core->reserve_parameter(count);
}

void DB_Statement::bind_parameter(int column, DBVARIANT *param)
{
	assert(_statement_core);
	_statement_core->bind_parameter(column, param);
}

void DB_Statement::prepare(const char *sql)
{
	assert(_statement_core);

	if (_logger) {
		if (sql) {
			_logger->log_printf(0, "SQL: %s", sql);
		}
	}

	_statement_core->prepare(sql);
}

void DB_Statement::execute()
{
	assert(_statement_core);
	_statement_core->execute();
}

bool DB_Statement::fetch()
{
	assert(_statement_core);
	return _statement_core->fetch(&_column_values);
}

unsigned long long DB_Statement::get_affected_rows()
{
	assert(_statement_core);
	return _statement_core->get_affected_rows();
}

bool DB_Statement::query_get_integer(const char *sql, int *result_p)
{
	bool fetched = false;
	prepare(sql);
	execute();
	if (fetch()) {
		if (column_count() >= 1) {
			*result_p = column_value(0).get_integer();
			fetched = true;
		}
	}
	finalize();
	return fetched;
}

bool DB_Statement::query_get_string(const char *sql, std::string *result_p)
{
	bool fetched = false;
	prepare(sql);
	execute();
	if (fetch()) {
		if (column_count() >= 1) {
			*result_p = column_value(0).get_string();
			fetched = true;
		}
	}
	finalize();
	return fetched;
}







DB_Connection *DB_Abstract_StatementCore::get_connection()
{
	assert(_statement);
	return _statement->_db;
}




void DB_Abstract_StatementCore::commit_transaction()
{
	try {
		prepare("commit");
		execute();
		finalize();
	} catch (DBException) {
	}
}

void DB_Abstract_StatementCore::rollback_transaction()
{
	try {
		prepare("rollback");
		execute();
		finalize();
	} catch (DBException) {
	}
}


