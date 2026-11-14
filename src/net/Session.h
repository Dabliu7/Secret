#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

#include "net/NetTransport.h"

namespace mtasa::net {

// Sessão de rede do cliente. Hoje fala o protocolo do servidor de eco (fase 3);
// é o ponto onde entrará o handshake do servidor de jogo.
// Usada só no thread do jogo: Pulse() uma vez por frame.
class Session {
public:
    enum class State : uint8_t { Idle, Connecting, Connected, Failed, Disconnected };

    static constexpr uint16_t kDefaultPort = 22003;

    // Separa "host:porta" (porta opcional -> kDefaultPort). Só IPv4/nome, sem IPv6.
    static bool ParseEndpoint(const char* text, char* host, size_t host_size, uint16_t* port);

    // Sem transporte nesta build (RakNet desligado), Connect* retorna false.
    bool Connect(const char* host, uint16_t port);
    // Autoteste: sobe um servidor de eco neste processo e conecta em 127.0.0.1.
    bool ConnectLoopback(uint16_t port = kDefaultPort);
    void Disconnect();
    void Pulse();

    State state() const { return state_; }
    bool active() const { return state_ == State::Connecting || state_ == State::Connected; }
    // Ida e volta do último pacote de eco, em ms; -1 se ainda não mediu.
    int ping_ms() const { return ping_ms_; }
    uint32_t echoes_received() const { return echoes_received_; }
    // Ex.: "conectado a 127.0.0.1:22003, 3 ms".
    void DescribeStatus(char* out, size_t size) const;

private:
    void SendPing(uint64_t now_us);

    std::unique_ptr<INetTransport> transport_;
    std::unique_ptr<IEchoServer> loopback_server_;
    State state_ = State::Idle;
    char host_[64] = {};
    uint16_t port_ = 0;
    uint32_t next_seq_ = 0;
    uint32_t echoes_received_ = 0;
    uint64_t last_ping_sent_us_ = 0;
    int ping_ms_ = -1;
};

}  // namespace mtasa::net
