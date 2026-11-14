// Testes mínimos de host para a lógica independente de plataforma.
#include <cstdio>
#include <cstdlib>

#include "lua/LuaAllocator.h"
#include "mtasa/Hook.h"
#include "net/NetTransport.h"
#include "net/Session.h"

#include <chrono>
#include <string>
#include <thread>

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

#ifdef MTASA_WITH_RAKNET
// Cliente e servidor de eco no mesmo processo, pela pilha UDP real em loopback.
static void TestSessionLoopback() {
    using mtasa::net::Session;
    Session session;
    CHECK(session.state() == Session::State::Idle);
    CHECK(session.ConnectLoopback(47113));
    for (int i = 0; i < 600 && session.echoes_received() < 2; ++i) {
        session.Pulse();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(session.state() == Session::State::Connected);
    CHECK(session.echoes_received() >= 2);
    CHECK(session.ping_ms() >= 0 && session.ping_ms() < 1000);
    char status[96];
    session.DescribeStatus(status, sizeof(status));
    std::printf("sessão: %s\n", status);
    session.Disconnect();
    CHECK(session.state() == Session::State::Idle);

    // Porta sem servidor: precisa terminar em Failed, não ficar pendurado.
    CHECK(session.Connect("127.0.0.1", 47114));
    for (int i = 0; i < 1500 && session.state() == Session::State::Connecting; ++i) {
        session.Pulse();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(session.state() == Session::State::Failed);
}
#endif

static void TestParseEndpoint() {
    using mtasa::net::Session;
    char host[16];
    uint16_t port = 0;
    CHECK(Session::ParseEndpoint("192.168.0.10:22010", host, sizeof(host), &port));
    CHECK(std::string(host) == "192.168.0.10" && port == 22010);
    CHECK(Session::ParseEndpoint("10.0.0.2", host, sizeof(host), &port));
    CHECK(std::string(host) == "10.0.0.2" && port == Session::kDefaultPort);
    CHECK(!Session::ParseEndpoint("", host, sizeof(host), &port));
    CHECK(!Session::ParseEndpoint(":22003", host, sizeof(host), &port));
    CHECK(!Session::ParseEndpoint("host:", host, sizeof(host), &port));
    CHECK(!Session::ParseEndpoint("host:0", host, sizeof(host), &port));
    CHECK(!Session::ParseEndpoint("host:70000", host, sizeof(host), &port));
    CHECK(!Session::ParseEndpoint("host:12ab", host, sizeof(host), &port));
    CHECK(!Session::ParseEndpoint("nome-de-host-longo-demais:1", host, sizeof(host), &port));
}

int main() {
    TestGotHook();
    TestParseEndpoint();
#ifdef MTASA_WITH_RAKNET
    TestSessionLoopback();
#endif
    TestCappedAllocator();
    CHECK(mtasa::net::TransportName() != nullptr);
    std::puts("ok");
    return 0;
}
