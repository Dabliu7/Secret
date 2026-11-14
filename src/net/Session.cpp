#include "net/Session.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#include "mtasa/Log.h"

namespace mtasa::net {
namespace {

constexpr uint64_t kPingIntervalUs = 1'000'000;

uint64_t NowUs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

// Pacote de eco: [id][seq u32][timestamp u64], little-endian, sem padding.
constexpr size_t kPingSize = 1 + 4 + 8;

}  // namespace

bool Session::ParseEndpoint(const char* text, char* host, size_t host_size, uint16_t* port) {
    if (!text || !*text || host_size == 0) return false;
    const char* colon = std::strrchr(text, ':');
    const size_t host_len = colon ? static_cast<size_t>(colon - text) : std::strlen(text);
    if (host_len == 0 || host_len >= host_size) return false;
    std::memcpy(host, text, host_len);
    host[host_len] = '\0';
    *port = kDefaultPort;
    if (colon) {
        unsigned value = 0;
        const char* p = colon + 1;
        if (!*p) return false;
        for (; *p; ++p) {
            if (*p < '0' || *p > '9') return false;
            value = value * 10 + static_cast<unsigned>(*p - '0');
            if (value > 65535) return false;
        }
        if (value == 0) return false;
        *port = static_cast<uint16_t>(value);
    }
    return true;
}

bool Session::Connect(const char* host, uint16_t port) {
    Disconnect();
    transport_ = CreateTransport();
    if (!transport_) {
        LOGE("rede: esta build não tem transporte (%s)", TransportName());
        state_ = State::Failed;
        return false;
    }
    std::snprintf(host_, sizeof(host_), "%s", host);
    port_ = port;
    if (!transport_->Connect(host, port)) {
        LOGE("rede: falha ao iniciar conexão com %s:%u", host, port);
        transport_.reset();
        state_ = State::Failed;
        return false;
    }
    LOGI("rede: conectando a %s:%u", host, port);
    state_ = State::Connecting;
    return true;
}

bool Session::ConnectLoopback(uint16_t port) {
    Disconnect();
    loopback_server_ = CreateEchoServer();
    if (!loopback_server_) {
        LOGE("rede: esta build foi compilada sem transporte (net=%s); "
             "reconfigure com -DMTASA_WITH_RAKNET=ON", TransportName());
        state_ = State::Failed;
        return false;
    }
    if (!loopback_server_->Start(port)) {
        LOGE("rede: não foi possível subir o servidor de eco local na porta %u", port);
        loopback_server_.reset();
        state_ = State::Failed;
        return false;
    }
    std::unique_ptr<IEchoServer> server = std::move(loopback_server_);
    const bool ok = Connect("127.0.0.1", port);  // Connect() chama Disconnect()
    if (ok) loopback_server_ = std::move(server);
    return ok;
}

void Session::Disconnect() {
    if (transport_) {
        transport_->Disconnect();
        transport_.reset();
        LOGI("rede: desconectado");
    }
    loopback_server_.reset();
    state_ = State::Idle;
    ping_ms_ = -1;
    echoes_received_ = 0;
    last_ping_sent_us_ = 0;
}

void Session::SendPing(uint64_t now_us) {
    uint8_t packet[kPingSize];
    packet[0] = transport_->FirstUserPacketId();
    const uint32_t seq = next_seq_++;
    std::memcpy(packet + 1, &seq, sizeof(seq));
    std::memcpy(packet + 5, &now_us, sizeof(now_us));
    transport_->Send(packet, sizeof(packet), Reliability::Unreliable, 0);
    last_ping_sent_us_ = now_us;
}

void Session::Pulse() {
    if (loopback_server_) loopback_server_->Pulse();
    if (!transport_) return;

    Event event;
    while (transport_->Poll(&event)) {
        switch (event.type) {
            case EventType::Connected:
                LOGI("rede: conectado a %s:%u", host_, port_);
                state_ = State::Connected;
                break;
            case EventType::ConnectFailed:
                LOGW("rede: conexão com %s:%u recusada ou sem resposta", host_, port_);
                state_ = State::Failed;
                break;
            case EventType::Disconnected:
                LOGW("rede: conexão perdida");
                state_ = State::Disconnected;
                break;
            case EventType::Packet:
                if (event.size == kPingSize && event.data[0] == transport_->FirstUserPacketId()) {
                    uint64_t sent_us = 0;
                    std::memcpy(&sent_us, event.data + 5, sizeof(sent_us));
                    ping_ms_ = static_cast<int>((NowUs() - sent_us) / 1000);
                    if (echoes_received_++ == 0) LOGI("rede: primeiro eco recebido (%d ms)", ping_ms_);
                }
                break;
            case EventType::None:
                break;
        }
    }

    if (state_ == State::Connected) {
        const uint64_t now = NowUs();
        if (now - last_ping_sent_us_ >= kPingIntervalUs) SendPing(now);
    } else if (state_ == State::Failed || state_ == State::Disconnected) {
        transport_.reset();
        loopback_server_.reset();
    }
}

void Session::DescribeStatus(char* out, size_t size) const {
    switch (state_) {
        case State::Idle: std::snprintf(out, size, "desconectado"); break;
        case State::Connecting: std::snprintf(out, size, "conectando a %s:%u", host_, port_); break;
        case State::Connected:
            if (ping_ms_ >= 0) {
                std::snprintf(out, size, "conectado a %s:%u, %d ms", host_, port_, ping_ms_);
            } else {
                std::snprintf(out, size, "conectado a %s:%u", host_, port_);
            }
            break;
        case State::Failed: std::snprintf(out, size, "falha ao conectar"); break;
        case State::Disconnected: std::snprintf(out, size, "conexao perdida"); break;
    }
}

}  // namespace mtasa::net
