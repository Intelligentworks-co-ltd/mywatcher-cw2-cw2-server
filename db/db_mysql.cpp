#include "../config.h"
#ifdef DB_SUPPORT_MYSQL

#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdlib.h>

#include <mysql.h>

#include "db_mysql.h"
#include <mysql.h>
#include "../common/calendar.h"

DB_Abstract_DriverCore *new_DB_DriverCore_MySQL()
{
	return new DB_DriverCore_MySQL();
}

DB_Abstract_StatementCore *new_DB_StatementCore_MySQL(DB_Statement *s)
{
	return new DB_StatementCore_MySQL(s);
}

///////////////////////////////////////////////////////////////////////////////
// DB_DriverCore_MySQL

void DB_DriverCore_MySQL::init()
{
	if (!mysql_thread_safe()) {
		fprintf(stderr, "mysql_thread_safe failed\n");
		abort();
	}
}

DB_Abstract_StatementCore *DB_DriverCore_MySQL::new_statement_core(DB_Statement *st) const
{
	return new_DB_StatementCore_MySQL(st);
}

void DB_DriverCore_MySQL::connect(DB_Parameter const *dbparam, db_handle_t *handle) const
{
	MYSQL *db;
	db = mysql_init(NULL);
	if (!db) {
		throw DBEXCEPTION("mysql_init failed");
	}
	std::string host = dbparam->host;
	std::string sock;
	{
		char const *p = host.c_str();
		char const *q = strchr(p, ':');
		if (q) {
			host.assign(p, q);
			sock.assign(q + 1);
		}
	}
	mysql_options(db, MYSQL_SET_CHARSET_NAME, "latin1");
	if (!mysql_real_connect(db, host.c_str(), dbparam->user.c_str(), dbparam->pass.c_str(), dbparam->name.c_str(), 0, sock.empty() ? 0 : sock.c_str(), 0)) {
		throw DBEXCEPTION(mysql_error(db));
	}

	handle->value = (void *)db;
}

static inline MYSQL *DBHANDLE(db_handle_t *handle)
{
	return (MYSQL *)handle->value;
}

void DB_DriverCore_MySQL::disconnect(db_handle_t *handle) const
{
	if (DBHANDLE(handle)) {
		mysql_close(DBHANDLE(handle));
	}
}

///////////////////////////////////////////////////////////////////////////////
// DB_StatementCore_MySQL

std::string DB_StatementCore_MySQL::get_error_message()
{
	_error_message = mysql_stmt_error(get_mysql_stmt());
	return _error_message;
}

void DB_StatementCore_MySQL::begin_transaction()
{
	close();

	_st =  mysql_stmt_init(DBHANDLE(get_connection()->get_handle()));
	if (!_st) {
		throw DBEXCEPTION("could not start transaction");
	}
}

void DB_StatementCore_MySQL::finalize()
{
	// nothing to do
}

void DB_StatementCore_MySQL::close()
{
	if (_st) {
		mysql_stmt_free_result(_st);
		mysql_stmt_close(_st);
		_st = 0;
	}
}


void DB_StatementCore_MySQL::bind_parameter(int column, DBVARIANT *param)
{
	if (column >= (int)_mysql_parameters.size()) {
		_mysql_parameters.resize(column + 1);
	}
	MYSQL_BIND *bind = &_mysql_parameters[column];
	bind->is_null = (my_bool *)&param->_isnull;
	switch (param->datatype) {
	case DBVARIANT::DT_INTEGER:
		bind->buffer_type = MYSQL_TYPE_LONG;
		break;
	case DBVARIANT::DT_TINYINT:
		bind->buffer_type = MYSQL_TYPE_TINY;
		break;
	case DBVARIANT::DT_LONGLONG:
		bind->buffer_type = MYSQL_TYPE_LONGLONG;
		break;
	case DBVARIANT::DT_STRING:
		bind->buffer_type = MYSQL_TYPE_STRING;
		break;
	case DBVARIANT::DT_DATETIME:
		bind->buffer_type = MYSQL_TYPE_DATETIME;
		break;
	case DBVARIANT::DT_VOID:
		assert(0);
		return;
	default:
		break;
	}
	if (param->_isnull) {
		param->buffer.resize(1);
		bind->buffer = (void *)&param->buffer[0];
		bind->buffer_length = 0;
		bind->length = &param->length.mysql;
	} else {
		assert(param->buffer.size() > 0);
		bind->buffer = (void *)&param->buffer[0];
		bind->buffer_length = param->length.mysql;
		bind->length = &param->length.mysql;
	}
}

void DB_StatementCore_MySQL::prepare(const char *sql)
{
	if (_st == 0) {
		fprintf(stderr, "warning: statment handle already closed\n");
		return;
	}
	if (::mysql_stmt_prepare(get_mysql_stmt(), sql, (unsigned long)strlen(sql))) {
		throw DBEXCEPTION(get_error_message());
	}

	MYSQL_RES *md = mysql_stmt_result_metadata(get_mysql_stmt());
	if (md) {
		unsigned int i, n;
		n = mysql_num_fields(md);
		_statement->_fields.clear();
		_statement->_fields.resize(n);
		for (i = 0; i < n; i++) {
			MYSQL_FIELD *f = mysql_fetch_field_direct(md, i);
			_statement->_fields[i].name = f->name;
		}
		mysql_free_result(md);
	}

	_mysql_parameters.clear();
}

