var common = require('./common')
  , odbc = require('../')
  , db = new odbc.Database()
  , assert = require('assert');

db.openSync(common.connectionString);
assert.equal(db.connected, true);
var err = null;

try {
  db.querySync('select invalid query');
}
catch (e) {
  console.log(e.stack);

  err = e;
}

db.closeSync();
assert.equal(err.error, '[node-odbc] Error in ODBCConnection::QuerySync');
