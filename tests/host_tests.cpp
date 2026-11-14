// Testes mínimos de host para a lógica independente de plataforma.
#include <cstdio>
#include <cstdlib>

#include "lua/LuaAllocator.h"
#include "mtasa/Hook.h"
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

extern "C" int got_victim_target(int);
extern "C" int got_victim_caller(int);

static int (*orig_target)(int) = nullptr;
static int FakeTarget(int x) { return orig_target(x) + 100; }

static void TestGotHook() {
    CHECK(got_victim_caller(1) == 20);
    const auto module = mtasa::hook::FindModule("libgot_victim.so");
    CHECK(module);
    CHECK(mtasa::hook::HookGot(module, "simbolo_inexistente", FakeTarget, &orig_target) == 0);
    CHECK(mtasa::hook::HookGot(module, "got_victim_target", FakeTarget, &orig_target) >= 1);
    CHECK(orig_target != nullptr);
    CHECK(got_victim_caller(1) == 1020);  // caller -> PLT -> FakeTarget -> original
    // Repetir não empilha o hook.
    CHECK(mtasa::hook::HookGot(module, "got_victim_target", FakeTarget, &orig_target) == 0);
    CHECK(got_victim_caller(1) == 1020);
}

int main() {
    TestGotHook();
    TestCappedAllocator();
    CHECK(mtasa::net::TransportName() != nullptr);
    std::puts("ok");
    return 0;
}
