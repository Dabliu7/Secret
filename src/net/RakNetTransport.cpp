// Transporte sobre RakNet 4.x aberto. NÃO é compatível em nível de fio com o
// módulo de rede oficial do MTA (fechado) — ver docs/FEASIBILITY.md, seção 4.
#include "net/NetTransport.h"

#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"

namespace mtasa::net {
namespace {

PacketReliability ToRak(Reliability r) {
    switch (r) {
        case Reliability::Unreliable: return UNRELIABLE;
        case Reliability::UnreliableSequenced: return UNRELIABLE_SEQUENCED;
        case Reliability::Reliable: return RELIABLE;
        case Reliability::ReliableOrdered: return RELIABLE_ORDERED;
    }
    return RELIABLE_ORDERED;
}

class RakNetTransport final : public INetTransport {
public:
    RakNetTransport() : peer_(RakNet::RakPeerInterface::GetInstance()) {}
    ~RakNetTransport() override {
        ReleasePacket();
        peer_->Shutdown(100);
        RakNet::RakPeerInterface::DestroyInstance(peer_);
    }

    bool Connect(const char* host, uint16_t port) override {
        if (!started_) {
            RakNet::SocketDescriptor sd;
            if (peer_->Startup(1, &sd, 1) != RakNet::RAKNET_STARTED) return false;
            started_ = true;
        }
        return peer_->Connect(host, port, nullptr, 0) == RakNet::CONNECTION_ATTEMPT_STARTED;
    }

    void Disconnect() override {
        if (server_ != RakNet::UNASSIGNED_SYSTEM_ADDRESS) peer_->CloseConnection(server_, true);
        server_ = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
    }

    bool Send(const uint8_t* data, size_t size, Reliability reliability, uint8_t channel) override {
        if (server_ == RakNet::UNASSIGNED_SYSTEM_ADDRESS) return false;
        return peer_->Send(reinterpret_cast<const char*>(data), static_cast<int>(size), HIGH_PRIORITY,
                           ToRak(reliability), static_cast<char>(channel), server_, false) != 0;
    }

    bool Poll(Event* out) override {
        ReleasePacket();
        packet_ = peer_->Receive();
        if (!packet_) return false;
        *out = Event{};
        switch (packet_->data[0]) {
            case ID_CONNECTION_REQUEST_ACCEPTED:
                server_ = packet_->systemAddress;
                out->type = EventType::Connected;
                break;
            case ID_CONNECTION_ATTEMPT_FAILED:
            case ID_NO_FREE_INCOMING_CONNECTIONS:
            case ID_CONNECTION_BANNED:
            case ID_INCOMPATIBLE_PROTOCOL_VERSION:
                out->type = EventType::ConnectFailed;
                break;
            case ID_DISCONNECTION_NOTIFICATION:
            case ID_CONNECTION_LOST:
                server_ = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
                out->type = EventType::Disconnected;
                break;
            default:
                out->type = EventType::Packet;
                out->data = packet_->data;
                out->size = packet_->length;
                break;
        }
        return true;
    }

    int PingMs() const override {
        return server_ == RakNet::UNASSIGNED_SYSTEM_ADDRESS ? -1 : peer_->GetAveragePing(server_);
    }

    uint8_t FirstUserPacketId() const override { return ID_USER_PACKET_ENUM; }

private:
    void ReleasePacket() {
        if (packet_) peer_->DeallocatePacket(packet_);
        packet_ = nullptr;
    }

    RakNet::RakPeerInterface* peer_;
    RakNet::Packet* packet_ = nullptr;
    RakNet::SystemAddress server_ = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
    bool started_ = false;
};

class RakNetEchoServer final : public IEchoServer {
public:
    ~RakNetEchoServer() override { Stop(); }

    bool Start(uint16_t port) override {
        if (peer_) return true;
        peer_ = RakNet::RakPeerInterface::GetInstance();
        RakNet::SocketDescriptor sd(port, nullptr);
        if (peer_->Startup(32, &sd, 1) != RakNet::RAKNET_STARTED) {
            Stop();
            return false;
        }
        peer_->SetMaximumIncomingConnections(32);
        return true;
    }

    void Pulse() override {
        if (!peer_) return;
        while (RakNet::Packet* packet = peer_->Receive()) {
            if (packet->length > 0 && packet->data[0] >= ID_USER_PACKET_ENUM) {
                peer_->Send(reinterpret_cast<const char*>(packet->data), static_cast<int>(packet->length),
                            HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
            }
            peer_->DeallocatePacket(packet);
        }
    }

    void Stop() override {
        if (!peer_) return;
        peer_->Shutdown(100);
        RakNet::RakPeerInterface::DestroyInstance(peer_);
        peer_ = nullptr;
    }

private:
    RakNet::RakPeerInterface* peer_ = nullptr;
};

}  // namespace

std::unique_ptr<IEchoServer> CreateRakNetEchoServer() { return std::make_unique<RakNetEchoServer>(); }

std::unique_ptr<INetTransport> CreateRakNetTransport() { return std::make_unique<RakNetTransport>(); }

}  // namespace mtasa::net
