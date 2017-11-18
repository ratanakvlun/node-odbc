var odbc = require('../')
  , openCount = 100
  , start = process.memoryUsage().heapUsed
  , x = 100
  , gc = global.gc;

gc();

start = process.memoryUsage().heapUsed;

for (x = 0; x < openCount; x++ ) {
  (function () {
    new odbc.Database();
  })();
}

gc();

console.log(process.memoryUsage().heapUsed - start);

gc();

for (x = 0; x < openCount; x++ ) {
  (function () {
    new odbc.Database();
  })();
}

gc();

console.log(process.memoryUsage().heapUsed - start);
