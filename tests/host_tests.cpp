// Testes mínimos de host para a lógica independente de plataforma.
#include <cstdio>
#include <cstdlib>

#include "lua/LuaAllocator.h"
#include "net/NetTransport.h"

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::fprintf(stderr, "FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                \
        }                                                                \
    } while (0)

static void TestCappedAllocator() {
    using mtasa::lua::CappedAllocator;
    CappedAllocator a(1024);
    void* p = CappedAllocator::Alloc(&a, nullptr, 5 /* tag de tipo, não tamanho */, 512);
    CHECK(p && a.used() == 512);
    CHECK(CappedAllocator::Alloc(&a, nullptr, 0, 600) == nullptr);  // estoura o teto
    CHECK(a.used() == 512);
    void* q = CappedAllocator::Alloc(&a, p, 512, 2048);  // crescer além do teto falha
    CHECK(q == nullptr && a.used() == 512);
    p = CappedAllocator::Alloc(&a, p, 512, 1024);  // exatamente no teto passa
    CHECK(p && a.used() == 1024 && a.peak() == 1024);
    p = CappedAllocator::Alloc(&a, p, 1024, 16);  // encolher sempre passa
    CHECK(p && a.used() == 16);
    CHECK(CappedAllocator::Alloc(&a, p, 16, 0) == nullptr && a.used() == 0);
}

int main() {
    TestCappedAllocator();
    CHECK(mtasa::net::TransportName() != nullptr);
    std::puts("ok");
    return 0;
}
