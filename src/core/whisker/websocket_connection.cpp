#include "websocket_connection.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <glog/logging.h>
#include <google/protobuf/message_lite.h>
#include <libwebsockets.h>

// libwebsockets.h includes a header on Windows that defines an interfering macro
#undef SendMessage

namespace whisker {

constexpr const char client_id_query_param[] = "client_id";
constexpr unsigned int client_id_buf_size = 256;

class WebsocketClientConnection final : public ClientConnection {
  public:
    WebsocketClientConnection(unsigned short bind_port, std::string&& root_path, ClientEventHandlers&& event_handlers)
            : mount_root(std::move(root_path)) {
        LOG(INFO) << "WebsocketClientConnection: listening on port " << bind_port;

        lws_set_log_level(0, nullptr);  // disable libwebsockets logging

        context_info.port = bind_port;
        context_info.user = this;

        if (!mount_root.empty()) {
            LOG(INFO) << "WebsocketClientConnection: serving content from "
                      << std::filesystem::absolute(mount_root).lexically_normal();

            mime_types[0] = {&mime_types[1], nullptr, ".proto", "text/plain"};
            mime_types[1] = {nullptr, nullptr, ".map", "application/json"};

            mount.mountpoint = "/";
            mount.mountpoint_len = 1;
            mount.origin = mount_root.c_str();
            mount.def = "index.html";
            mount.extra_mimetypes = &mime_types[0];
            mount.origin_protocol = LWSMPRO_FILE;

            context_info.mounts = &mount;
        }

        protocols[0] = {"http", &lws_callback_http_dummy, 0, 0};
        protocols[1] = {"whisker", &WebsocketClientConnection::WebsocketCallback, client_id_buf_size, 0};
        protocols[2] = {nullptr, nullptr, 0, 0};
        context_info.protocols = protocols;

        websocket_context = CHECK_NOTNULL(lws_create_context(&context_info));

        for (auto& [msg, handler] : event_handlers.message_handlers) {
            const auto [it, emplaced] = message_handlers.try_emplace(
                    msg.GetTypeName(), std::unique_ptr<google::protobuf::MessageLite>{msg.New()}, std::move(handler));
            CHECK(emplaced) << "Message handler already set for type " << msg.GetTypeName();
        }
        connection_state_handler = std::move(event_handlers.connection_state_handler);

        message_loop_thread = std::thread{&WebsocketClientConnection::MessageLoop, this};
    }

    ~WebsocketClientConnection() override {
        StopMessageHandling();
        lws_context_destroy(websocket_context);
        LOG(INFO) << "WebsocketClientConnection: closed";
    }

  private:
    struct ClientData {
        lws* websocket_instance;
        std::queue<std::string> outgoing_messages;
    };

    static std::string& SerializeMessage(const google::protobuf::MessageLite& message) {
        thread_local std::string serialized_message;
        serialized_message.resize(LWS_PRE);
        serialized_message.append(message.GetTypeName());
        serialized_message.push_back('\0');
        message.AppendToString(&serialized_message);
        return serialized_message;
    }

    void SendMessage(const google::protobuf::MessageLite& message, const std::string& recipient_id) override {
        const auto& serialized_message = SerializeMessage(message);

        std::unique_lock lock(client_data_mutex);
        const auto recipient = client_data.find(recipient_id);
        if (recipient != client_data.end()) {
            recipient->second.outgoing_messages.emplace(serialized_message);
            lws_callback_on_writable(recipient->second.websocket_instance);
            lws_cancel_service(websocket_context);
        }
    }

    void BroadcastMessage(const google::protobuf::MessageLite& message) override {
        const auto& serialized_message = SerializeMessage(message);

        std::unique_lock lock(client_data_mutex);
        for (auto& [client_id, data] : client_data) {
            data.outgoing_messages.emplace(serialized_message);
            lws_callback_on_writable(data.websocket_instance);
        }
        lws_cancel_service(websocket_context);
    }

    void StopMessageHandling() override {
        if (run_message_loop) {
            run_message_loop = false;
            lws_cancel_service(websocket_context);
            message_loop_thread.join();
        }
        message_handlers.clear();
        connection_state_handler = {};
    }

    std::unordered_set<std::string> GetConnectedClientIds() override {
        std::unordered_set<std::string> connected_clients;
        std::shared_lock lock(client_data_mutex);
        for (const auto& [client_id, data] : client_data) {
            connected_clients.emplace(client_id);
        }
        return connected_clients;
    }

    void MessageLoop() {
        while (run_message_loop) {
            lws_service(websocket_context, 0);
        }
    }

