#include <future>
#include <memory>
#include <utility>
#include <vector>
#include <glog/logging.h>
#include <json/json.h>
#include <whisker/client_connection.h>
#include <whisker/init.h>
#include <whisker/websocket_connection.h>
#include <whisker/zmq_connection.h>
#include <client.pb.h>
#include <console.pb.h>
#include "server_tasks.h"

class Server final : public whisker::Init::Context {
  public:
    using whisker::Init::Context::Context;

    ~Server() override {
        // the connection objects reference each other through their message handlers, so stop them before destruction
        for (const auto& connection : client_connections) {
            connection->StopMessageHandling();
        }
        for (const auto& connection : console_connections) {
            connection->StopMessageHandling();
        }
    }

  private:
    void InitContext(Json::Value&& config) override {
        const auto& server_config = config["server"];

        auto server_tasks = std::make_shared<ServerTasks>(server_config);

        std::promise<void> init_completed_promise;
        init_completed = init_completed_promise.get_future();

        InitClientService(server_config["client_service"], server_tasks);
        InitConsoleService(server_config["console_service"], server_tasks);

        init_completed_promise.set_value();
    }

    void InitClientService(const Json::Value& client_service_cfg, std::shared_ptr<ServerTasks>& server_tasks) {
        whisker::ClientEventHandlers event_handlers;

        event_handlers.SetMessageHandler<whisker::proto::SensorClientInitMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& client_id) {
                    if (!message.vehicle_id().empty() && (message.keep_out_radius() > 0)) {
                        LOG(INFO) << "Init sensor client '" << client_id << "' on vehicle '" << message.vehicle_id()
                                  << "'";
                        server_tasks->AddSensorClient(
                                client_id, message,
                                [&connection, client_id, msg = whisker::proto::RequestObservationMessage{}] {
                                    connection.SendMessage(msg, client_id);
                                });
                        BroadcastConsoleMessage(server_tasks->GetServerState());
                    } else {
                        LOG(WARNING) << "Sensor client '" << client_id
                                     << "' init requires specifying vehicle_id and keep_out_radius";
                    }
                });

        event_handlers.SetMessageHandler<whisker::proto::CapabilityClientInitMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& client_id) {
                    if (!message.vehicle_id().empty()) {
                        LOG(INFO) << "Init capability client '" << client_id << "' on vehicle '" << message.vehicle_id()
                                  << "'";
                        server_tasks->AddCapabilityClient(client_id, message, MakeResponder(connection, client_id));
                        BroadcastConsoleMessage(server_tasks->GetServerState());
                    } else {
                        LOG(WARNING) << "Capability client '" << client_id << "' init requires specifying vehicle_id";
                    }
                });

        event_handlers.SetMessageHandler<whisker::proto::ObservationMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& client_id) {
                    server_tasks->SubmitObservation(std::move(client_id), message);
                });

        const auto& ws_cfg = client_service_cfg["websocket"];
        if (ws_cfg["enabled"].asBool()) {
            client_connections.emplace_back(
                    whisker::WebsocketConnection::CreateClientConnection(ws_cfg["port"].asUInt(), {}, event_handlers));
        }

        const auto& zmq_cfg = client_service_cfg["zeromq"];
        if (zmq_cfg["enabled"].asBool()) {
            client_connections.emplace_back(
                    whisker::ZmqConnection::CreateClientConnection(zmq_cfg["bind_address"].asString(), event_handlers));
        }
    }

    void InitConsoleService(const Json::Value& console_service_cfg, std::shared_ptr<ServerTasks>& server_tasks) {
        whisker::ClientEventHandlers event_handlers;

        event_handlers.connection_state_handler = [server_tasks](auto& connection, auto&& console_id,
                                                                 auto is_connected) {
            if (is_connected) {
                // notify newly connected consoles of current server state
                connection.SendMessage(server_tasks->GetServerState(), console_id);
            }
        };

        event_handlers.SetMessageHandler<whisker::proto::RequestResourceFilesMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    server_tasks->GetResourceFiles(MakeResponder(connection, std::move(console_id)));
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestMapDataMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    server_tasks->GetMapData(message.map_id(), message.have_version(),
                                             MakeResponder(connection, std::move(console_id)));
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestSubmapTexturesMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    server_tasks->GetSubmapTextures(message, MakeResponder(connection, std::move(console_id)));
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestVehiclePosesMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    server_tasks->GetVehiclePoses(message.map_id(), MakeResponder(connection, std::move(console_id)));
                });

        event_handlers.SetMessageHandler<whisker::proto::InvokeCapabilityMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    server_tasks->InvokeCapability(message);
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestCreateMapMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Create map '" << message.map_id() << "' requested by '" << console_id << "'";
                    server_tasks->CreateMap(message.map_id(), message.use_overlapping_trimmer());
                    BroadcastConsoleMessage(server_tasks->GetServerState());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestDeleteMapMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Delete map '" << message.map_id() << "' requested by '" << console_id << "'";
                    server_tasks->DeleteMap(message.map_id());
                    BroadcastConsoleMessage(server_tasks->GetServerState());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestSaveMapMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Save map '" << message.map_id() << "' requested by '" << console_id << "'";
                    server_tasks->SaveMap(message.map_id());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestLoadMapMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Load map '" << message.map_id() << "' from file '" << message.map_file_name()
                              << "' requested by '" << console_id << "'";
                    server_tasks->LoadMap(message.map_id(), message.map_file_name(), message.is_frozen(),
                                          message.use_overlapping_trimmer());
                    BroadcastConsoleMessage(server_tasks->GetServerState());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestDeleteVehicleMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Delete vehicle '" << message.vehicle_id() << "' requested by '" << console_id << "'";
                    server_tasks->DeleteVehicle(message.vehicle_id());
                    BroadcastConsoleMessage(server_tasks->GetServerState());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestAssignVehicleToMapMessage>(
                [this, server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Assign vehicle '" << message.vehicle_id() << "' to map '" << message.map_id()
                              << "' requested by '" << console_id << "'";
                    server_tasks->AssignVehicleToMap(message);
                    BroadcastConsoleMessage(server_tasks->GetServerState());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestStartObservationLogMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Start observation log for vehicle '" << message.vehicle_id() << "' requested by '"
                              << console_id << "'";
                    server_tasks->StartObservationLog(message.vehicle_id());
                });

        event_handlers.SetMessageHandler<whisker::proto::RequestStopObservationLogMessage>(
                [server_tasks](auto&& message, auto& connection, auto&& console_id) {
                    LOG(INFO) << "Stop observation log for vehicle '" << message.vehicle_id() << "' requested by '"
                              << console_id << "'";
                    server_tasks->StopObservationLog(message.vehicle_id());
                });

        const auto& ws_cfg = console_service_cfg["websocket"];
        if (ws_cfg["enabled"].asBool()) {
            console_connections.emplace_back(whisker::WebsocketConnection::CreateClientConnection(
                    ws_cfg["port"].asUInt(), ws_cfg["root"].asString(), event_handlers));
        }

        const auto& zmq_cfg = console_service_cfg["zeromq"];
        if (zmq_cfg["enabled"].asBool()) {
            console_connections.emplace_back(
                    whisker::ZmqConnection::CreateClientConnection(zmq_cfg["bind_address"].asString(), event_handlers));
        }
    }

    template <typename MessageType>
    void BroadcastConsoleMessage(const MessageType& message) {
        init_completed.wait();
        for (const auto& connection : console_connections) {
            connection->BroadcastMessage(message);
        }
    }

    template <typename RecipientIdType>
    static auto MakeResponder(whisker::ClientConnection& connection, RecipientIdType&& recipient_id) {
        return [&connection, recipient_id = std::forward<RecipientIdType>(recipient_id)](const auto& message) {
            connection.SendMessage(message, recipient_id);
        };
    }

    std::vector<std::shared_ptr<whisker::ClientConnection>> client_connections;
    std::vector<std::shared_ptr<whisker::ClientConnection>> console_connections;
    std::future<void> init_completed;
};

int main(int argc, char* argv[]) {
    return Server{&argc, &argv}.Run();
}
