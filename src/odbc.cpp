/*
  ISC License

  Copyright (c) 2017, Ratanak Lun <ratanakvlun@gmail.com>
  Copyright (c) 2013, Dan VerWeire <dverweire@gmail.com>
  Copyright (c) 2010, Lee Smith <notwink@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <string.h>
#include <v8.h>
#include <node.h>
#include <node_version.h>
#include <time.h>
#include <uv.h>

#include "odbc.h"
#include "odbc_connection.h"
#include "odbc_result.h"
#include "odbc_statement.h"

#include "util.h"
#include "chunked_buffer.h"

#ifdef dynodbc
#include "dynodbc.h"
#endif

#ifdef _WIN32
#include "strptime.h"
#endif

using namespace v8;
using namespace node;

uv_mutex_t ODBC::g_odbcMutex;

Nan::Persistent<Function> ODBC::constructor;

void ODBC::Init(v8::Handle<Object> exports) {
  DEBUG_PRINTF("ODBC::Init\n");
  Nan::HandleScope scope;

  Local<FunctionTemplate> constructor_template = Nan::New<FunctionTemplate>(New);

  // Constructor Template
  constructor_template->SetClassName(Nan::New("ODBC").ToLocalChecked());

  // Reserve space for one Handle<Value>
  Local<ObjectTemplate> instance_template = constructor_template->InstanceTemplate();
  instance_template->SetInternalFieldCount(1);
  
  // Constants
#if (NODE_MODULE_VERSION < NODE_0_12_MODULE_VERSION)

#else

#endif
  PropertyAttribute constant_attributes = static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  constructor_template->Set(Nan::New<String>("SQL_CLOSE").ToLocalChecked(), Nan::New<Number>(SQL_CLOSE), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_DROP").ToLocalChecked(), Nan::New<Number>(SQL_DROP), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_UNBIND").ToLocalChecked(), Nan::New<Number>(SQL_UNBIND), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_RESET_PARAMS").ToLocalChecked(), Nan::New<Number>(SQL_RESET_PARAMS), constant_attributes);
  constructor_template->Set(Nan::New<String>("SQL_DESTROY").ToLocalChecked(), Nan::New<Number>(SQL_DESTROY), constant_attributes);
  constructor_template->Set(Nan::New<String>("FETCH_ARRAY").ToLocalChecked(), Nan::New<Number>(FETCH_ARRAY), constant_attributes);
  NODE_ODBC_DEFINE_CONSTANT(constructor_template, FETCH_OBJECT);
  NODE_ODBC_DEFINE_CONSTANT(constructor_template, MAX_VALUE_SIZE);
  NODE_ODBC_DEFINE_CONSTANT(constructor_template, MAX_VALUE_CHUNK_SIZE);

  // Prototype Methods
  Nan::SetPrototypeMethod(constructor_template, "createConnection", CreateConnection);
  Nan::SetPrototypeMethod(constructor_template, "createConnectionSync", CreateConnectionSync);

  // Attach the Database Constructor to the target object
  constructor.Reset(constructor_template->GetFunction());
  exports->Set(Nan::New("ODBC").ToLocalChecked(),
               constructor_template->GetFunction());
  
  // Initialize the cross platform mutex provided by libuv
  uv_mutex_init(&ODBC::g_odbcMutex);
}

ODBC::~ODBC() {
  DEBUG_PRINTF("ODBC::~ODBC\n");
  this->Free();
}

void ODBC::Free() {
  DEBUG_PRINTF("ODBC::Free\n");
  if (m_hEnv) {
    uv_mutex_lock(&ODBC::g_odbcMutex);
    
    if (m_hEnv) {
      SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
      m_hEnv = NULL;      
    }

    uv_mutex_unlock(&ODBC::g_odbcMutex);
  }
}

NAN_METHOD(ODBC::New) {
  DEBUG_PRINTF("ODBC::New\n");
  Nan::HandleScope scope;
  ODBC* dbo = new ODBC();
  
  dbo->Wrap(info.Holder());

  dbo->m_hEnv = NULL;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  // Initialize the Environment handle
  int ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &dbo->m_hEnv);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
  
  if (!SQL_SUCCEEDED(ret)) {
    DEBUG_PRINTF("ODBC::New - ERROR ALLOCATING ENV HANDLE!!\n");
    
    Local<Value> objError = ODBC::GetSQLError(SQL_HANDLE_ENV, dbo->m_hEnv);
    
    return Nan::ThrowError(objError);
  }
  
  // Use ODBC 3.x behavior
  SQLSetEnvAttr(dbo->m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
  
  info.GetReturnValue().Set(info.Holder());
}

//void ODBC::WatcherCallback(uv_async_t *w, int revents) {
//  DEBUG_PRINTF("ODBC::WatcherCallback\n");
//  //i don't know if we need to do anything here
//}

/*
 * CreateConnection
 */

NAN_METHOD(ODBC::CreateConnection) {
  DEBUG_PRINTF("ODBC::CreateConnection\n");
  Nan::HandleScope scope;

  Local<Function> cb = info[0].As<Function>();
  Nan::Callback *callback = new Nan::Callback(cb);
  //REQ_FUN_ARG(0, cb);

  ODBC* dbo = Nan::ObjectWrap::Unwrap<ODBC>(info.Holder());
  
  //initialize work request
  uv_work_t* work_req = (uv_work_t *) (calloc(1, sizeof(uv_work_t)));
  
  //initialize our data
  create_connection_work_data* data = 
    (create_connection_work_data *) (calloc(1, sizeof(create_connection_work_data)));

  data->cb = callback;
  data->dbo = dbo;

  work_req->data = data;
  
  uv_queue_work(uv_default_loop(), work_req, UV_CreateConnection, (uv_after_work_cb)UV_AfterCreateConnection);

  dbo->Ref();

  info.GetReturnValue().Set(Nan::Undefined());
}

