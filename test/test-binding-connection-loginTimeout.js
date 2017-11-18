var common = require('./common')
  , odbc = require('../')
  , db = new odbc.ODBC()
  , assert = require('assert');

db.createConnection(function (err, conn) {
  //loginTimeout should be 5 by default as set in C++
  assert.equal(conn.loginTimeout, 5);

  //test the setter and getter
  conn.loginTimeout = 1234;
  assert.equal(conn.loginTimeout, 1234);

  //set the time out to something small
  conn.loginTimeout = 1;
  assert.equal(conn.loginTimeout, 1);

  conn.open(common.connectionString, function (err) {
    //TODO: it would be nice if we could somehow
    //force a timeout to occurr, but most testing is
    //done locally and it's hard to get a local server
    //to not accept a connection within one second...

    console.log(err);
    conn.close(function () {

    });
  });
});