    static int WebsocketCallback(lws* wsi, lws_callback_reasons reason, void* user, void* in, size_t len) {
        const auto this_obj = static_cast<WebsocketClientConnection*>(lws_context_user(lws_get_context(wsi)));
        thread_local std::string client_id;

        switch (reason) {
            case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION: {
                char buf[client_id_buf_size];

                if (lws_get_urlarg_by_name_safe(wsi, client_id_query_param, buf, sizeof(buf)) > 0) {
                    // save the client id to the connection's user data buffer for subsequent callbacks
                    std::strcpy(static_cast<char*>(user), buf);
                } else {
                    // refuse connection -- next callback will be LWS_CALLBACK_CLOSED
                    LOG(WARNING) << "WebsocketClientConnection: refused connection without client ID";
                    *(static_cast<char*>(user)) = '\0';
                    return -1;
                }
            } break;

            case LWS_CALLBACK_ESTABLISHED: {
                client_id = static_cast<const char*>(user);

                {
                    std::unique_lock lock(this_obj->client_data_mutex);
                    const auto it = this_obj->client_data.find(client_id);
                    if ((it != this_obj->client_data.end()) && (it->second.websocket_instance != wsi)) {
                        // new connection for existing client id -- close old connection, new one takes over
                        lws_set_timeout(it->second.websocket_instance, PENDING_TIMEOUT_CLOSE_SEND, LWS_TO_KILL_ASYNC);
                    }
                    this_obj->client_data[client_id].websocket_instance = wsi;
                }
                LOG(INFO) << "WebsocketClientConnection: '" << client_id << "' connected";
                if (this_obj->connection_state_handler) {
                    this_obj->connection_state_handler(*this_obj, std::move(client_id), true);
                }
            } break;

            case LWS_CALLBACK_CLOSED: {
                client_id = static_cast<const char*>(user);

                std::unique_lock lock(this_obj->client_data_mutex);
                const auto it = this_obj->client_data.find(client_id);
                if ((it != this_obj->client_data.end()) && (it->second.websocket_instance == wsi)) {
                    this_obj->client_data.erase(it);
                    lock.unlock();
                    LOG(INFO) << "WebsocketClientConnection: '" << client_id << "' disconnected";
                    if (this_obj->connection_state_handler) {
                        this_obj->connection_state_handler(*this_obj, std::move(client_id), false);
                    }
                }
            } break;

            case LWS_CALLBACK_SERVER_WRITEABLE: {
                client_id = static_cast<const char*>(user);

                std::unique_lock lock(this_obj->client_data_mutex);
                auto& recipient = this_obj->client_data.at(client_id);
                if (!recipient.outgoing_messages.empty()) {
                    auto message = std::move(recipient.outgoing_messages.front());

                    recipient.outgoing_messages.pop();
                    if (!recipient.outgoing_messages.empty()) {
                        lws_callback_on_writable(wsi);
                    }

                    lock.unlock();

                    lws_write(wsi, reinterpret_cast<unsigned char*>(message.data() + LWS_PRE), message.size() - LWS_PRE,
                              LWS_WRITE_BINARY);
                }
            } break;

            case LWS_CALLBACK_RECEIVE: {
                client_id = static_cast<const char*>(user);

                const auto begin = static_cast<const char*>(in);
                const auto end = begin + len;
                const auto delimiter = std::find(begin, end, '\0');

                if (delimiter != end) {
                    thread_local std::string message_type;

                    message_type.assign(begin, delimiter - begin);

                    const auto it = this_obj->message_handlers.find(message_type);
                    if (it != this_obj->message_handlers.end()) {
                        const auto& [cached_message, handler] = it->second;
                        const auto message_size = (((delimiter + 1) == end) ? 0 : (end - delimiter - 1));

                        if (cached_message->ParseFromArray(delimiter + 1, message_size)) {
                            handler(std::move(*cached_message), *this_obj, std::move(client_id));
                        } else {
                            LOG(WARNING) << "WebsocketClientConnection: malformed " << message_type << " from '"
                                         << client_id << "'";
                        }
                    }
                }
            } break;

            default:
                break;
        }

        return 0;
    }

    lws_context* websocket_context;
    std::thread message_loop_thread;
    std::atomic_bool run_message_loop = true;

    const std::string mount_root;
    lws_context_creation_info context_info = {};
    lws_protocol_vhost_options mime_types[2];
    lws_http_mount mount = {};
    lws_protocols protocols[3];

    std::unordered_map<std::string,
                       std::pair<std::unique_ptr<google::protobuf::MessageLite>, ClientEventHandlers::MessageHandler>>
            message_handlers;
    ClientEventHandlers::ConnectionStateHandler connection_state_handler;

    std::unordered_map<std::string, ClientData> client_data;
    std::shared_mutex client_data_mutex;
};

std::shared_ptr<ClientConnection> WebsocketConnection::CreateClientConnection(unsigned short bind_port,
                                                                              std::string root_path,
                                                                              ClientEventHandlers event_handlers) {
    return std::make_shared<WebsocketClientConnection>(bind_port, std::move(root_path), std::move(event_handlers));
}

}  // namespace whisker