void ODBC::UV_CreateConnection(uv_work_t* req) {
  DEBUG_PRINTF("ODBC::UV_CreateConnection\n");
  
  //get our work data
  create_connection_work_data* data = (create_connection_work_data *)(req->data);
  
  uv_mutex_lock(&ODBC::g_odbcMutex);

  //allocate a new connection handle
  data->result = SQLAllocHandle(SQL_HANDLE_DBC, data->dbo->m_hEnv, &data->hDBC);
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);
}

void ODBC::UV_AfterCreateConnection(uv_work_t* req, int status) {
  DEBUG_PRINTF("ODBC::UV_AfterCreateConnection\n");
  Nan::HandleScope scope;

  create_connection_work_data* data = (create_connection_work_data *)(req->data);
  
  Nan::TryCatch try_catch;
  
  if (!SQL_SUCCEEDED(data->result)) {
    Local<Value> info[1];
    
    info[0] = ODBC::GetSQLError(SQL_HANDLE_ENV, data->dbo->m_hEnv);
    
    data->cb->Call(1, info);
  }
  else {
    Local<Value> info[2];
    info[0] = Nan::New<External>(data->dbo->m_hEnv);
    info[1] = Nan::New<External>(data->hDBC);
    
    Local<Value> js_result = Nan::New<Function>(ODBCConnection::constructor)->NewInstance(2, info);

    info[0] = Nan::Null();
    info[1] = js_result;

    data->cb->Call(data->dbo->handle(), 2, info);
  }
  
  if (try_catch.HasCaught()) {
      Nan::FatalException(try_catch);
  }

  
  data->dbo->Unref();
  delete data->cb;

  free(data);
  free(req);
}

/*
 * CreateConnectionSync
 */

NAN_METHOD(ODBC::CreateConnectionSync) {
  DEBUG_PRINTF("ODBC::CreateConnectionSync\n");
  Nan::HandleScope scope;

  ODBC* dbo = Nan::ObjectWrap::Unwrap<ODBC>(info.Holder());
   
  HDBC hDBC;
  
  uv_mutex_lock(&ODBC::g_odbcMutex);
  
  //allocate a new connection handle
  SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, dbo->m_hEnv, &hDBC);
  
  if (!SQL_SUCCEEDED(ret)) {
    //TODO: do something!
  }
  
  uv_mutex_unlock(&ODBC::g_odbcMutex);

  Local<Value> params[2];
  params[0] = Nan::New<External>(dbo->m_hEnv);
  params[1] = Nan::New<External>(hDBC);

  Local<Object> js_result = Nan::New<Function>(ODBCConnection::constructor)->NewInstance(2, params);

  info.GetReturnValue().Set(js_result);
}

/*
 * GetColumns
 */

Column* ODBC::GetColumns(SQLHSTMT hStmt, short* colCount) {
  SQLRETURN ret;
  SQLSMALLINT buflen;

  //always reset colCount for the current result set to 0;
  *colCount = 0; 

  //get the number of columns in the result set
  ret = SQLNumResultCols(hStmt, colCount);
  
  if (!SQL_SUCCEEDED(ret)) {
    return new Column[0];
  }
  
  Column *columns = new Column[*colCount];

  for (int i = 0; i < *colCount; i++) {
    //save the index number of this column
    columns[i].index = i + 1;

    //get the estimated size of column name
    ret = SQLColAttribute( hStmt,
                           columns[i].index,
#ifdef STRICT_COLUMN_NAMES
                           SQL_DESC_NAME,
#else
                           SQL_DESC_LABEL,
#endif
                           NULL,
                           NULL,
                           &buflen,
                           NULL);

    SQLSMALLINT estimatedSize = buflen + sizeof(SQLWCHAR);
    columns[i].name = new uint8_t[estimatedSize];
    memset(columns[i].name, NULL, sizeof(SQLWCHAR));

    //get the column name
    ret = SQLColAttribute( hStmt,
                           columns[i].index,
#ifdef STRICT_COLUMN_NAMES
                           SQL_DESC_NAME,
#else
                           SQL_DESC_LABEL,
#endif
                           columns[i].name,
                           estimatedSize,
                           &buflen,
                           NULL);

    //get the estimated size of column type name
    ret = SQLColAttribute( hStmt,
                           columns[i].index,
                           SQL_DESC_TYPE_NAME,
                           NULL,
                           NULL,
                           &buflen,
                           NULL);

    estimatedSize = buflen + sizeof(SQLWCHAR);
    columns[i].typeName = new uint8_t[estimatedSize];
    memset(columns[i].typeName, NULL, sizeof(SQLWCHAR));

    // get the column type name
    ret = SQLColAttribute( hStmt,
                           columns[i].index,
                           SQL_DESC_TYPE_NAME,
                           columns[i].typeName,
                           estimatedSize,
                           &buflen,
                           NULL);

    columns[i].type = 0;
    ret = SQLColAttribute(hStmt,
      columns[i].index,
      SQL_DESC_CONCISE_TYPE,
      NULL,
      0,
      NULL,
      &columns[i].type);

    columns[i].length = 0;
    ret = SQLColAttribute(hStmt,
      columns[i].index,
      SQL_DESC_LENGTH,
      NULL,
      0,
      NULL,
      &columns[i].length);

    columns[i].octetLength = 0;
    ret = SQLColAttribute(hStmt,
      columns[i].index,
      SQL_DESC_OCTET_LENGTH,
      NULL,
      0,
      NULL,
      &columns[i].octetLength);

    columns[i].scale = 0;
    ret = SQLColAttribute(hStmt,
      columns[i].index,
      SQL_DESC_SCALE,
      NULL,
      0,
      NULL,
      &columns[i].scale);

    columns[i].radix = 0;
    ret = SQLColAttribute(hStmt,
      columns[i].index,
      SQL_DESC_NUM_PREC_RADIX,
      NULL,
      0,
      NULL,
      &columns[i].radix);
  }
  
  return columns;
}

