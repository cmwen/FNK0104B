#include <image_codec/allocator.h>
#include <image_codec/image.hpp>
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace {
image_codec::Allocator hooks = {std::malloc, std::realloc, std::free};
size_t allocated = 0;
// Only portable scalar codecs are built; size_t alignment preserves their
// byte/int/pointer alignment on both Xtensa and the native test platform.
struct Header { size_t size; };
}
namespace image_codec {
void setAllocator(Allocator allocator) {
  assert(allocated == 0);
  hooks = allocator;
}
size_t allocatedBytes() { return allocated; }
}
extern "C" void* frame_image_malloc(size_t size) {
  if (!size || size > image_codec::kMaxWorkingBytes - allocated) return nullptr;
  auto* header = static_cast<Header*>(hooks.allocate(sizeof(Header) + size));
  if (!header) return nullptr;
  header->size = size;
  allocated += size;
  return header + 1;
}
extern "C" void frame_image_free(void* pointer) {
  if (!pointer) return;
  auto* header = static_cast<Header*>(pointer) - 1;
  allocated -= header->size;
  hooks.release(header);
}
extern "C" void* frame_image_calloc(size_t count, size_t size) {
  if (!size || count > image_codec::kMaxWorkingBytes / size) return nullptr;
  void* pointer = frame_image_malloc(count * size);
  if (pointer) std::memset(pointer, 0, count * size);
  return pointer;
}
extern "C" void* frame_image_realloc(void* pointer, size_t size) {
  if (!pointer) return frame_image_malloc(size);
  if (!size) { frame_image_free(pointer); return nullptr; }
  auto* header = static_cast<Header*>(pointer) - 1;
  const size_t old_size = header->size;
  if (size > image_codec::kMaxWorkingBytes - (allocated - old_size)) return nullptr;
  auto* replacement = static_cast<Header*>(hooks.reallocate(header, sizeof(Header) + size));
  if (!replacement) return nullptr;
  replacement->size = size;
  allocated = allocated - old_size + size;
  return replacement + 1;
}
