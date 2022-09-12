#ifndef WHISKER_CLIENT_CONNECTION_H
#define WHISKER_CLIENT_CONNECTION_H

#include <functional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace google::protobuf {
class MessageLite;
}

namespace whisker {

class ClientConnection {
  public:
    virtual ~ClientConnection() = default;

    virtual void SendMessage(const google::protobuf::MessageLite& message, const std::string& recipient_id) = 0;

    virtual void BroadcastMessage(const google::protobuf::MessageLite& message) = 0;

    virtual void StopMessageHandling() = 0;

    virtual std::unordered_set<std::string> GetConnectedClientIds() = 0;
};

struct ClientEventHandlers {
    using MessageHandler = std::function<void(google::protobuf::MessageLite&&, ClientConnection&, std::string&&)>;
    using ConnectionStateHandler = std::function<void(ClientConnection&, std::string&&, bool)>;

    template <typename MessageType, typename HandlerType>
    void SetMessageHandler(HandlerType&& message_handler) {
        static_assert(std::is_base_of_v<google::protobuf::MessageLite, MessageType>,
                      "MessageType must derive from google::protobuf::MessageLite");
        static_assert(std::is_invocable_r_v<void, HandlerType, MessageType&&, ClientConnection&, std::string&&>,
                      "HandlerType must be compatible with void(MessageType&&, ClientConnection&, std::string&&)");

        message_handlers.emplace_back(
                MessageType::default_instance(), [handler = std::forward<HandlerType>(message_handler)](
                                                         google::protobuf::MessageLite&& message,
                                                         ClientConnection& connection, std::string&& sender_id) {
                    handler(static_cast<MessageType&&>(message), connection, std::move(sender_id));
                });
    }

    std::vector<std::pair<const google::protobuf::MessageLite&, MessageHandler>> message_handlers;
    ConnectionStateHandler connection_state_handler;
};

}  // namespace whisker

#endif  // WHISKER_CLIENT_CONNECTION_H