/*
 * FreeColumns
 */

void ODBC::FreeColumns(Column* columns, short* colCount) {
  for(int i = 0; i < *colCount; i++) {
      delete [] columns[i].name;
      delete [] columns[i].typeName;
  }

  delete [] columns;
  
  *colCount = 0;
}

/*
 * GetColumnMetadata
 */

Local<Array> ODBC::GetColumnMetadata(Column* columns, short* colCount) {
  Nan::EscapableHandleScope scope;

  Local<Array> columnMetadata = Nan::New<Array>();

  for (int i = 0; i < *colCount; i++) {
    Local<Object> metadataObj = Nan::New<Object>();

    metadataObj->Set(Nan::New<String>("INDEX").ToLocalChecked(),
                     Nan::New(columns[i].index));

#ifdef UNICODE
    metadataObj->Set(Nan::New<String>("COLUMN_NAME").ToLocalChecked(),
                     Nan::New<String>((const uint16_t *) columns[i].name).ToLocalChecked());
#else
    metadataObj->Set(Nan::New<String>("COLUMN_NAME").ToLocalChecked(),
                     Nan::New<String>((const char *) columns[i].name).ToLocalChecked());
#endif

#ifdef UNICODE
    metadataObj->Set(Nan::New<String>("TYPE_NAME").ToLocalChecked(),
                     Nan::New<String>((const uint16_t *) columns[i].typeName).ToLocalChecked());
#else
    metadataObj->Set(Nan::New<String>("TYPE_NAME").ToLocalChecked(),
                     Nan::New<String>((const char *) columns[i].typeName).ToLocalChecked());
#endif

    metadataObj->Set(Nan::New<String>("DATA_TYPE").ToLocalChecked(),
      Nan::New<Number>(columns[i].type));

    metadataObj->Set(Nan::New<String>("COLUMN_SIZE").ToLocalChecked(),
      Nan::New<Number>(columns[i].length));

    metadataObj->Set(Nan::New<String>("BUFFER_LENGTH").ToLocalChecked(),
      Nan::New<Number>(columns[i].octetLength));

    metadataObj->Set(Nan::New<String>("DECIMAL_DIGITS").ToLocalChecked(),
      Nan::New<Number>(columns[i].scale));

    metadataObj->Set(Nan::New<String>("NUM_PREC_RADIX").ToLocalChecked(),
      Nan::New<Number>(columns[i].radix));

    columnMetadata->Set(Nan::New(i),
      metadataObj);
  }

  return scope.Escape(columnMetadata);
}

/*
 * GetColumnValue
 */

void FreeBufferCallback(char* data, void* hint) {
  free(data);
}

