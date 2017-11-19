/*
  Copyright (c) 2013, Dan VerWeire<dverweire@gmail.com>

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

#ifndef _SRC_ODBC_RESULT_H
#define _SRC_ODBC_RESULT_H

#include <nan.h>

class ODBCResult : public Nan::ObjectWrap {
  public:
   static Nan::Persistent<String> OPTION_FETCH_MODE;
   static Nan::Persistent<String> OPTION_INCLUDE_METADATA;
   static Nan::Persistent<Function> constructor;
   static void Init(v8::Handle<Object> exports);
   
   void Free();
   
  protected:
    ODBCResult() {};
    
    explicit ODBCResult(HENV hENV, HDBC hDBC, HSTMT hSTMT, bool canFreeHandle): 
      Nan::ObjectWrap(),
      m_hENV(hENV),
      m_hDBC(hDBC),
      m_hSTMT(hSTMT),
      m_canFreeHandle(canFreeHandle) {};
     
    ~ODBCResult();

    //constructor
public:
    static NAN_METHOD(New);

    //async methods
    static NAN_METHOD(Fetch);
protected:
    static void UV_Fetch(uv_work_t* work_req);
    static void UV_AfterFetch(uv_work_t* work_req, int status);

public:
    static NAN_METHOD(FetchAll);
protected:
    static void UV_FetchAll(uv_work_t* work_req);
    static void UV_AfterFetchAll(uv_work_t* work_req, int status);
    
    //sync methods
public:
    static NAN_METHOD(CloseSync);
    static NAN_METHOD(MoreResultsSync);
    static NAN_METHOD(FetchSync);
    static NAN_METHOD(FetchAllSync);
    static NAN_METHOD(GetColumnNamesSync);
    static NAN_METHOD(GetColumnMetadataSync);
    
    //property getter/setters
    static NAN_GETTER(FetchModeGetter);
    static NAN_SETTER(FetchModeSetter);

protected:
    struct fetch_work_data {
      Nan::Callback* cb;
      ODBCResult *objResult;
      SQLRETURN result;
      
      int fetchMode;
      bool includeMetadata;
      int count;
      int errorCount;
      Nan::Persistent<Array> rows;
      Nan::Persistent<Object> objError;
    };
    
    ODBCResult *self(void) { return this; }

  protected:
    HENV m_hENV;
    HDBC m_hDBC;
    HSTMT m_hSTMT;
    bool m_canFreeHandle;
    int m_fetchMode;
    
    uint16_t *buffer;
    int bufferLength;
    Column *columns;
    short colCount;
};



#endif
