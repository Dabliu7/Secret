#pragma once
#include <atomic>
#include <cstddef>

namespace mtasa::lua {

// Alocador com teto de memória, no formato lua_Alloc. Um por VM (um por resource),
// para que um script com vazamento receba LUA_ERRMEM em vez de derrubar o processo
// inteiro via low-memory killer.
class CappedAllocator {
public:
    explicit CappedAllocator(size_t limit_bytes) : limit_(limit_bytes) {}

    // Assinatura compatível com lua_Alloc: passar `this` como `ud`.
    static void* Alloc(void* ud, void* ptr, size_t osize, size_t nsize);

    size_t used() const { return used_.load(std::memory_order_relaxed); }
    size_t peak() const { return peak_; }
    size_t limit() const { return limit_; }
    void set_limit(size_t limit_bytes) { limit_ = limit_bytes; }

private:
    std::atomic<size_t> used_{0};
    size_t peak_ = 0;
    size_t limit_;
};

}  // namespace mtasa::lua