Handle<Value> ODBC::GetColumnValue(SQLHSTMT hStmt, Column column,
                                   uint8_t* buffer, int bufferLength,
                                   size_t maxValueSize, size_t valueChunkSize) {
  Nan::EscapableHandleScope scope;

  const char* errHint = "[node-odbc] Error in ODBC::GetColumnValue";
  Local<Value> objError;
  bool hasError = false;

  if (valueChunkSize > MAX_VALUE_CHUNK_SIZE) { valueChunkSize = MAX_VALUE_CHUNK_SIZE; }
  if (valueChunkSize > maxValueSize) { valueChunkSize = maxValueSize; }

  int ret;
  SQLLEN len;
  SQLLEN type = column.type;

  switch (type) {
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_FLOAT:
    case SQL_REAL:
    case SQL_DOUBLE:
    case SQL_BIT:
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_BIGINT:
    case SQL_DATE:
    case SQL_TIME:
    case SQL_TIMESTAMP:
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_GUID:
      break;
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    {
      if (column.octetLength == 0 || column.octetLength > LONG_DATA_THRESHOLD) {
        type = SQL_BINARY;
      }
      break;
    }
    default:
    {
      // Determine how unknown type should be treated
      type = SQL_CHAR;
      if (column.radix != 0) { break; }

      // (column.octetLength != column.length) suggests formatting, thus likely to be char data
      if (column.octetLength == 0 || column.octetLength > LONG_DATA_THRESHOLD || column.octetLength == column.length) {
        type = SQL_BINARY;
      }
    }
  }

  switch (type) {
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    {
      int32_t value;

      ret = SQLGetData(
        hStmt,
        column.index,
        SQL_C_SLONG,
        &value,
        sizeof(value),
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Integer: index=%u name=%s type=%zi len=%zi ret=%i\n",
                   column.index, column.name, column.type, len, ret);

      if (!SQL_SUCCEEDED(ret)) { break; }

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
      }
      return scope.Escape(Nan::New<Integer>(value));
    }
    case SQL_FLOAT:
    case SQL_REAL:
    case SQL_DOUBLE:
    {
      double value;

      ret = SQLGetData(
        hStmt,
        column.index,
        SQL_C_DOUBLE,
        &value,
        sizeof(value),
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Number: index=%u name=%s type=%zi len=%zi ret=%i\n",
                   column.index, column.name, column.type, len, ret);

      if (!SQL_SUCCEEDED(ret)) { break; }

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
      }
      return scope.Escape(Nan::New<Number>(value));
    }
    case SQL_BIT:
    {
      ret = SQLGetData(
        hStmt,
        column.index,
        SQL_C_CHAR,
        buffer,
        bufferLength,
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Bit: index=%u name=%s type=%zi len=%zi ret=%i\n",
                   column.index, column.name, column.type, len, ret);

      if (!SQL_SUCCEEDED(ret)) { break; }

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
      }
      return scope.Escape(Nan::New<Boolean>((*buffer == '0') ? false : true));
    }
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
    {
      ChunkedBuffer cbuffer(maxValueSize);
      SQLLEN totalSize = 0;

      do {
        Chunk* chunk = cbuffer.createChunk(valueChunkSize);
        if (!chunk) {
          if (!cbuffer.isFull()) {
            objError = GetError("[node-odbc] Failed to allocate buffer for column data", "ENOMEM", errHint);
            hasError = true;
          }
          break;
        }

        ret = SQLGetData(
          hStmt,
          column.index,
          SQL_C_BINARY,
          chunk->buffer(),
          chunk->bufferSize(),
          &len);

        DEBUG_PRINTF("ODBC::GetColumnValue - Long Data: index=%u name=%s type=%zi len=%zi ret=%i\n",
                     column.index, column.name, column.type, len, ret);

        if (!SQL_SUCCEEDED(ret)) { break; }
        if (ret == SQL_NO_DATA) { break; }

        if (len == SQL_NULL_DATA) {
          return scope.Escape(Nan::Null());
        }

        if (totalSize == 0 && len > 0) { totalSize = len; };
      } while (ret != SQL_SUCCESS);

      if (hasError || !SQL_SUCCEEDED(ret)) { break; }

      Local<Array> buffers = Nan::New<Array>();
      std::list<Chunk*> chunks = cbuffer.getChunks();

      size_t count = 0;
      size_t currentSize = 0;
      size_t size;
      uint8_t* buf;

      if (totalSize <= 0) { totalSize = cbuffer.bufferSize(); }

      for (std::list<Chunk*>::iterator it = chunks.begin(); it != chunks.end(); it++) {
        size = (*it)->bufferSize();

        if (size > totalSize - currentSize) {
          size = totalSize - currentSize;
        }
        if (size == 0) { continue; }

        if (size == (*it)->bufferSize()) {
          buf = (*it)->detach();
        } else {
          buf = (uint8_t*) malloc(size);
          if (!buf) { break; }
          (*it)->copy(buf, size);
        }

        (*it)->clear();

        buffers->Set(Nan::New((uint32_t) count), Nan::NewBuffer((char*) buf, (size_t) size, FreeBufferCallback, NULL).ToLocalChecked());

        currentSize += size;
        count++;
      }

      return scope.Escape(buffers);
    }
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    {
      ret = SQLGetData(
        hStmt,
        column.index,
        SQL_C_WCHAR,
        buffer,
        bufferLength,
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - Wide String: index=%u name=%s type=%zi len=%zi ret=%i\n",
                   column.index, column.name, column.type, len, ret);

      if (!SQL_SUCCEEDED(ret)) { break; }

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
      }
      return scope.Escape(Nan::New<String>((const uint16_t*)buffer).ToLocalChecked());
    }
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_BIGINT:
    case SQL_DATE:
    case SQL_TIME:
    case SQL_TIMESTAMP:
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_GUID:
    default:
    {
      ret = SQLGetData(
        hStmt,
        column.index,
        SQL_C_CHAR,
        buffer,
        bufferLength,
        &len);

      DEBUG_PRINTF("ODBC::GetColumnValue - String: index=%u name=%s type=%zi len=%zi ret=%i\n",
                   column.index, column.name, column.type, len, ret);

      if (!SQL_SUCCEEDED(ret)) { break; }

      if (len == SQL_NULL_DATA) {
        return scope.Escape(Nan::Null());
      }
      return scope.Escape(Nan::New<String>((const char*)buffer).ToLocalChecked());
    }
  }

  //an error has occured

  if (!hasError) {
    //possible values for ret are SQL_ERROR (-1) and SQL_INVALID_HANDLE (-2)

    //If we have an invalid handle, then stuff is way bad and we should abort
    //immediately. Memory errors are bound to follow as we must be in an
    //inconsisant state.
    assert(ret != SQL_INVALID_HANDLE);

    //Not sure if throwing here will work out well for us but we can try
    //since we should have a valid handle and the error is something we
    //can look into

    objError = ODBC::GetSQLError(
      SQL_HANDLE_STMT,
      hStmt,
      errHint
    );
  }

  Nan::ThrowError(objError);
  return scope.Escape(Nan::Undefined());
}

/*
 * GetRecordTuple
 */

Local<Value> ODBC::GetRecordTuple (SQLHSTMT hStmt,
                                   Column* columns, short* colCount,
                                   uint8_t* buffer, int bufferLength,
                                   size_t maxValueSize, size_t valueChunkSize) {
  Nan::EscapableHandleScope scope;
  
  Local<Object> tuple = Nan::New<Object>();
        
  for(int i = 0; i < *colCount; i++) {
#ifdef UNICODE
    tuple->Set( Nan::New((const uint16_t *) columns[i].name).ToLocalChecked(),
                GetColumnValue( hStmt, columns[i], buffer, bufferLength, maxValueSize, valueChunkSize));
#else
    tuple->Set( Nan::New((const char *) columns[i].name).ToLocalChecked(),
                GetColumnValue( hStmt, columns[i], buffer, bufferLength, maxValueSize, valueChunkSize));
#endif
  }
  
  return scope.Escape(tuple);
}

