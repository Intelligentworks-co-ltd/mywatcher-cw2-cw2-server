#include "../config.h"
#ifdef DB_SUPPORT_ODBC

#ifdef WIN32
#include <winsock2.h>
#endif

#include "db_odbc.h"
#include <sql.h>
#include <sqlext.h>
#include "../common/calendar.h"


DB_Abstract_DriverCore *new_DB_DriverCore_ODBC()
{
	return new DB_DriverCore_ODBC();
}

DB_Abstract_StatementCore *new_DB_StatementCore_ODBC(DB_Statement *s)
{
	return new DB_StatementCore_ODBC(s);
}

std::string get_odbc_error_message(HENV henv, HDBC hdbc, HSTMT hstmt)
{
	std::stringstream ss;

	size_t count = 0;

	char state[10];
	SQLINTEGER native;
	char message[1000];
	SQLSMALLINT msglen = 0;
	while (1) {
		SQLRETURN rc;
		rc = SQLErrorA(henv, hdbc, hstmt, (SQLCHAR *)state, &native, (SQLCHAR *)message, sizeof(message), &msglen);
		if (rc == SQL_NO_DATA) {
			break;
		}
		if (count > 0) {
			ss.put(L'\t');
		}
		ss.write(message, msglen);
	}
	return ss.str();
}


///////////////////////////////////////////////////////////////////////////////
// DB_DriverCore_ODBC

void DB_DriverCore_ODBC::init()
{
	SQLAllocEnv(&henv);
}

void DB_DriverCore_ODBC::term()
{
	SQLFreeEnv(henv);
}

DB_Abstract_StatementCore *DB_DriverCore_ODBC::new_statement_core(DB_Statement *st) const
{
	return new_DB_StatementCore_ODBC(st);
}

void DB_DriverCore_ODBC::connect(DB_Parameter const *dbparam, db_handle_t *handle) const
{
	HDBC hdbc = 0;
	SQLRETURN rc;
	rc = SQLAllocConnect(henv, &hdbc);
	if (!SQL_SUCCEEDED(rc)) {
		throw DBEXCEPTION(get_odbc_error_message(henv, 0, 0));
	}

	try {
		rc = SQLDriverConnectA(hdbc, 0, (SQLCHAR *)dbparam->text.c_str(), SQL_NTS, 0, 0, 0, SQL_DRIVER_NOPROMPT);
		if (!SQL_SUCCEEDED(rc)) {
			throw DBEXCEPTION(get_odbc_error_message(0, hdbc, 0));
		}
		rc = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0);
		if (!SQL_SUCCEEDED(rc)) {
			throw DBEXCEPTION(get_odbc_error_message(0, hdbc, 0));
		}
	} catch (...) {
		SQLFreeConnect(hdbc);
		throw;
	}

	handle->value = hdbc;
}

static inline HDBC DBHANDLE(db_handle_t *handle)
{
	return (HDBC)handle->value;
}

void DB_DriverCore_ODBC::disconnect(db_handle_t *handle) const
{
	if (DBHANDLE(handle)) {
		SQLDisconnect(DBHANDLE(handle));
		SQLFreeConnect(DBHANDLE(handle));
	}
}

///////////////////////////////////////////////////////////////////////////////
// DB_StatementCore_ODBC

std::string DB_StatementCore_ODBC::get_error_message()
{
	_error_message = get_odbc_error_message(0, 0, _st);
	return _error_message;
}

void DB_StatementCore_ODBC::begin_transaction()
{
	close();

	DB_Connection *db = get_connection();

	HSTMT hstmt = 0;
	SQLRETURN rc;
	rc = SQLAllocStmt(DBHANDLE(db->get_handle()), &hstmt);
	if (!SQL_SUCCEEDED(rc)) {
		throw DBEXCEPTION(get_odbc_error_message(0, DBHANDLE(db->get_handle()), 0));
	}
	_st = hstmt;
}

void DB_StatementCore_ODBC::finalize()
{
	// nothing to do
}

void DB_StatementCore_ODBC::close()
{
	if (_st) {
		SQLFreeStmt(_st, SQL_DROP);
		_st = 0;
	}
}


