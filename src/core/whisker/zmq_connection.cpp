#include "zmq_connection.h"
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <glog/logging.h>
#include <google/protobuf/message_lite.h>
#include <zmq.h>

namespace whisker {

class ZmqOps final {
  public:
    ZmqOps(int socket_type) {
        zeromq_context = zmq_ctx_new();
        PCHECK(zeromq_context != nullptr);
        zeromq_socket = zmq_socket(zeromq_context, socket_type);
        PCHECK(zeromq_socket != nullptr);
    }

    ~ZmqOps() {
        zmq_close(signal_socket_w);
        zmq_close(signal_socket_r);
        zmq_close(zeromq_socket);
        zmq_ctx_term(zeromq_context);
    }

    template <bool expect_sender_id, typename EventHandlerType>
    void StartMessageLoop(const std::string& address, EventHandlerType&& event_handler) {
        // we signal the message loop to terminate via a pair of inproc sockets
        signal_socket_r = zmq_socket(zeromq_context, ZMQ_PAIR);
        PCHECK(signal_socket_r != nullptr);
        signal_socket_w = zmq_socket(zeromq_context, ZMQ_PAIR);
        PCHECK(signal_socket_w != nullptr);

        // hash the endpoint address to get a unique (and valid) inproc socket address per connection
        const std::string signal_socket_address = "inproc://" + std::to_string(std::hash<std::string>{}(address));

        CHECK_ERR(zmq_bind(signal_socket_r, signal_socket_address.c_str()));
        CHECK_ERR(zmq_connect(signal_socket_w, signal_socket_address.c_str()));

        message_loop_thread = std::thread{[this, event_handler = std::move(event_handler)]() mutable {
            zmq_pollitem_t poll_items[2]{};
            auto& [signal_poll_item, socket_poll_item] = poll_items;
            signal_poll_item.socket = signal_socket_r;
            signal_poll_item.events = ZMQ_POLLIN;
            socket_poll_item.socket = zeromq_socket;
            socket_poll_item.events = ZMQ_POLLIN;

            zmq_msg_t zeromq_message;
            std::string sender_id;

            while (true) {
                zmq_poll(poll_items, 2, -1);

                if (signal_poll_item.revents) {
                    break;
                }

                zmq_msg_close(&zeromq_message);
                zmq_msg_init(&zeromq_message);
                {
                    std::scoped_lock lock(socket_mutex);
                    zmq_msg_recv(&zeromq_message, zeromq_socket, 0);
                }

                if constexpr (expect_sender_id) {
                    if (zmq_msg_more(&zeromq_message)) {
                        sender_id.assign(static_cast<const char*>(zmq_msg_data(&zeromq_message)),
                                         zmq_msg_size(&zeromq_message));

                        zmq_msg_close(&zeromq_message);
                        zmq_msg_init(&zeromq_message);
                        {
                            std::scoped_lock lock(socket_mutex);
                            zmq_msg_recv(&zeromq_message, zeromq_socket, 0);
                        }

                        event_handler(std::move(sender_id), static_cast<const char*>(zmq_msg_data(&zeromq_message)),
                                      zmq_msg_size(&zeromq_message));
                    } else {
                        LOG(WARNING) << "ZmqClientConnection: malformed message with no sender ID";
                    }
                } else {
                    event_handler(static_cast<const char*>(zmq_msg_data(&zeromq_message)),
                                  zmq_msg_size(&zeromq_message));
                }
            }

            zmq_msg_close(&zeromq_message);
        }};
    }

    void StopMessageLoop() {
        if (message_loop_thread.joinable()) {
            zmq_send(signal_socket_w, "#", 1, 0);
            message_loop_thread.join();
        }
    }

    template <typename Operation>
    void UseSocket(const Operation& op) {
        std::scoped_lock lock(socket_mutex);
        op(zeromq_socket);
    }

    template <typename HandlerType>
    static auto BuildMessageHandlerMap(
            std::vector<std::pair<const google::protobuf::MessageLite&, HandlerType>>&& message_handlers) {
        std::unordered_map<std::string, std::pair<std::unique_ptr<google::protobuf::MessageLite>, HandlerType>>
                message_handler_map;
        for (auto& [msg, handler] : message_handlers) {
            const auto [it, emplaced] = message_handler_map.try_emplace(
                    msg.GetTypeName(), std::unique_ptr<google::protobuf::MessageLite>{msg.New()}, std::move(handler));
            CHECK(emplaced) << "Message handler already set for type " << msg.GetTypeName();
        }
        return message_handler_map;
    }

