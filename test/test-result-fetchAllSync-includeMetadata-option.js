var common = require('./common')
  , odbc = require('../')
  , db = odbc()
  , assert = require('assert');

process.on('exit', () => {
  if (db.connected) db.closeSync();
});

db.openSync(common.connectionString);
assert.equal(db.connected, true);

var result = db.queryResultSync({ sql: "select 1 as COLINT, 'some test' as COLTEXT", includeMetadata: true });
assert.equal(result.includeMetadata, true);

var data = result.fetchAllSync();
assert.equal(data.metadata[0].COLUMN_NAME, 'COLINT');
assert.equal(data.metadata[1].COLUMN_NAME, 'COLTEXT');
assert.deepEqual(data.rows, [{ COLINT: 1, COLTEXT: 'some test' }]);

result.closeSync();
