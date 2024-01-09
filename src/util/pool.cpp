// SPDX-License-Identifier: GPL-2.0-or-later
#include "pool.h"
#include <cstdint>
#include <cassert>

namespace Inkscape {
namespace Util {

// Round up x to the next multiple of m.
static std::byte *roundup(std::byte *x, std::size_t m)
{
    auto y = reinterpret_cast<uintptr_t>(x);
    y = ((y - 1) / m + 1) * m;
    return reinterpret_cast<std::byte*>(y);
}

std::byte *Pool::allocate(std::size_t size, std::size_t alignment)
{
    auto a = roundup(cur, alignment);
    auto b = a + size;

    if (b <= end) {
        cur = b;
        return a;
    }

    cursize = std::max(nextsize, size + alignment - 1);
    buffers.emplace_back(std::make_unique<std::byte[]>(cursize));
    // buffers.emplace_back(std::make_unique_for_overwrite<std::byte[]>(cursize)); // Todo: C++20.
    resetblock();
    nextsize = cursize * 3 / 2;

    a = roundup(cur, alignment);
    b = a + size;

    assert(b <= end);
    cur = b;
    return a;
};

void Pool::free_all() noexcept
{
    if (buffers.empty()) return;
    if (buffers.size() > 1) {
        buffers.front() = std::move(buffers.back());
        buffers.resize(1);
    }
    resetblock();
}

void Pool::movefrom(Pool &other) noexcept
{
    buffers = std::move(other.buffers);
    cur = other.cur;
    end = other.end;
    cursize = other.cursize;
    nextsize = other.nextsize;

    other.buffers.clear();
    other.cur = nullptr;
    other.end = nullptr;
    other.cursize = 0;
    other.nextsize = 2;
}

void Pool::resetblock() noexcept
{
    assert(!buffers.empty());
    cur = buffers.back().get();
    end = cur + cursize;
}

} // namespace Util
} // namespace Inkscape
