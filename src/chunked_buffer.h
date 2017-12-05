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

#include <list>

#ifndef _SRC_CHUNKED_BUFFER_H
#define _SRC_CHUNKED_BUFFER_H

#define WIDE_CHAR unsigned short

class Chunk {
public:
  enum Type { Binary, Char, WideChar };

  Chunk(size_t size, Chunk::Type type = Chunk::Type::Binary);
  ~Chunk();

  void clear();
  uint8_t* buffer();
  size_t bufferSize();
  size_t size();

  uint8_t* detach();
  size_t copy(uint8_t* dest, size_t size);
  size_t copyString(uint8_t* dest, size_t size);

private:
  Chunk::Type _type;
  uint8_t* _buffer;
  size_t _size;
};

class ChunkedBuffer {
public:
  ChunkedBuffer();
  ChunkedBuffer(Chunk::Type type);
  ChunkedBuffer(size_t maxSize, Chunk::Type type = Chunk::Type::Binary);
  ~ChunkedBuffer();

  void clear();
  std::list<Chunk*> getChunks();
  size_t bufferSize();
  size_t size();
  bool isFull();

  Chunk* createChunk(size_t size);
  size_t toBinaryBuffer(uint8_t** out);
  size_t toStringBuffer(char** out);
  size_t toWideStringBuffer(WIDE_CHAR** out);

private:
  Chunk::Type _type;
  std::list<Chunk*> _chunks;
  size_t _maxSize;
};

#endif
