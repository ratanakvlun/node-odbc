var common = require('./common')
  , odbc = require('../')
  , db = odbc()
  , assert = require('assert');

process.on('exit', () => {
  if (db.connected) db.closeSync();
});

db.openSync(common.connectionString);
assert.equal(db.connected, true);

var result = db.queryResultSync({ sql: "select 1 as COLINT, 'some test' as COLTEXT" });
var metadata = result.getColumnMetadataSync();

assert.equal(metadata[0].COLUMN_NAME, 'COLINT');
assert.equal(metadata[1].COLUMN_NAME, 'COLTEXT');

result.closeSync();