    static std::string& SerializeMessage(const google::protobuf::MessageLite& message) {
        thread_local std::string serialized_message;
        serialized_message = message.GetTypeName();
        serialized_message.push_back('\0');
        message.AppendToString(&serialized_message);
        return serialized_message;
    }

  private:
    void* zeromq_context;
    void* zeromq_socket;
    std::mutex socket_mutex;

    std::thread message_loop_thread;
    void* signal_socket_r;
    void* signal_socket_w;
};

class ZmqClientConnection final : public ClientConnection {
  public:
    ZmqClientConnection(const std::string& bind_address, ClientEventHandlers&& event_handlers) : zmq_ops(ZMQ_ROUTER) {
        LOG(INFO) << "ZmqClientConnection: listening on " << bind_address;

        zmq_ops.UseSocket([&bind_address](const auto socket) {
            // talk to newer client if duplicate client id encountered
            const int opt_handover = 1;
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_ROUTER_HANDOVER, &opt_handover, sizeof(opt_handover)));

            // discard pending messages when closing socket
            const int opt_linger = 0;
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_LINGER, &opt_linger, sizeof(opt_linger)));

            // get notified with an empty message when client connects
            const int opt_router_notify = ZMQ_NOTIFY_CONNECT;
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_ROUTER_NOTIFY, &opt_router_notify, sizeof(opt_router_notify)));

            // get notified with a special message when client disconnects
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_DISCONNECT_MSG, "D", 1));

            CHECK_ERR(zmq_bind(socket, bind_address.c_str()));
        });

        zmq_ops.StartMessageLoop<true>(
                bind_address,
                [this, message_type = std::string{},
                 message_handlers = ZmqOps::BuildMessageHandlerMap(std::move(event_handlers.message_handlers)),
                 connection_state_handler = std::move(event_handlers.connection_state_handler)](
                        auto&& sender_id, auto msg, auto msg_size) mutable {
                    // handle special connection state change messages:
                    // empty message = client connected, single 'D' character = client disconnected
                    if (msg_size == 0) {
                        {
                            std::unique_lock lock(connected_clients_mutex);
                            connected_clients.emplace(sender_id);
                        }
                        LOG(INFO) << "ZmqClientConnection: '" << sender_id << "' connected";
                        if (connection_state_handler) {
                            connection_state_handler(*this, std::move(sender_id), true);
                        }
                        return;
                    } else if ((msg_size == 1) && (*msg == 'D')) {
                        {
                            std::unique_lock lock(connected_clients_mutex);
                            connected_clients.erase(sender_id);
                        }
                        LOG(INFO) << "ZmqClientConnection: '" << sender_id << "' disconnected";
                        if (connection_state_handler) {
                            connection_state_handler(*this, std::move(sender_id), false);
                        }
                        return;
                    }

                    const auto begin = msg;
                    const auto end = begin + msg_size;
                    const auto delimiter = std::find(begin, end, '\0');

                    if (delimiter != end) {
                        message_type.assign(begin, delimiter - begin);

                        const auto it = message_handlers.find(message_type);
                        if (it != message_handlers.end()) {
                            const auto& [cached_message, handler] = it->second;
                            const auto message_size = (((delimiter + 1) == end) ? 0 : (end - delimiter - 1));

                            if (cached_message->ParseFromArray(delimiter + 1, message_size)) {
                                handler(std::move(*cached_message), *this, std::move(sender_id));
                            } else {
                                LOG(WARNING) << "ZmqClientConnection: malformed " << message_type << " from '"
                                             << sender_id << "'";
                            }
                        }
                    }
                });
    }

    ~ZmqClientConnection() override {
        StopMessageHandling();
        LOG(INFO) << "ZmqClientConnection: closed";
    }

  private:
    void SendMessage(const google::protobuf::MessageLite& message, const std::string& recipient_id) override {
        if (!recipient_id.empty()) {
            zmq_ops.UseSocket([&recipient_id, &msg = ZmqOps::SerializeMessage(message)](const auto socket) {
                zmq_send(socket, recipient_id.c_str(), recipient_id.size(), ZMQ_SNDMORE);
                zmq_send(socket, msg.c_str(), msg.size(), 0);
            });
        }
    }

    void BroadcastMessage(const google::protobuf::MessageLite& message) override {
        std::shared_lock lock_clients(connected_clients_mutex);
        zmq_ops.UseSocket([this, &msg = ZmqOps::SerializeMessage(message)](const auto socket) {
            for (const auto& recipient_id : connected_clients) {
                zmq_send(socket, recipient_id.c_str(), recipient_id.size(), ZMQ_SNDMORE);
                zmq_send(socket, msg.c_str(), msg.size(), 0);
            }
        });
    }

    void StopMessageHandling() override { zmq_ops.StopMessageLoop(); }

    std::unordered_set<std::string> GetConnectedClientIds() override {
        std::shared_lock lock(connected_clients_mutex);
        return connected_clients;
    }

    ZmqOps zmq_ops;
    std::unordered_set<std::string> connected_clients;
    std::shared_mutex connected_clients_mutex;
};