/*
 * GetRecordArray
 */

Local<Value> ODBC::GetRecordArray (SQLHSTMT hStmt,
                                   Column* columns, short* colCount,
                                   uint8_t* buffer, int bufferLength,
                                   size_t maxValueSize, size_t valueChunkSize) {
  Nan::EscapableHandleScope scope;
  
  Local<Array> array = Nan::New<Array>();
        
  for(int i = 0; i < *colCount; i++) {
    array->Set( Nan::New(i),
                GetColumnValue( hStmt, columns[i], buffer, bufferLength, maxValueSize, valueChunkSize));
  }
  
  return scope.Escape(array);
}

/*
 * GetParametersFromObjectArray
 */
Parameter* ODBC::GetParametersFromObjectArray(Local<Array> objects, int* paramCount) {
  DEBUG_PRINTF("ODBC::GetParametersFromObjectArray\n");

  Local<String> typeKey = Nan::New<String>("type").ToLocalChecked();
  Local<String> idKey = Nan::New<String>("id").ToLocalChecked();
  Local<String> valueKey = Nan::New<String>("value").ToLocalChecked();
  Local<String> optionsKey = Nan::New<String>("options").ToLocalChecked();
  Local<String> lengthKey = Nan::New<String>("length").ToLocalChecked();
  Local<String> precisionKey = Nan::New<String>("precision").ToLocalChecked();
  Local<String> scaleKey = Nan::New<String>("scale").ToLocalChecked();

  Parameter* params = NULL;

  *paramCount = objects->Length();
  if (*paramCount > 0) {
    params = (Parameter*)malloc(*paramCount * sizeof(Parameter));
  }

  for (int i = 0; i < *paramCount; i++) {
    Local<Object> obj = objects->Get(i)->ToObject();

    params[i].ValueType = SQL_C_DEFAULT;
    params[i].ParameterType = SQL_TYPE_NULL;
    params[i].ColumnSize = 0;
    params[i].DecimalDigits = 0;
    params[i].ParameterValuePtr = NULL;
    params[i].BufferLength = 0;
    params[i].StrLen_or_IndPtr = SQL_NULL_DATA;

    Local<Value> value = Nan::Undefined();
    Local<Object> options;

    if (obj->Has(typeKey) && obj->Get(typeKey)->IsObject()) {
      Local<Object> typeObj = obj->Get(typeKey)->ToObject();
      if (typeObj->Has(idKey) && typeObj->Get(idKey)->IsInt32()) {
        params[i].ParameterType = typeObj->Get(idKey)->Int32Value();
      }
    }

    if (obj->Has(valueKey)) {
      value = obj->Get(valueKey);

      if (value->IsInt32()) {
        params[i].ValueType = SQL_C_LONG;
      } else if (value->IsNumber()) {
        params[i].ValueType = SQL_C_DOUBLE;
      } else if (value->IsBoolean()) {
        params[i].ValueType = SQL_C_BIT;
      } else if (value->IsObject() && Buffer::HasInstance(value)) {
        params[i].ValueType = SQL_C_BINARY;
      } else if (value->IsString()) {
        params[i].ValueType = SQL_C_WCHAR;
      } else {
        value = Nan::Undefined();
      }
    }

    if (value->IsUndefined()) { continue; }

    SQLULEN columnSize = 0;
    SQLSMALLINT decimalDigits = 0;

    switch (params[i].ValueType) {
      case SQL_C_LONG:
      {
        int32_t* v = (int32_t*)malloc(sizeof(int32_t));
        *v = value->Int32Value();

        params[i].ParameterValuePtr = v;
        params[i].BufferLength = sizeof(int32_t);
        params[i].StrLen_or_IndPtr = NULL;
        columnSize = 11;
        break;
      }
      case SQL_C_DOUBLE:
      {
        double* v = (double*)malloc(sizeof(double));
        *v = value->NumberValue();

        params[i].ParameterValuePtr = v;
        params[i].BufferLength = sizeof(double);
        params[i].StrLen_or_IndPtr = NULL;
        columnSize = 23;
        break;
      }
      case SQL_C_BIT:
      {
        boolean* v = (boolean*)malloc(sizeof(boolean));
        *v = value->BooleanValue();

        params[i].ParameterValuePtr = v;
        params[i].BufferLength = sizeof(boolean);
        params[i].StrLen_or_IndPtr = NULL;
        columnSize = 1;
        break;
      }
      case SQL_C_BINARY:
      {
        params[i].ParameterValuePtr = Buffer::Data(value);
        params[i].BufferLength = Buffer::Length(value);
        params[i].StrLen_or_IndPtr = params[i].BufferLength;
        columnSize = params[i].BufferLength;
        break;
      }
      case SQL_C_WCHAR:
      {
        Local<String> v = value->ToString();

        switch (params[i].ParameterType) {
          case SQL_CHAR:
          case SQL_VARCHAR:
          case SQL_LONGVARCHAR:
          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY:
            params[i].ValueType = SQL_C_BINARY;
            params[i].BufferLength = v->Utf8Length() + 1;
            params[i].ParameterValuePtr = malloc(params[i].BufferLength);
            v->WriteUtf8((char*)params[i].ParameterValuePtr);
            params[i].StrLen_or_IndPtr = v->Utf8Length();
            columnSize = v->Utf8Length();
            break;
          case SQL_WCHAR:
          case SQL_WVARCHAR:
          case SQL_WLONGVARCHAR:
          default:
            params[i].BufferLength = (v->Length() + 1) * sizeof(uint16_t);
            params[i].ParameterValuePtr = malloc(params[i].BufferLength);
            v->Write((uint16_t*)params[i].ParameterValuePtr);
            params[i].StrLen_or_IndPtr = SQL_NTS;
            columnSize = v->Length();
            break;
        }
        break;
      }
    }

    switch (params[i].ParameterType) {
      case SQL_TIME:
      case SQL_TIMESTAMP:
      case SQL_TYPE_TIME:
      case SQL_TYPE_TIMESTAMP:
        decimalDigits = 7;
        break;
    }

    if (obj->Has(optionsKey) && obj->Get(optionsKey)->IsObject()) {
      options = obj->Get(optionsKey)->ToObject();
    }

    if (!options.IsEmpty()) {
      if (options->Has(lengthKey) && options->Get(lengthKey)->IsUint32()) {
        columnSize = options->Get(lengthKey)->Uint32Value();
      } else if (options->Has(precisionKey) && options->Get(precisionKey)->IsUint32()) {
        columnSize = options->Get(precisionKey)->Uint32Value();
      }

      if (options->Has(scaleKey) && options->Get(scaleKey)->IsInt32()) {
        decimalDigits = options->Get(scaleKey)->Int32Value();
      }
    }

    if (columnSize == 0) { columnSize = 1; }

    switch (params[i].ParameterType) {
      case SQL_CHAR:
      case SQL_BINARY:
        params[i].ColumnSize = columnSize <= LONG_DATA_THRESHOLD ? columnSize : LONG_DATA_THRESHOLD;
        break;
      case SQL_VARCHAR:
      case SQL_LONGVARCHAR:
      case SQL_VARBINARY:
      case SQL_LONGVARBINARY:
        params[i].ColumnSize = columnSize <= LONG_DATA_THRESHOLD ? columnSize : 0;
        break;
      case SQL_WCHAR:
        params[i].ColumnSize = columnSize <= LONG_DATA_THRESHOLD / sizeof(uint16_t) ? columnSize : LONG_DATA_THRESHOLD / sizeof(uint16_t);
        break;
      case SQL_WVARCHAR:
      case SQL_WLONGVARCHAR:
        params[i].ColumnSize = columnSize <= LONG_DATA_THRESHOLD / sizeof(uint16_t) ? columnSize : 0;
        break;
      case SQL_TIMESTAMP:
      case SQL_TYPE_TIMESTAMP:
        params[i].ColumnSize = 27;
      case SQL_TIME:
      case SQL_TYPE_TIME:
        params[i].DecimalDigits = decimalDigits;
        break;
      case SQL_GUID:
        params[i].ColumnSize = 36;
        break;
      case SQL_NUMERIC:
      case SQL_DECIMAL:
      default:
        params[i].ColumnSize = columnSize;
        params[i].DecimalDigits = decimalDigits;
        break;
    }

    DEBUG_PRINTF("ODBC::GetParametersFromObjectArray - parameter=%i valueType=%i parameterType=%i columnSize=%u decimalDigits=%i parameterValuePtr=0x%x bufferLength=%i lengthPtr=0x%x\n",
                 i, params[i].ValueType, params[i].ParameterType, params[i].ColumnSize, params[i].DecimalDigits, (size_t)params[i].ParameterValuePtr, params[i].BufferLength, params[i].StrLen_or_IndPtr);
  }

  return params;
}