void DB_StatementCore_ODBC::bind_parameter(int column, DBVARIANT *param)
{
	SQLSMALLINT ctype = 0;
	SQLSMALLINT dtype = 0;
	switch (param->datatype) {
	case DBVARIANT::DT_INTEGER:
		ctype = SQL_C_LONG;
		dtype = SQL_INTEGER;
		break;
	case DBVARIANT::DT_TINYINT:
		ctype = SQL_C_CHAR;
		dtype = SQL_TINYINT;
		break;
	case DBVARIANT::DT_STRING:
		ctype = SQL_C_CHAR;
		dtype = SQL_VARCHAR;
		break;
	case DBVARIANT::DT_DATETIME:
		ctype = SQL_C_TIMESTAMP;
		dtype = SQL_TIMESTAMP;
		break;
	case DBVARIANT::DT_VOID:
		assert(0);
		return;
	default:
		break;
	}
	assert(param->buffer.size() > 0);

	SQLRETURN rc;
	rc = SQLBindParameter(_st, column + 1, SQL_PARAM_INPUT, ctype, dtype, 0, 0, (SQLPOINTER)&param->buffer[0], param->length.odbc, &param->length.odbc);
	if (!SQL_SUCCEEDED(rc)) {
		throw DBEXCEPTION(get_error_message());
	}
}

void DB_StatementCore_ODBC::prepare(const char *sql)
{
	if (_st == 0) {
		fprintf(stderr, "warning: statment handle already closed\n");
		return;
	}
	{
		SQLRETURN rc;
		HSTMT hstmt = get_odbc_stmt();
		SQLFreeStmt(hstmt, SQL_CLOSE);
		rc = SQLPrepareA(hstmt, (SQLCHAR *)sql, SQL_NTS);
		if (!SQL_SUCCEEDED(rc)) {
			throw DBEXCEPTION(get_error_message());
		}
	}
}

void DB_StatementCore_ODBC::execute()
{
	if (_st == 0) {
		fprintf(stderr, "warning: statment handle already closed\n");
		return;
	}
	{
		SQLRETURN rc;
		rc = SQLExecute(_st);
		if (!SQL_SUCCEEDED(rc)) {
			throw DBEXCEPTION(get_error_message());
		}
	}
}

void DB_StatementCore_ODBC::unbind()
{
}

bool DB_StatementCore_ODBC::fetch(std::vector<DBVARIANT> *result)
{
	SQLRETURN rc;

	rc = SQLFetch(_st);
	if (rc == SQL_NO_DATA) {
		return false;
	}

	if (!result) {
		return true;
	}

	SQLSMALLINT i, n = 0;
	rc = SQLNumResultCols(_st, &n);
	if (!SQL_SUCCEEDED(rc)) {
		return false;
	}
	result->resize(n);
	for (i = 0; i < n; i++) {
		result->at(i).setup(DBMODE_ODBC);
		SQLSMALLINT type = 0;
		rc = SQLDescribeCol(_st, i + 1, 0, 0, 0, &type, 0, 0, 0);
		if (!SQL_SUCCEEDED(rc)) {
			return false;
		}
		SQLSMALLINT ctype = 0;
		switch (type) {
		case SQL_INTEGER:
		case SQL_SMALLINT:
			result->at(i).assign_integer(0);
			ctype = SQL_C_LONG;
			break;
		case SQL_FLOAT:
		case SQL_REAL:
		case SQL_DOUBLE:
		case SQL_NUMERIC:
		case SQL_DECIMAL:
			result->at(i).assign_double(0);
			ctype = SQL_C_DOUBLE;
			break;
		case SQL_CHAR:
		case SQL_VARCHAR:
			result->at(i).assign_string(0, 1024);
			ctype = SQL_C_CHAR;
			break;
		case SQL_DATETIME:
		case SQL_TYPE_DATE:
		case SQL_TYPE_TIME:
		case SQL_TYPE_TIMESTAMP:
			result->at(i).assign_datetime((time_t)0);
			ctype = SQL_C_TIMESTAMP;
			break;
		default:
			result->at(i).assign_string(0);
			break;
		}
		SQLINTEGER buflen = (SQLINTEGER)result->at(i).buffer.size();
		if (buflen > 0) {
			SQLPOINTER bufptr = (SQLPOINTER)&result->at(i).buffer[0];
			rc = SQLGetData(_st, i + 1, ctype, bufptr, buflen, 0);
			if (!SQL_SUCCEEDED(rc)) {
				return false;
			}
		}
	}
	return true;
}

#else // #ifdef DB_SUPPORT_ODBC

#ifdef WIN32
#include <winsock2.h>
#endif

#include "database.h"

DB_Abstract_DriverCore *new_DB_DriverCore_ODBC()
{
	return 0;
}

DB_Abstract_StatementCore *new_DB_StatementCore_ODBC(DB_Statement *s)
{
	return 0;
}

#endif

