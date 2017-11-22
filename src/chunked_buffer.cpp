/*
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

#include "chunked_buffer.h"

Chunk::Chunk(size_t size, Chunk::Type type) {
  this->_type = type;
  this->_buffer = NULL;

  if (type == Chunk::Type::WideChar) {
    size = size > 0 && size < sizeof(wchar_t) * 2 ? sizeof(wchar_t) * 2 : size;
    size = size > size % sizeof(wchar_t) ? size - size % sizeof(wchar_t) : 0;
  } else if (type == Chunk::Type::Char) {
    size = size > 0 && size < sizeof(char) * 2 ? sizeof(char) * 2 : size;
  }

  if (size > 0) {
    this->_buffer = (uint8_t*) malloc(size);
  }

  if (this->_buffer) {
    this->_size = size;
  } else {
    this->_size = 0;
  }
}

Chunk::~Chunk() {
  this->clear();
}

void Chunk::clear() {
  if (this->_buffer) {
    free(this->_buffer);
    this->_buffer = NULL;
  }
  this->_size = 0;
}

uint8_t* Chunk::buffer() {
  return this->_buffer;
}

size_t Chunk::bufferSize() {
  return this->_size;
}

size_t Chunk::size() {
  size_t nullBytes = 0;

  if (this->_type == Chunk::Type::WideChar) {
    nullBytes = sizeof(wchar_t);
  } else if (this->_type == Chunk::Type::Char) {
    nullBytes = sizeof(char);
  }

  return this->_size > nullBytes ? this->_size - nullBytes : 0;
}

uint8_t* Chunk::detach() {
  uint8_t* buffer = this->_buffer;

  this->_buffer = NULL;
  this->clear();

  return buffer;
}

size_t Chunk::copy(uint8_t* dest, size_t size) {
  size_t count = size < this->_size ? size : this->_size;
  memcpy(dest, this->_buffer, count);
  return count;
}

size_t Chunk::copyString(uint8_t* dest, size_t size) {
  size_t maxSize = this->size();
  size_t count = size;

  if (this->_type == Chunk::Type::WideChar) {
    count = size > sizeof(wchar_t) ? size - sizeof(wchar_t) : 0;
    count = count > count % sizeof(wchar_t) ? count - count % sizeof(wchar_t) : 0;
  } else if (this->_type == Chunk::Type::Char) {
    count = size > sizeof(char) ? size - sizeof(char) : 0;
  }

  count = count <= maxSize ? count : maxSize;
  memcpy(dest, this->_buffer, count);

  if (count > 0) {
    if (this->_type == Chunk::Type::WideChar) {
      memset(&dest[count], NULL, sizeof(wchar_t));
    } else if (this->_type == Chunk::Type::Char) {
      memset(&dest[count], NULL, sizeof(char));
    }
  }

  return count;
}

ChunkedBuffer::ChunkedBuffer() {
  this->_maxSize = SIZE_MAX;
  this->_type = Chunk::Type::Binary;
};

ChunkedBuffer::ChunkedBuffer(Chunk::Type type) {
  this->_maxSize = SIZE_MAX;
  this->_type = type;

  if (type == Chunk::Type::WideChar) {
    this->_maxSize = this->_maxSize - this->_maxSize % sizeof(wchar_t);
  }
};

ChunkedBuffer::ChunkedBuffer(size_t maxSize, Chunk::Type type) {
  this->_maxSize = maxSize;
  this->_type = type;

  if (type == Chunk::Type::WideChar) {
    this->_maxSize = this->_maxSize > 0 && this->_maxSize < sizeof(wchar_t) ? sizeof(wchar_t) : this->_maxSize;
    this->_maxSize = this->_maxSize > this->_maxSize % sizeof(wchar_t) ? this->_maxSize - this->_maxSize % sizeof(wchar_t) : 0;
  }
};

ChunkedBuffer::~ChunkedBuffer() {
  this->clear();
};

void ChunkedBuffer::clear() {
  for (std::list<Chunk*>::iterator it = this->_chunks.begin(); it != this->_chunks.end(); it++) {
    (*it)->clear();
    delete (*it);
  }
  this->_chunks.clear();
};

size_t ChunkedBuffer::bufferSize() {
  size_t size = 0;
  for (std::list<Chunk*>::iterator it = this->_chunks.begin(); it != this->_chunks.end(); it++) {
    size += (*it)->bufferSize();
  }
  return size;
}

size_t ChunkedBuffer::size() {
  size_t size = 0;
  for (std::list<Chunk*>::iterator it = this->_chunks.begin(); it != this->_chunks.end(); it++) {
    size += (*it)->size();
  }
  return size;
}

std::list<Chunk*> ChunkedBuffer::getChunks() {
  return this->_chunks;
}

Chunk* ChunkedBuffer::createChunk(size_t size) {
  size_t currentSize = this->size();
  if (size > this->_maxSize - currentSize) { size = this->_maxSize - currentSize; }
  if (size == 0) { return NULL; }

  Chunk* chunk = new Chunk(size, this->_type);
  if (chunk->buffer() == NULL) {
    delete chunk;
    return NULL;
  }

  this->_chunks.push_back(chunk);
  return chunk;
}

size_t ChunkedBuffer::toBinaryBuffer(uint8_t** out) {
  size_t expectedSize = this->bufferSize();
  if (expectedSize == 0) {
    *out = NULL;
    return 0;
  }

  uint8_t* buffer = (uint8_t*) malloc(expectedSize);
  if (buffer == NULL) {
    *out = NULL;
    return 0;
  }

  size_t currentSize = 0;
  uint8_t* ptr = buffer;

  for (std::list<Chunk*>::iterator it = this->_chunks.begin(); it != this->_chunks.end(); it++) {
    currentSize += (*it)->copy(ptr, (*it)->bufferSize());
    ptr += (*it)->bufferSize();
  }

  *out = buffer;
  return currentSize;
}

size_t ChunkedBuffer::toStringBuffer(char** out) {
  size_t expectedSize = this->bufferSize();
  if (expectedSize == 0) {
    *out = NULL;
    return 0;
  }

  if (expectedSize < SIZE_MAX) { expectedSize += sizeof(char); }

  uint8_t* buffer = (uint8_t*) malloc(expectedSize);
  if (buffer == NULL) {
    *out = NULL;
    return 0;
  }

  size_t currentSize = 0;
  size_t size;
  uint8_t* ptr = buffer;

  for (std::list<Chunk*>::iterator it = this->_chunks.begin(); it != this->_chunks.end(); it++) {
    size = strnlen((const char*) (*it)->buffer(), (*it)->bufferSize());
    currentSize += (*it)->copy(ptr, size);
    ptr += size;
  }

  if (currentSize < expectedSize) {
    memset(&buffer[currentSize], NULL, sizeof(char));
  } else {
    memset(&buffer[expectedSize-sizeof(char)], NULL, sizeof(char));
  }

  *out = (char*) buffer;
  return currentSize;
}

size_t ChunkedBuffer::toWideStringBuffer(wchar_t** out) {
  size_t expectedSize = this->bufferSize();
  if (expectedSize == 0) {
    *out = NULL;
    return 0;
  }

  expectedSize = expectedSize - expectedSize % sizeof(wchar_t);
  if (expectedSize < SIZE_MAX - sizeof(wchar_t)) { expectedSize += sizeof(wchar_t); }

  uint8_t* buffer = (uint8_t*) malloc(expectedSize);
  if (buffer == NULL) {
    *out = NULL;
    return 0;
  }

  size_t currentSize = 0;
  size_t size;
  uint8_t* ptr = buffer;

  for (std::list<Chunk*>::iterator it = this->_chunks.begin(); it != this->_chunks.end(); it++) {
    size = wcsnlen((const wchar_t*) (*it)->buffer(), (*it)->bufferSize() / sizeof(wchar_t)) * sizeof(wchar_t);
    currentSize += (*it)->copy(ptr, size);
    ptr += size;
  }

  if (currentSize < expectedSize) {
    memset(&buffer[currentSize], NULL, sizeof(wchar_t));
  } else {
    memset(&buffer[expectedSize-sizeof(wchar_t)], NULL, sizeof(wchar_t));
  }

  *out = (wchar_t*) buffer;
  return currentSize;
}
