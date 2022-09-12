#ifndef WHISKER_SERVER_CONNECTION_H
#define WHISKER_SERVER_CONNECTION_H

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace google::protobuf {
class MessageLite;
}

namespace whisker {

class ServerConnection {
  public:
    virtual ~ServerConnection() = default;

    virtual void SendMessage(const google::protobuf::MessageLite& message) = 0;

    virtual void StopMessageHandling() = 0;
};

struct ServerEventHandlers {
    using MessageHandler = std::function<void(google::protobuf::MessageLite&&, ServerConnection&)>;
    using DisconnectHandler = std::function<void(ServerConnection&)>;

    template <typename MessageType, typename HandlerType>
    void SetMessageHandler(HandlerType&& message_handler) {
        static_assert(std::is_base_of_v<google::protobuf::MessageLite, MessageType>,
                      "MessageType must derive from google::protobuf::MessageLite");
        static_assert(std::is_invocable_r_v<void, HandlerType, MessageType&&, ServerConnection&>,
                      "HandlerType must be compatible with void(MessageType&&, ServerConnection&)");

        message_handlers.emplace_back(MessageType::default_instance(),
                                      [handler = std::forward<HandlerType>(message_handler)](
                                              google::protobuf::MessageLite&& message, ServerConnection& connection) {
                                          handler(static_cast<MessageType&&>(message), connection);
                                      });
    }

    std::vector<std::pair<const google::protobuf::MessageLite&, MessageHandler>> message_handlers;
    DisconnectHandler disconnect_handler;
};

}  // namespace whisker

#endif  // WHISKER_SERVER_CONNECTION_H
