#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <json/json.h>
#include <whisker/init.h>
#include <whisker/server_connection.h>
#include <whisker/zmq_connection.h>
#include <client.pb.h>
#include "pca9685.h"

DEFINE_string(id, "drive0", "Vehicle-wide unique ID used to identify this client to the server");

constexpr std::string_view capability_forward = "forward";
constexpr std::string_view capability_reverse = "reverse";
constexpr std::string_view capability_left = "left";
constexpr std::string_view capability_right = "right";
constexpr std::string_view capability_stop = "stop";

class Client final : public whisker::Init::Context {
  public:
    using whisker::Init::Context::Context;

  private:
    void InitContext(Json::Value&& config) override {
        const auto& vehicle_config = config["vehicle"];

        auto pca9685 = std::make_shared<Pca9685>(vehicle_config["clients"][FLAGS_id]);

        whisker::proto::CapabilityClientInitMessage init_msg;
        init_msg.set_vehicle_id(vehicle_config["id"].asString());
        init_msg.add_capabilities(std::string{capability_forward});
        init_msg.add_capabilities(std::string{capability_reverse});
        init_msg.add_capabilities(std::string{capability_left});
        init_msg.add_capabilities(std::string{capability_right});
        init_msg.add_capabilities(std::string{capability_stop});

        whisker::ServerEventHandlers event_handlers;
        event_handlers.disconnect_handler = [pca9685, init_msg](auto& connection) {
            // if disconnected, stop the vehicle and enqueue init msg to send when reconnected
            pca9685->MoveForward(0);
            connection.SendMessage(init_msg);
        };
        event_handlers.SetMessageHandler<whisker::proto::InvokeCapabilityMessage>(
                [pca9685](auto&& message, auto& connection) {
                    if (message.capability() == capability_stop) {
                        pca9685->MoveForward(0);
                    } else {
                        float input = 1;
                        if (!message.input().empty()) {
                            try {
                                input = std::stof(message.input());
                            } catch (const std::exception& ex) {
                                LOG(WARNING) << "Invalid input to '" << message.capability()
                                             << "' capability: " << message.input();
                                pca9685->MoveForward(0);
                                return;
                            }
                        }
                        if (message.capability() == capability_forward) {
                            pca9685->MoveForward(input);
                        } else if (message.capability() == capability_reverse) {
                            pca9685->MoveForward(-input);
                        } else if (message.capability() == capability_left) {
                            pca9685->RotateRight(-input);
                        } else if (message.capability() == capability_right) {
                            pca9685->RotateRight(input);
                        }
                    }
                });

        server_connection = whisker::ZmqConnection::CreateServerConnection(vehicle_config["server_address"].asString(),
                                                                           vehicle_config["id"].asString() + FLAGS_id,
                                                                           std::move(event_handlers));
        server_connection->SendMessage(init_msg);
    }

    std::shared_ptr<whisker::ServerConnection> server_connection;
};

int main(int argc, char* argv[]) {
    return Client{&argc, &argv}.Run();
}