/*
 * GetParametersFromArray
 */

Parameter* ODBC::GetParametersFromArray (Local<Array> values, int *paramCount) {
  DEBUG_PRINTF("ODBC::GetParametersFromArray\n");
  *paramCount = values->Length();
  
  Parameter* params = NULL;
  
  if (*paramCount > 0) {
    params = (Parameter *) malloc(*paramCount * sizeof(Parameter));
  }
  
  for (int i = 0; i < *paramCount; i++) {
    Local<Value> value = values->Get(i);
    
    params[i].ColumnSize       = 0;
    params[i].StrLen_or_IndPtr = SQL_NULL_DATA;
    params[i].BufferLength     = 0;
    params[i].DecimalDigits    = 0;

    DEBUG_PRINTF("ODBC::GetParametersFromArray - param[%i].length = %zi\n",
                 i, params[i].StrLen_or_IndPtr);

    if (value->IsString()) {
      Local<String> string = value->ToString();
      
      params[i].ValueType         = SQL_C_TCHAR;
      params[i].ColumnSize        = 0; //SQL_SS_LENGTH_UNLIMITED 
#ifdef UNICODE
      params[i].ParameterType     = SQL_WVARCHAR;
      params[i].BufferLength      = (string->Length() * sizeof(uint16_t)) + sizeof(uint16_t);
#else
      params[i].ParameterType     = SQL_VARCHAR;
      params[i].BufferLength      = string->Utf8Length() + 1;
#endif
      params[i].ParameterValuePtr = malloc(params[i].BufferLength);
      params[i].StrLen_or_IndPtr  = SQL_NTS;//params[i].BufferLength;

#ifdef UNICODE
      string->Write((uint16_t *) params[i].ParameterValuePtr);
#else
      string->WriteUtf8((char *) params[i].ParameterValuePtr);
#endif

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsString(): params[%i] c_type=%i type=%i buffer_length=%zi size=%zi length=%zi value=%s\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr, 
                    (char*) params[i].ParameterValuePtr);
    }
    else if (value->IsNull()) {
      params[i].ValueType = SQL_C_DEFAULT;
      params[i].ParameterType   = SQL_VARCHAR;
      params[i].StrLen_or_IndPtr = SQL_NULL_DATA;

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNull(): params[%i] c_type=%i type=%i buffer_length=%zi size=%zi length=%zi\n",
                   i, params[i].ValueType, params[i].ParameterType,
                   params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr);
    }
    else if (value->IsInt32()) {
      int64_t  *number = new int64_t(value->IntegerValue());
      params[i].ValueType = SQL_C_SBIGINT;
      params[i].ParameterType   = SQL_BIGINT;
      params[i].ParameterValuePtr = number;
      params[i].StrLen_or_IndPtr = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsInt32(): params[%i] c_type=%i type=%i buffer_length=%zi size=%zi length=%zi value=%lld\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr,
                    *number);
    }
    else if (value->IsNumber()) {
      double *number   = new double(value->NumberValue());
      
      params[i].ValueType         = SQL_C_DOUBLE;
      params[i].ParameterType     = SQL_DECIMAL;
      params[i].ParameterValuePtr = number;
      params[i].BufferLength      = sizeof(double);
      params[i].StrLen_or_IndPtr  = params[i].BufferLength;
      params[i].DecimalDigits     = 7;
      params[i].ColumnSize        = sizeof(double);

      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsNumber(): params[%i] c_type=%i type=%i buffer_length=%zi size=%zi length=%zi value=%f\n",
                    i, params[i].ValueType, params[i].ParameterType,
                    params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr,
		                *number);
    }
    else if (value->IsBoolean()) {
      bool *boolean = new bool(value->BooleanValue());
      params[i].ValueType         = SQL_C_BIT;
      params[i].ParameterType     = SQL_BIT;
      params[i].ParameterValuePtr = boolean;
      params[i].StrLen_or_IndPtr  = 0;
      
      DEBUG_PRINTF("ODBC::GetParametersFromArray - IsBoolean(): params[%i] c_type=%i type=%i buffer_length=%zi size=%zi length=%zi\n",
                   i, params[i].ValueType, params[i].ParameterType,
                   params[i].BufferLength, params[i].ColumnSize, params[i].StrLen_or_IndPtr);
    }
  } 
  
  return params;
}

