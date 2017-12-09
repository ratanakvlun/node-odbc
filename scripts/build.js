/*
  ISC License

  Copyright (c) 2017, Ratanak Lun <ratanakvlun@gmail.com>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

'use strict';

let path = require('path');
let argparse = require('argparse');
let cp = require('child_process');

let archs = [
  'ia32',
  'x64'
];

let versions = [
  '4.0.0',
  '5.0.0',
  '6.0.0',
  '7.0.0',
  '8.0.0',
  '9.0.0'
];

let parser = new argparse.ArgumentParser({
  usage: '%(prog)s [-h] [--target=TARGET] [--target_arch=TARGET_ARCH]',
  description: 'Build multiple architecture and Node.js version targets.',
  epilog: 'Any other options are passed through to node-pre-gyp.'
});

parser.addArgument('--target', { help: 'target Node.js version (>=4.0.0, or all)', metavar: '' });
parser.addArgument('--target_arch', { help: 'target architecture (ia32, x64, or all)', metavar: '' });

let groups = parser.parseKnownArgs();
let args = groups[0];
let otherArgs = groups[1];

let buildQueue = {};

if (args.target_arch === 'all') {
  for (let i in archs) {
    buildQueue[archs[i]] = [];
  }
} else {
  buildQueue[args.target_arch || ''] = [];
}

if (args.target === 'all') {
  for (let arch in buildQueue) {
    for (let i in versions) {
      buildQueue[arch].push(versions[i]);
    }
  }
} else {
  for (let arch in buildQueue) {
    buildQueue[arch].push(args.target || '');
  }
}

let others = otherArgs.join(' ');
let binPath = path.resolve('./node_modules/.bin');

for (let arch in buildQueue) {
  for (let i in buildQueue[arch]) {
    let ver = buildQueue[arch][i];

    let cmdStr = [
      `${ binPath }/node-pre-gyp configure build`,
      ver ? ` --target=${ ver }` : '',
      arch ? ` --target_arch=${ arch }` : '',
      others ? ` ${ others }` : ''
    ].join('');

    try {
      cp.execSync(cmdStr, { stdio: 'inherit' });
    } catch (err) {
      parser.exit(err.status);
    }
  }
}
