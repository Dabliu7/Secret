#include "net/NetTransport.h"

namespace mtasa::net {

#ifdef MTASA_WITH_RAKNET
std::unique_ptr<INetTransport> CreateRakNetTransport();
std::unique_ptr<IEchoServer> CreateRakNetEchoServer();
std::unique_ptr<INetTransport> CreateTransport() { return CreateRakNetTransport(); }
std::unique_ptr<IEchoServer> CreateEchoServer() { return CreateRakNetEchoServer(); }
const char* TransportName() { return "raknet"; }
#else
std::unique_ptr<INetTransport> CreateTransport() { return nullptr; }
std::unique_ptr<IEchoServer> CreateEchoServer() { return nullptr; }
const char* TransportName() { return "none"; }
#endif

}  // namespace mtasa::net
