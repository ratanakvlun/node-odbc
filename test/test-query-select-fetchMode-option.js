var common = require('./common')
  , odbc = require('../')
  , db = odbc({ fetchMode : odbc.FETCH_ARRAY })
  , assert = require('assert');

process.on('exit', () => {
  if (db.connected) db.closeSync();
});

db.openSync(common.connectionString);
assert.equal(db.connected, true);

db.query("select 1 as COLINT, 'some test' as COLTEXT", function (err, data) {
  assert.equal(err, null);
  assert.deepEqual(data, [[1,'some test']]);
});

db.query({ sql: "select 1 as COLINT, 'some test' as COLTEXT", fetchMode: odbc.FETCH_OBJECT }, function (err, data) {
  assert.equal(err, null);
  assert.deepEqual(data, [{ COLINT: 1, COLTEXT: 'some test' }]);
});
