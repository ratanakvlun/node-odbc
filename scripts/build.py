#
# ISC License
#
# Copyright (c) 2017, Ratanak Lun <ratanakvlun@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

import argparse
import os
import subprocess

archs = [
  'ia32',
  'x64'
]

auto_versions = [
  '4.0.0',
  '5.0.0',
  '6.0.0',
  '7.0.0',
  '8.0.0',
  '9.0.0'
]

parser = argparse.ArgumentParser(
  usage='%(prog)s [-h] [--target=TARGET] [--target_arch=TARGET_ARCH]',
  description='Build and package multiple architecture and Node.js version targets.',
  epilog='Any other options are passed through to node-pre-gyp.'
)
parser.add_argument('--target', help='target Node.js version (>=4.0.0, or all)', metavar='')
parser.add_argument('--target_arch', help='target architecture (ia32, x64, or all)', metavar='')
(args, other_args) = parser.parse_known_args()

build_queue = {}

if args.target_arch == 'all':
  for arch in archs:
    build_queue[arch] = []
else:
  build_queue[args.target_arch] = []

if args.target == 'all':
  for arch in build_queue:
    for ver in auto_versions:
      build_queue[arch].append(ver)
else:
  for arch in build_queue:
    build_queue[arch].append(args.target)

bin_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../node_modules/.bin'))
others = ' '.join(other_args)

for arch in build_queue:
  for ver in build_queue[arch]:
    status = subprocess.call(
      '%s/node-pre-gyp configure build package%s%s%s' % (
        bin_path,
        ' --target=%s' % ver if ver else '',
        ' --target_arch=%s' % arch if arch else '',
        ' %s' % others if others else ''
      ),
      shell=True
    )

    if status != 0:
      parser.exit(status)
