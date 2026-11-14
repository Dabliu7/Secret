// Cliente de eco para PC: conecta em um servidor de eco e mostra o ping.
// Serve para testar a rede no sentido PC -> aparelho, quando o PC não consegue
// receber conexões (ex.: WSL2 em NAT). No aparelho, "Testar rede" sobe o servidor.
// Uso: mtasa_echo_client <host[:porta]> [segundos]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "net/Session.h"

int main(int argc, char** argv) {
    using mtasa::net::Session;
    char host[64];
    uint16_t port = 0;
    if (argc < 2 || !Session::ParseEndpoint(argv[1], host, sizeof(host), &port)) {
        std::fprintf(stderr, "uso: %s <host[:porta]> [segundos]\n", argv[0]);
        return 2;
    }
    const int seconds = argc > 2 ? std::atoi(argv[2]) : 10;

    Session session;
    if (!session.Connect(host, port)) return 1;

    uint32_t reported = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        session.Pulse();
        if (session.state() == Session::State::Failed || session.state() == Session::State::Disconnected) break;
        if (session.echoes_received() != reported) {
            reported = session.echoes_received();
            std::printf("eco %u de %s:%u: %d ms\n", reported, host, port, session.ping_ms());
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    char status[96];
    session.DescribeStatus(status, sizeof(status));
    std::printf("resultado: %s, %u eco(s)\n", status, reported);
    return reported > 0 ? 0 : 1;
}