void DB_StatementCore_MySQL::mysql_stmt_bind_result_(MYSQL_BIND *bnd)
{
	if (::mysql_stmt_bind_result(get_mysql_stmt(), bnd) != 0) {
		throw DBEXCEPTION(get_error_message());
	}
}

void DB_StatementCore_MySQL::execute()
{
	if (_st == 0) {
		fprintf(stderr, "warning: statment handle already closed\n");
		return;
	}
	if (_mysql_parameters.size() > 0) {
		if (::mysql_stmt_bind_param(get_mysql_stmt(), &_mysql_parameters[0]) != 0) {
			throw DBEXCEPTION(get_error_message());
		}
	}
	if (::mysql_stmt_execute(get_mysql_stmt()) != 0) {
		throw DBEXCEPTION(get_error_message());
	}
}

void DB_StatementCore_MySQL::unbind()
{
}

bool DB_StatementCore_MySQL::fetch_row_(std::vector<DBVARIANT> *result)
{
	MYSQL_STMT *st = get_mysql_stmt();
	MYSQL_RES *md = mysql_stmt_result_metadata(st);
	if (!md) {
		fprintf(stderr, "no result set");
		throw DBEXCEPTION(mysql_stmt_error(st));
		//return false;
	}
	unsigned int i, n;
	n = mysql_num_fields(md);
	result->resize(n);
	std::vector<MYSQL_BIND> bindvec(n);
	std::vector<unsigned long> lenvec(n);
	for (i = 0; i < n; i++) {
		result->at(i).setup(DBMODE_MYSQL);

		MYSQL_FIELD *f = mysql_fetch_field_direct(md, i);
		switch (f->type) {

		case MYSQL_TYPE_NULL:
			break;

		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_ENUM:
			result->at(i).assign_integer(0);
			bindvec[i].buffer_type = MYSQL_TYPE_LONG;
			break;
		case MYSQL_TYPE_LONGLONG:
			result->at(i).assign_longlong(0);
			bindvec[i].buffer_type = MYSQL_TYPE_LONGLONG;
			break;
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_NEWDECIMAL:
			result->at(i).assign_double(0);
			bindvec[i].buffer_type = MYSQL_TYPE_DOUBLE;
			break;
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_YEAR:
			result->at(i).assign_datetime((time_t)0);
			bindvec[i].buffer_type = MYSQL_TYPE_DATETIME;
			break;

		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
			result->at(i).assign_string((char const *)0, 1024);
			bindvec[i].buffer_type = MYSQL_TYPE_VAR_STRING;
			break;

		case MYSQL_TYPE_BLOB:
			result->at(i).assign_string((char const *)0, 1024);
			bindvec[i].buffer_type = MYSQL_TYPE_BLOB;
			break;

		//case MYSQL_TYPE_SET:
		//case MYSQL_TYPE_TINY_BLOB:
		//case MYSQL_TYPE_MEDIUM_BLOB:
		//case MYSQL_TYPE_LONG_BLOB:
		//case MYSQL_TYPE_BLOB:
		//case MYSQL_TYPE_GEOMETRY:
		default:
			break;
		}
		if (result->at(i).buffer.size() > 0) {
			//unsigned long len = (unsigned long)result->at(i).buffer.size();
			bindvec[i].buffer = &result->at(i).buffer[0];
			bindvec[i].buffer_length = result->at(i).length.mysql;
			bindvec[i].length = &result->at(i).length.mysql;
		} else {
			bindvec[i].buffer = 0;
			bindvec[i].buffer_length = 0;
			bindvec[i].length = 0;
		}
		bindvec[i].is_null = 0;
	}
	mysql_free_result(md);

	mysql_stmt_bind_result_(&bindvec[0]);

	if (mysql_stmt_fetch(st) != 0) {
		return false;
	}
	return true;
}

bool DB_StatementCore_MySQL::fetch(std::vector<DBVARIANT> *result)
{
	if (!result) {
		if (::mysql_stmt_fetch(get_mysql_stmt()) == 0) {
			return true;
		}
		return false;
	}

	return fetch_row_(result);
}

unsigned long long DB_StatementCore_MySQL::get_affected_rows()
{
	return mysql_stmt_affected_rows(get_mysql_stmt());
}

unsigned long long DB_Statement::x_mysql_insert_id()
{
	return mysql_insert_id(DBHANDLE(&_db->_handle));
}

#else // #ifdef DB_SUPPORT_MYSQL

#include <winsock2.h>
#include "database.h"

DB_Abstract_DriverCore *new_DB_DriverCore_MySQL()
{
	return 0;
}

DB_Abstract_StatementCore *new_DB_StatementCore_MySQL(DB_Statement *s)
{
	return 0;
}

#endif
