var common = require('./common')
  , odbc = require('../')
  , openCount = 100
  , connections = []
  , errorCount = 0;

for (var x = 0; x < openCount; x++ ) {
  var db = new odbc.Database();
  connections.push(db);

  try {
    db.openSync(common.connectionString);
  }
  catch (e) {
    console.log(common.connectionString);
    console.log(e.stack);
    errorCount += 1;
    break;
  }
}

connections.forEach(function (db) {
  db.closeSync();
});

console.log('Done');
process.exit(errorCount);
