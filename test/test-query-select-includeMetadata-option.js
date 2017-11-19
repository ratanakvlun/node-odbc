var common = require('./common')
  , odbc = require('../')
  , db = odbc()
  , assert = require('assert');

process.on('exit', () => {
  if (db.connected) db.closeSync();
});

db.openSync(common.connectionString);
assert.equal(db.connected, true);

db.query({ sql: "select 1 as COLINT, 'some test' as COLTEXT", includeMetadata: true }, function (err, data) {
  assert.equal(err, null);
  assert.equal(data.metadata[0].COLUMN_NAME, 'COLINT');
  assert.equal(data.metadata[1].COLUMN_NAME, 'COLTEXT');
  assert.deepEqual(data.rows, [{ COLINT: 1, COLTEXT: 'some test' }]);
});
