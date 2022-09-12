#ifndef WHISKER_SICK_H
#define WHISKER_SICK_H

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <json/json.h>
#include <whisker/overwriting_buffer.h>
#include <whisker/sensor_time_sync.h>
#include <client.pb.h>

class Sick final {
  public:
    Sick(const Json::Value& config);
    ~Sick();

    Sick(const Sick&) = delete;
    Sick& operator=(const Sick&) = delete;

    whisker::proto::LidarSensorProperties GetSensorProperties() const;
    whisker::proto::ObservationMessage GetLatestObservation();

  private:
    struct Response {
        std::chrono::system_clock::time_point host_time;
        std::string payload;
    };

    void PopulateSensorProperties(const Json::Value& config);
    void ProcessSensorData();
    void SendCommand(const std::string_view& command);
    void ReadResponse(const std::string_view& expected_prefix, Response* response);

    const short direction;
    const short angle_offset;

    static constexpr auto response_buf_size = 2048;
    const std::unique_ptr<char[]> response_buf = std::make_unique<char[]>(response_buf_size);
    const char* buf_cursor = response_buf.get();
    const char* buf_end = response_buf.get();  // points to one past the last written char in 'response_buf'

    whisker::proto::LidarSensorProperties sensor_properties;
    whisker::OverwritingBuffer<Response> observation_buffer;
    whisker::SensorTimeSync<std::chrono::microseconds> sensor_time_sync;
#ifdef _WIN32
    SOCKET socket_fd;
#else
    int socket_fd;
#endif
    std::thread sensor_thread;
    std::atomic_bool run_sensor_thread = true;
};

#endif  // WHISKER_SICK_H
