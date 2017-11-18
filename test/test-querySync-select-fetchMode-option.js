var common = require('./common')
  , odbc = require('../')
  , db = odbc({ fetchMode : odbc.FETCH_ARRAY })
  , assert = require('assert');

process.on('exit', () => {
  if (db.connected) db.closeSync();
});

db.openSync(common.connectionString);
assert.equal(db.connected, true);

var data = db.querySync("select 1 as COLINT, 'some test' as COLTEXT");
assert.deepEqual(data, [[1,'some test']]);

data = db.querySync({ sql: "select 1 as COLINT, 'some test' as COLTEXT", fetchMode: odbc.FETCH_OBJECT });
assert.deepEqual(data, [{ COLINT: 1, COLTEXT: 'some test' }]);
