var common = require('./common')
  , odbc = require('../')
  , db = new odbc.Database();

db.openSync(common.connectionString);
common.createTables(db, function (err, data, morefollowing) {
  console.log(arguments);
  db.closeSync();
});
