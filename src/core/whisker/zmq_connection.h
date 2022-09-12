#ifndef WHISKER_ZMQ_CONNECTION_H
#define WHISKER_ZMQ_CONNECTION_H

#include <memory>
#include <string>
#include <whisker/client_connection.h>
#include <whisker/server_connection.h>

namespace whisker {

class ZmqConnection final {
  public:
    static std::shared_ptr<ClientConnection> CreateClientConnection(const std::string& bind_address,
                                                                    ClientEventHandlers event_handlers = {});

    static std::shared_ptr<ServerConnection> CreateServerConnection(const std::string& server_address,
                                                                    const std::string& client_id,
                                                                    ServerEventHandlers event_handlers = {});
};

}  // namespace whisker

#endif  // WHISKER_ZMQ_CONNECTION_H