/*
 * CallbackSQLError
 */

Handle<Value> ODBC::CallbackSQLError (SQLSMALLINT handleType,
                                      SQLHANDLE handle, 
                                      Nan::Callback* cb) {
  Nan::EscapableHandleScope scope;
  
  return scope.Escape(CallbackSQLError(
    handleType,
    handle,
    (char *) "[node-odbc] SQL_ERROR",
    cb));
}

Local<Value> ODBC::CallbackSQLError (SQLSMALLINT handleType,
                                      SQLHANDLE handle,
                                      char* message,
                                      Nan::Callback* cb) {
  Nan::EscapableHandleScope scope;
  
  Local<Object> objError = ODBC::GetSQLError(
    handleType, 
    handle, 
    message
  );
  
  Local<Value> info[1];
  info[0] = objError;
  cb->Call(1, info);
  
  return scope.Escape(Nan::Undefined());
}

/*
 * GetSQLError
 */

Local<Object> ODBC::GetSQLError (SQLSMALLINT handleType, SQLHANDLE handle) {
  Nan::EscapableHandleScope scope;
  
  return scope.Escape(GetSQLError(
    handleType,
    handle,
    (char *) "[node-odbc] SQL_ERROR"));
}

Local<Object> ODBC::GetSQLError (SQLSMALLINT handleType, SQLHANDLE handle, const char* message) {
  Nan::EscapableHandleScope scope;
  
  DEBUG_PRINTF("ODBC::GetSQLError : handleType=%i, handle=%p\n", handleType, handle);
  
  Local<Object> objError = Nan::New<Object>();

  int32_t i = 0;
  SQLINTEGER native;
  
  SQLSMALLINT len;
  SQLINTEGER statusRecCount;
  SQLRETURN ret;
  char errorSQLState[14];
  char errorMessage[ERROR_MESSAGE_BUFFER_BYTES];

  ret = SQLGetDiagField(
    handleType,
    handle,
    0,
    SQL_DIAG_NUMBER,
    &statusRecCount,
    SQL_IS_INTEGER,
    &len);

  // Windows seems to define SQLINTEGER as long int, unixodbc as just int... %i should cover both
  DEBUG_PRINTF("ODBC::GetSQLError : called SQLGetDiagField; ret=%i, statusRecCount=%i\n", ret, statusRecCount);

  Local<Array> errors = Nan::New<Array>();
  objError->Set(Nan::New("errors").ToLocalChecked(), errors);
  
  for (i = 0; i < statusRecCount; i++){
    DEBUG_PRINTF("ODBC::GetSQLError : calling SQLGetDiagRec; i=%i, statusRecCount=%i\n", i, statusRecCount);
    
    ret = SQLGetDiagRec(
      handleType, 
      handle,
      (SQLSMALLINT)(i + 1), 
      (SQLTCHAR *) errorSQLState,
      &native,
      (SQLTCHAR *) errorMessage,
      ERROR_MESSAGE_BUFFER_CHARS,
      &len);
    
    DEBUG_PRINTF("ODBC::GetSQLError : after SQLGetDiagRec; i=%i\n", i);

    if (SQL_SUCCEEDED(ret)) {
      DEBUG_PRINTF("ODBC::GetSQLError : errorMessage=%s, errorSQLState=%s\n", errorMessage, errorSQLState);
      
      if (i == 0) {
        // First error is assumed the primary error
        objError->Set(Nan::New("error").ToLocalChecked(), Nan::New(message).ToLocalChecked());
#ifdef UNICODE
        objError->SetPrototype(Exception::Error(Nan::New((uint16_t *)errorMessage).ToLocalChecked()));
        objError->Set(Nan::New("message").ToLocalChecked(), Nan::New((uint16_t *)errorMessage).ToLocalChecked());
        objError->Set(Nan::New("state").ToLocalChecked(), Nan::New((uint16_t *)errorSQLState).ToLocalChecked());
#else
        objError->SetPrototype(Exception::Error(Nan::New(errorMessage).ToLocalChecked()));
        objError->Set(Nan::New("message").ToLocalChecked(), Nan::New(errorMessage).ToLocalChecked());
        objError->Set(Nan::New("state").ToLocalChecked(), Nan::New(errorSQLState).ToLocalChecked());
#endif
        objError->Set(Nan::New("code").ToLocalChecked(), Nan::New(native));
      }

      Local<Object> subError = Nan::New<Object>();

#ifdef UNICODE
      subError->Set(Nan::New("message").ToLocalChecked(), Nan::New((uint16_t *)errorMessage).ToLocalChecked());
      subError->Set(Nan::New("state").ToLocalChecked(), Nan::New((uint16_t *)errorSQLState).ToLocalChecked());
#else
      subError->Set(Nan::New("message").ToLocalChecked(), Nan::New(errorMessage).ToLocalChecked());
      subError->Set(Nan::New("state").ToLocalChecked(), Nan::New(errorSQLState).ToLocalChecked());
#endif
      subError->Set(Nan::New("code").ToLocalChecked(), Nan::New(native));
      errors->Set(Nan::New(i), subError);

    } else if (ret == SQL_NO_DATA) {
      break;
    }
  }

  if (statusRecCount == 0) {
    //Create a default error object if there were no diag records
    objError->Set(Nan::New("error").ToLocalChecked(), Nan::New(message).ToLocalChecked());
    objError->SetPrototype(Exception::Error(Nan::New(message).ToLocalChecked()));
    objError->Set(Nan::New("message").ToLocalChecked(), Nan::New(
      (const char *) "[node-odbc] An error occurred but no diagnostic information was available.").ToLocalChecked());
  }

  return scope.Escape(objError);
}

