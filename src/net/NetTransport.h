#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace mtasa::net {

enum class Reliability : uint8_t { Unreliable, UnreliableSequenced, Reliable, ReliableOrdered };

enum class EventType : uint8_t { None, Connected, ConnectFailed, Disconnected, Packet };

struct Event {
    EventType type = EventType::None;
    const uint8_t* data = nullptr;  // válido até a próxima chamada de Poll()
    size_t size = 0;
};

// Fronteira equivalente à CNet do MTA (interface exposta por netc.dll no PC).
// Tudo acima daqui (CClientGame, sync, packets) não deve conhecer o transporte.
class INetTransport {
public:
    virtual ~INetTransport() = default;
    virtual bool Connect(const char* host, uint16_t port) = 0;
    virtual void Disconnect() = 0;
    virtual bool Send(const uint8_t* data, size_t size, Reliability reliability, uint8_t channel) = 0;
    // Não bloqueante; chamado 1x por frame a partir de CGame::Process.
    virtual bool Poll(Event* out) = 0;
    virtual int PingMs() const = 0;
    // Primeiro byte livre para pacotes da aplicação (os anteriores são do transporte).
    virtual uint8_t FirstUserPacketId() const = 0;
};

// Servidor de eco: devolve ao remetente todo pacote de aplicação recebido.
// Usado no autoteste em loopback dentro do jogo e na ferramenta de host.
class IEchoServer {
public:
    virtual ~IEchoServer() = default;
    virtual bool Start(uint16_t port) = 0;
    virtual void Pulse() = 0;  // não bloqueante
    virtual void Stop() = 0;
};
std::unique_ptr<IEchoServer> CreateEchoServer();

// Transporte disponível nesta build (RakNet se MTASA_WITH_RAKNET, senão nulo).
std::unique_ptr<INetTransport> CreateTransport();
const char* TransportName();

}  // namespace mtasa::net
