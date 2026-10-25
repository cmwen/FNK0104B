/* Host checks for the vendored engine. No microphone or device claims. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "nd_whistle.h"
static size_t current, peak;
void* whistle_test_alloc(size_t n) {
    size_t* p = malloc(n + sizeof(size_t));
    assert(p); *p = n; current += n;
    if (current > peak) peak = current;
    return p + 1;
}
void whistle_test_free(void* p) {
    if (p) { size_t* q = (size_t*)p - 1; current -= *q; free(q); }
}
int main(int argc, char** argv) {
    float quiet[16000] = {0};
    uint32_t start, end;
    assert(!nd_whistle_find_speech(quiet, 16000, 0.001f, 12, 300, &start, &end));
    for (unsigned i = 4000; i < 10000; ++i) quiet[i] = 0.1f * sinf(i * 0.12f);
    assert(nd_whistle_find_speech(quiet, 16000, 0.001f, 12, 100, &start, &end));
    assert(start < 10000 && end > 4000 && end <= 16000);
    if (argc == 1) return 0;
    FILE* file = fopen(argv[1], "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END));
    long size = ftell(file); assert(size > 0); rewind(file);
    void* blob = malloc(size); assert(blob);
    assert(fread(blob, 1, size, file) == (size_t)size); fclose(file);
    nd_whistle model;
    assert(!nd_whistle_open(&model, blob, size));
    assert(model.depth == 4);
    assert(nd_whistle_cache_head(&model, whistle_test_alloc) > 0);
    const unsigned n = 80000, frames = nd_whistle_frames(&model, n);
    float* clip = whistle_test_alloc(n * sizeof(float));
    float* encoded = whistle_test_alloc(frames * model.d * sizeof(float));
    for (unsigned i = 0; i < n; ++i) clip[i] = 0.02f * sinf(i * 0.12f);
    const size_t baseline = current;
    for (int repeat = 0; repeat < 2; ++repeat) {
        uint32_t ids[96];
        assert(!nd_whistle_encode(&model, clip, n, encoded));
        assert(nd_whistle_decode(&model, encoded, frames, -1, ids, 96) >= 0);
        assert(current == baseline); // Session/encoder scratch must be released.
    }
    printf("five_second_psram_peak=%zu baseline=%zu\n", peak, baseline);
    assert(peak < 6 * 1024 * 1024); // Leaves room for LCD and platform allocations.
    return 0;
}
