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

#ifndef _SRC_UTIL_H
#define _SRC_UTIL_H

#define ALIGN_SIZE(v) ((v) > (v) % sizeof(SQLWCHAR) ? (v) - (v) % sizeof(SQLWCHAR) : 0)

#define CLAMP_SIZE_UNSIGNED(v, max) ((ssize_t)(v) < 0 ? (size_t)(max) : (size_t)(v))
#define CLAMP_SIZE_SIGNED(v) ((ssize_t)(v) < 0 ? -1 : (ssize_t)(v))

#endif
