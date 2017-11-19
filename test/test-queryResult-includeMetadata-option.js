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
result.closeSync();

db.queryResult({ sql: "select 1 as COLINT, 'some test' as COLTEXT", includeMetadata: true }, (err, result) => {
  assert.equal(result.includeMetadata, true);
  result.closeSync();
});
