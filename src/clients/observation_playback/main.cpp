#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <json/json.h>
#include <whisker/init.h>
#include <whisker/server_connection.h>
#include <whisker/zmq_connection.h>
#include <client.pb.h>
#include "observation_log.h"

DEFINE_string(id, "playback0", "Vehicle-wide unique ID used to identify this client to the server");
DEFINE_string(logs, "input.obslog", "Comma-separated list of observation log files to play back");
DEFINE_uint64(startoffset, 0, "Number of milliseconds from log start at which to start playback");
DEFINE_uint64(stopoffset, 0, "Number of milliseconds from log start at which to stop playback (0 = no limit)");
DEFINE_bool(realtime, true, "Whether to play back observations at the rate at which they were recorded");

class Client final : public whisker::Init::Context {
  public:
    using whisker::Init::Context::Context;

  private:
    struct TimeOffsetData {
        std::mutex mutex;
        std::optional<std::chrono::system_clock::duration> offset;
        std::chrono::system_clock::time_point earliest_log_start = std::chrono::system_clock::time_point::max();
    };

    void InitContext(Json::Value&& config) override {
        LOG_IF(INFO, FLAGS_realtime) << "'realtime' option enabled, observations may be dropped";

        const auto& vehicle_config = config["vehicle"];

        const auto offset_data = std::make_shared<TimeOffsetData>();

        std::vector<std::pair<std::shared_ptr<whisker::ServerConnection>, whisker::proto::SensorClientInitMessage>>
                init_messages;

        std::istringstream log_file_names{FLAGS_logs};
        std::string log_file_name;

        // each log file is a separate sensor client, so they each have an ObservationLog + ServerConnection
        // with which to independently play back observations
        for (auto i = 0; std::getline(log_file_names, log_file_name, ','); ++i) {
            const auto sensor_name = "sensor" + std::to_string(i);
            LOG(INFO) << sensor_name << ": " << log_file_name;

            std::shared_ptr<ObservationLog> log;
            if (FLAGS_realtime) {
                log = std::make_shared<ObservationLog>(
                        log_file_name, sensor_name, FLAGS_startoffset, FLAGS_stopoffset, [offset_data] {
                            std::scoped_lock lock(offset_data->mutex);
                            if (!offset_data->offset) {
                                offset_data->offset = std::chrono::system_clock::now() -
                                                      offset_data->earliest_log_start + std::chrono::seconds{1};
                            }
                            return *(offset_data->offset);
                        });
            } else {
                log = std::make_shared<ObservationLog>(log_file_name, sensor_name, FLAGS_startoffset, FLAGS_stopoffset);
            }

            whisker::ServerEventHandlers event_handlers;
            event_handlers.SetMessageHandler<whisker::proto::RequestObservationMessage>(
                    [log](auto&& message, auto& connection) {
                        log->GetNextObservationMessage(
                                [&connection](const auto& observation) { connection.SendMessage(observation); });
                    });

            const auto server_connection = whisker::ZmqConnection::CreateServerConnection(
                    vehicle_config["server_address"].asString(),
                    vehicle_config["id"].asString() + FLAGS_id + sensor_name, std::move(event_handlers));
            server_connections.emplace_back(server_connection);

            auto init_msg = log->GetSensorClientInitMessage();
            init_msg.set_vehicle_id(vehicle_config["id"].asString());  // override with the vehicle_id in our config
            init_messages.emplace_back(server_connection, std::move(init_msg));

            offset_data->earliest_log_start = std::min(offset_data->earliest_log_start, log->GetPlaybackStartTime());
        }

        for (const auto& [connection, init_message] : init_messages) {
            connection->SendMessage(init_message);
        }
    }

    std::vector<std::shared_ptr<whisker::ServerConnection>> server_connections;
};

int main(int argc, char* argv[]) {
    return Client{&argc, &argv}.Run();
}
