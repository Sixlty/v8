// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_LOCAL_HANDLES_INL_H_
#define V8_HANDLES_LOCAL_HANDLES_INL_H_

#include "src/execution/local-isolate.h"
#include "src/handles/local-handles.h"
#include "src/sanitizer/msan.h"

namespace v8 {
namespace internal {

// static
V8_INLINE Address* LocalHandleScope::GetHandle(LocalHeap* local_heap,
                                               Address value) {
  LocalHandles* handles = local_heap->handles();
  Address* result = handles->scope_.next;
  if (result == handles->scope_.limit) {
    result = handles->AddBlock();
  }
  DCHECK_LT(result, handles->scope_.limit);
  handles->scope_.next++;
  *result = value;
  return result;
}

LocalHandleScope::LocalHandleScope(LocalIsolate* local_isolate)
    : LocalHandleScope(local_isolate->heap()) {}

LocalHandleScope::LocalHandleScope(LocalHeap* local_heap) {
  LocalHandles* handles = local_heap->handles();
  local_heap_ = local_heap;
  prev_next_ = handles->scope_.next;
  prev_limit_ = handles->scope_.limit;
  handles->scope_.level++;
}

LocalHandleScope::~LocalHandleScope() {
  if (local_heap_ == nullptr) return;
  CloseScope(local_heap_, prev_next_, prev_limit_);
}

template <typename T>
Handle<T> LocalHandleScope::CloseAndEscape(Handle<T> handle_value) {
  HandleScopeData* current = &local_heap_->handles()->scope_;
  T value = *handle_value;
  // Throw away all handles in the current scope.
  CloseScope(local_heap_, prev_next_, prev_limit_);
  // Allocate one handle in the parent scope.
  DCHECK(current->level > current->sealed_level);
  Handle<T> result(value, local_heap_);
  // Reinitialize the current scope (so that it's ready
  // to be used or closed again).
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
  return result;
}

void LocalHandleScope::CloseScope(LocalHeap* local_heap, Address* prev_next,
                                  Address* prev_limit) {
  LocalHandles* handles = local_heap->handles();
  Address* old_limit = handles->scope_.limit;

  handles->scope_.next = prev_next;
  handles->scope_.limit = prev_limit;
  handles->scope_.level--;

  if (old_limit != handles->scope_.limit) {
    handles->RemoveUnusedBlocks();
    old_limit = handles->scope_.limit;
  }

#ifdef ENABLE_HANDLE_ZAPPING
  LocalHandles::ZapRange(handles->scope_.next, old_limit);
#endif

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(
      handles->scope_.next,
      static_cast<size_t>(reinterpret_cast<Address>(old_limit) -
                          reinterpret_cast<Address>(handles->scope_.next)));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_LOCAL_HANDLES_INL_H_
