#ifndef WHISKER_WEBSOCKET_CONNECTION_H
#define WHISKER_WEBSOCKET_CONNECTION_H

#include <memory>
#include <string>
#include <whisker/client_connection.h>

namespace whisker {

class WebsocketConnection final {
  public:
    // serve files out of root_path, or pass an empty string to disable file serving
    static std::shared_ptr<ClientConnection> CreateClientConnection(unsigned short bind_port,
                                                                    std::string root_path,
                                                                    ClientEventHandlers event_handlers = {});
};

}  // namespace whisker

#endif  // WHISKER_WEBSOCKET_CONNECTION_H
