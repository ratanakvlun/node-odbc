var common = require('./common')
  , odbc = require('../')
  , db = new odbc.Database()
  , assert = require('assert')
  , exitCode = 0;

db.openSync(common.connectionString);

common.createTables(db, function (err, data) {
  try {
    db.beginTransactionSync();

    var results = db.querySync('insert into ' + common.tableName + ' (COLINT, COLDATETIME, COLTEXT) VALUES (42, null, null)' );

    db.rollbackTransactionSync();

    results = db.querySync('select * from ' + common.tableName);

    assert.deepEqual(results, []);
  }
  catch (e) {
    console.log('Failed when rolling back');
    console.log(e.stack);
    exitCode = 1
  }

  try {
    //Start a new transaction
    db.beginTransactionSync();

    var result = db.querySync('insert into ' + common.tableName + ' (COLINT, COLDATETIME, COLTEXT) VALUES (42, null, null)' );

    db.commitTransactionSync(); //commit

    result = db.querySync('select * from ' + common.tableName);

    assert.deepEqual(result, [ { COLINT: 42, COLDATETIME: null, COLTEXT: null } ]);
  }
  catch (e) {
    console.log('Failed when committing');
    console.log(e.stack);

    exitCode = 2;
  }

  common.dropTables(db, function (err) {
    db.closeSync();
    process.exit(exitCode);
  });
});