class ZmqServerConnection final : public ServerConnection {
  public:
    ZmqServerConnection(const std::string& server_address,
                        const std::string& client_id,
                        ServerEventHandlers&& event_handlers)
            : zmq_ops(ZMQ_DEALER) {
        LOG(INFO) << "ZmqServerConnection: connecting to " << server_address << " with ID '" << client_id << "'";

        zmq_ops.UseSocket([&client_id, &server_address](const auto socket) {
            // discard pending messages when closing socket
            const int opt_linger = 0;
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_LINGER, &opt_linger, sizeof(opt_linger)));

            // get notified with a special message when server disconnects
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_HICCUP_MSG, "D", 1));

            // use our client id as the socket routing id
            CHECK_ERR(zmq_setsockopt(socket, ZMQ_ROUTING_ID, client_id.c_str(), client_id.size()));

            CHECK_ERR(zmq_connect(socket, server_address.c_str()));
        });

        zmq_ops.StartMessageLoop<false>(
                server_address,
                [this, message_type = std::string{},
                 message_handlers = ZmqOps::BuildMessageHandlerMap(std::move(event_handlers.message_handlers)),
                 disconnect_handler = std::move(event_handlers.disconnect_handler)](auto msg, auto msg_size) mutable {
                    // handle special connection state change message:
                    // single 'D' character = server disconnected
                    if ((msg_size == 1) && (*msg == 'D')) {
                        LOG(INFO) << "ZmqServerConnection: disconnected from server, will try to reconnect";
                        if (disconnect_handler) {
                            disconnect_handler(*this);
                        }
                        return;
                    }

                    const auto begin = msg;
                    const auto end = begin + msg_size;
                    const auto delimiter = std::find(begin, end, '\0');

                    if (delimiter != end) {
                        message_type.assign(begin, delimiter - begin);

                        const auto it = message_handlers.find(message_type);
                        if (it != message_handlers.end()) {
                            const auto& [cached_message, handler] = it->second;
                            const auto message_size = (((delimiter + 1) == end) ? 0 : (end - delimiter - 1));

                            if (cached_message->ParseFromArray(delimiter + 1, message_size)) {
                                handler(std::move(*cached_message), *this);
                            } else {
                                LOG(WARNING) << "ZmqServerConnection: malformed " << message_type << " from server";
                            }
                        }
                    }
                });
    }

    ~ZmqServerConnection() override {
        StopMessageHandling();
        LOG(INFO) << "ZmqServerConnection: closed";
    }

  private:
    void SendMessage(const google::protobuf::MessageLite& message) override {
        zmq_ops.UseSocket([&msg = ZmqOps::SerializeMessage(message)](const auto socket) {
            zmq_send(socket, msg.c_str(), msg.size(), 0);
        });
    }

    void StopMessageHandling() override { zmq_ops.StopMessageLoop(); }

    ZmqOps zmq_ops;
};

std::shared_ptr<ClientConnection> ZmqConnection::CreateClientConnection(const std::string& bind_address,
                                                                        ClientEventHandlers event_handlers) {
    return std::make_shared<ZmqClientConnection>(bind_address, std::move(event_handlers));
}

std::shared_ptr<ServerConnection> ZmqConnection::CreateServerConnection(const std::string& server_address,
                                                                        const std::string& client_id,
                                                                        ServerEventHandlers event_handlers) {
    return std::make_shared<ZmqServerConnection>(server_address, client_id, std::move(event_handlers));
}

}  // namespace whisker
