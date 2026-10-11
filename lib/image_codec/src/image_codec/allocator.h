#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void* frame_image_malloc(size_t size);
void* frame_image_calloc(size_t count, size_t size);
void* frame_image_realloc(void* pointer, size_t size);
void frame_image_free(void* pointer);
#ifdef __cplusplus
}
#endif
