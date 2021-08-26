#pragma once

// aligned_malloc: Implement/declare linux equivalents here!
#if !defined(_MSC_VER)
#if defined(__USE_ISOC11) && !defined(ASAN_WORKAROUND) // not supported yet on gcc 4.9
	#define _aligned_malloc(size, a) aligned_alloc(a, size)
#else
static inline void * _aligned_malloc(size_t size, size_t align)
{
    void *result = 0;
    posix_memalign(&result, align, size);
    return result;
}
#endif
extern void *__fastcall pcsx2_aligned_realloc(void *handle, size_t new_size, size_t align, size_t old_size);
#define _aligned_free(pmem) (free((pmem)))
#else
#define pcsx2_aligned_realloc(handle, new_size, align, old_size) \
    _aligned_realloc(handle, new_size, align)
#endif
