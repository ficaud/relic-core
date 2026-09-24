// SPDX-License-Identifier: GPL-3.0-or-later
//
// Route C++ dynamic allocation (operator new/delete) to external PSRAM.
//
// Compiled only when CONFIG_RELIC_QR_DECODE_PSRAM is set. ZXing-cpp allocates
// its internal state (BitMatrix, binarizer buffers, std::string/std::vector)
// through the global operator new; at 640x640 those allocations far exceed the
// small internal libc heap (a few tens of KB once the C++ runtime is linked),
// so on the XIAO we redirect them to the 8 MB PSRAM via Zephyr's shared
// multi-heap. The single-threaded HTTP server means there is no concurrency to
// worry about.

#if defined(CONFIG_RELIC_QR_DECODE_PSRAM)

#include <cstddef>
#include <cstdint>
#include <new>

#include <zephyr/multi_heap/shared_multi_heap.h>

static void *psram_alloc(std::size_t size)
{
    return shared_multi_heap_aligned_alloc(SMH_REG_ATTR_EXTERNAL, 16, size);
}

static void psram_free(void *ptr)
{
    if (ptr != nullptr)
    {
        shared_multi_heap_free(ptr);
    }
}

void *operator new(std::size_t size)
{
    if (void *ptr = psram_alloc(size))
    {
        return ptr;
    }
    throw std::bad_alloc();
}

void *operator new[](std::size_t size)
{
    return ::operator new(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    return psram_alloc(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return psram_alloc(size);
}

void operator delete(void *ptr) noexcept
{
    psram_free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    psram_free(ptr);
}

void operator delete(void *ptr, std::size_t) noexcept
{
    psram_free(ptr);
}

void operator delete[](void *ptr, std::size_t) noexcept
{
    psram_free(ptr);
}

void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
    psram_free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    psram_free(ptr);
}

#endif /* CONFIG_RELIC_QR_DECODE_PSRAM */
