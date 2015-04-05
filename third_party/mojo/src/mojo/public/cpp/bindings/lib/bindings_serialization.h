// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_H_

#include <vector>

#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

class BoundsChecker;

// Please note that this is a different value than |mojo::kInvalidHandleValue|,
// which is the "decoded" invalid handle.
const MojoHandle kEncodedInvalidHandleValue = static_cast<MojoHandle>(-1);

size_t Align(size_t size);
char* AlignPointer(char* ptr);

bool IsAligned(const void* ptr);

// Pointers are encoded as relative offsets. The offsets are relative to the
// address of where the offset value is stored, such that the pointer may be
// recovered with the expression:
//
//   ptr = reinterpret_cast<char*>(offset) + *offset
//
// A null pointer is encoded as an offset value of 0.
//
void EncodePointer(const void* ptr, uint64_t* offset);
// Note: This function doesn't validate the encoded pointer value.
const void* DecodePointerRaw(const uint64_t* offset);

// Note: This function doesn't validate the encoded pointer value.
template <typename T>
inline void DecodePointer(const uint64_t* offset, T** ptr) {
  *ptr = reinterpret_cast<T*>(const_cast<void*>(DecodePointerRaw(offset)));
}

// Checks whether decoding the pointer will overflow and produce a pointer
// smaller than |offset|.
bool ValidateEncodedPointer(const uint64_t* offset);

// Handles are encoded as indices into a vector of handles. These functions
// manipulate the value of |handle|, mapping it to and from an index.
void EncodeHandle(Handle* handle, std::vector<Handle>* handles);
// Note: This function doesn't validate the encoded handle value.
void DecodeHandle(Handle* handle, std::vector<Handle>* handles);

// The following 2 functions are used to encode/decode all objects (structs and
// arrays) in a consistent manner.

template <typename T>
inline void Encode(T* obj, std::vector<Handle>* handles) {
  if (obj->ptr)
    obj->ptr->EncodePointersAndHandles(handles);
  EncodePointer(obj->ptr, &obj->offset);
}

// Note: This function doesn't validate the encoded pointer and handle values.
template <typename T>
inline void Decode(T* obj, std::vector<Handle>* handles) {
  DecodePointer(&obj->offset, &obj->ptr);
  if (obj->ptr)
    obj->ptr->DecodePointersAndHandles(handles);
}

// Validates that |data| contains a valid struct header, in terms of alignment
// and size (i.e., the |num_bytes| field of the header is sufficient for storing
// the header itself). Besides, it checks that the memory range
// [data, data + num_bytes) is not marked as occupied by other objects in
// |bounds_checker|. On success, the memory range is marked as occupied.
// Note: Does not verify |version| or that |num_bytes| is correct for the
// claimed version.
bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        BoundsChecker* bounds_checker);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDINGS_SERIALIZATION_H_