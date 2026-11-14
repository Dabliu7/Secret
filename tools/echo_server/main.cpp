// Servidor de eco para testar a rede do cliente a partir de um PC.
// Uso: mtasa_echo_server [porta]   (padrão 22003/UDP)
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "net/NetTransport.h"
#include "net/Session.h"

static volatile std::sig_atomic_t g_running = 1;

int main(int argc, char** argv) {
    const auto port = static_cast<uint16_t>(argc > 1 ? std::atoi(argv[1]) : mtasa::net::Session::kDefaultPort);
    auto server = mtasa::net::CreateEchoServer();
    if (!server) {
        std::fprintf(stderr, "esta build não tem transporte (compile com -DMTASA_WITH_RAKNET=ON)\n");
        return 1;
    }
    if (!server->Start(port)) {
        std::fprintf(stderr, "não foi possível abrir a porta %u/UDP\n", port);
        return 1;
    }
    std::signal(SIGINT, [](int) { g_running = 0; });
    std::printf("servidor de eco em 0.0.0.0:%u/UDP (Ctrl+C para sair)\n", port);
    while (g_running) {
        server->Pulse();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    server->Stop();
    return 0;
}