/*
 * GetError
 */

Local<Object> ODBC::GetError(const char* message, const char* code, const char* hint) {
  Nan::EscapableHandleScope scope;

  Local<Object> objError = Nan::New<Object>();
  objError->SetPrototype(Exception::Error(Nan::New<String>(message).ToLocalChecked()));
  objError->Set(Nan::New<String>("message").ToLocalChecked(), Nan::New<String>(message).ToLocalChecked());

  if (code) {
    objError->Set(Nan::New<String>("code").ToLocalChecked(), Nan::New<String>(code).ToLocalChecked());
  }

  if (hint) {
    objError->Set(Nan::New<String>("error").ToLocalChecked(), Nan::New<String>(hint).ToLocalChecked());
  }

  return scope.Escape(objError);
}

/*
 * GetAllRecordsSync
 */

Local<Array> ODBC::GetAllRecordsSync (HENV hENV, 
                                      HDBC hDBC,
                                      HSTMT hSTMT,
                                      uint8_t* buffer,
                                      int bufferLength,
                                      size_t maxValueSize,
                                      size_t valueChunkSize) {
  DEBUG_PRINTF("ODBC::GetAllRecordsSync\n");
  
  Nan::EscapableHandleScope scope;
  
  Local<Object> objError = Nan::New<Object>();
  
  int count = 0;
  int errorCount = 0;
  short colCount = 0;
  
  Column* columns = GetColumns(hSTMT, &colCount);
  
  Local<Array> rows = Nan::New<Array>();
  
  //loop through all records
  while (true) {
    SQLRETURN ret = SQLFetch(hSTMT);
    
    //check to see if there was an error
    if (ret == SQL_ERROR)  {
      //TODO: what do we do when we actually get an error here...
      //should we throw??
      
      errorCount++;
      
      objError = ODBC::GetSQLError(
        SQL_HANDLE_STMT, 
        hSTMT,
        (char *) "[node-odbc] Error in ODBC::GetAllRecordsSync"
      );
      
      break;
    }
    
    //check to see if we are at the end of the recordset
    if (ret == SQL_NO_DATA) {
      ODBC::FreeColumns(columns, &colCount);
      
      break;
    }

    rows->Set(
      Nan::New(count), 
      ODBC::GetRecordTuple(
        hSTMT,
        columns,
        &colCount,
        buffer,
        bufferLength,
        maxValueSize,
        valueChunkSize)
    );

    count++;
  }
  //TODO: what do we do about errors!?!
  //we throw them
  return scope.Escape(rows);
}

#ifdef dynodbc
NAN_METHOD(ODBC::LoadODBCLibrary) {
  Nan::HandleScope scope;
  
  REQ_STR_ARG(0, js_library);
  
  bool result = DynLoadODBC(*js_library);
  
  info.GetReturnValue().Set((result) ? Nan::True() : Nan::False());
}
#endif

extern "C" void init(v8::Handle<Object> exports) {
#ifdef dynodbc
  exports->Set(Nan::New("loadODBCLibrary").ToLocalChecked(),
        Nan::New<FunctionTemplate>(ODBC::LoadODBCLibrary)->GetFunction());
#endif
  
  ODBC::Init(exports);
  ODBCResult::Init(exports);
  ODBCConnection::Init(exports);
  ODBCStatement::Init(exports);
}

NODE_MODULE(odbc_bindings, init)
