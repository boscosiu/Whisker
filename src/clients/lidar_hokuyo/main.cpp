#include <memory>
#include <utility>
#include <gflags/gflags.h>
#include <json/json.h>
#include <whisker/init.h>
#include <whisker/server_connection.h>
#include <whisker/zmq_connection.h>
#include <client.pb.h>
#include "hokuyo.h"

DEFINE_string(id, "hokuyo0", "Vehicle-wide unique ID used to identify this client to the server");

class Client final : public whisker::Init::Context {
  public:
    using whisker::Init::Context::Context;

  private:
    void InitContext(Json::Value&& config) override {
        const auto& vehicle_config = config["vehicle"];

        auto hokuyo = std::make_shared<Hokuyo>(vehicle_config["clients"][FLAGS_id]);

        whisker::proto::SensorClientInitMessage init_msg;
        init_msg.set_vehicle_id(vehicle_config["id"].asString());
        init_msg.set_keep_out_radius(vehicle_config["keep_out_radius"].asFloat());
        *init_msg.mutable_lidar_properties() = hokuyo->GetSensorProperties();

        whisker::ServerEventHandlers event_handlers;
        event_handlers.disconnect_handler = [init_msg](auto& connection) {
            // enqueue init msg to send when reconnected
            connection.SendMessage(init_msg);
        };
        event_handlers.SetMessageHandler<whisker::proto::RequestObservationMessage>(
                [hokuyo](auto&& message, auto& connection) { connection.SendMessage(hokuyo->GetLatestObservation()); });

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
