#include "sick.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <glog/logging.h>

#ifdef _WIN32
// Winsock errors are slightly different from BSD sockets so we can't use CHECK_ERR()
#define WHISKER_CHECK_WINSOCK(e, x) CHECK_NE(e, x) << "(Winsock error " << WSAGetLastError() << ") "
#define WHISKER_CHECK_SOCKET(x) WHISKER_CHECK_WINSOCK(SOCKET_ERROR, x)
#define WHISKER_CHECK_SOCKET_VALID(x) WHISKER_CHECK_WINSOCK(INVALID_SOCKET, x)
#else
#define WHISKER_CHECK_SOCKET(x) CHECK_ERR(x)
#define WHISKER_CHECK_SOCKET_VALID(x) CHECK_ERR(x)
#endif

constexpr std::string_view cmd_log_in = "sMN SetAccessMode 03 F4724744";
constexpr std::string_view cmd_log_out = "sMN Run";
constexpr std::string_view cmd_laser_on = "sMN LMCstartmeas";
constexpr std::string_view cmd_laser_off = "sMN LMCstopmeas";
constexpr std::string_view cmd_start_measurement = "sEN LMDscandata 1";
constexpr std::string_view cmd_stop_measurement = "sEN LMDscandata 0";
constexpr std::string_view cmd_get_properties = "sRN LMPscancfg";
// below command disables most fields other than distance data
constexpr std::string_view cmd_set_config = "sWN LMDscandatacfg 01 00 0 1 0 00 00 0 0 0 0 +1";
constexpr char payload_start_marker = '\x02';
constexpr char payload_end_marker = '\x03';
constexpr std::string_view sensor_properties_response_prefix = "sRA LMPscancfg";
constexpr std::string_view scan_response_prefix = "sSN LMDscandata";

Sick::Sick(const Json::Value& config)
        : direction(config["rotates_clockwise"].asBool() ? -1 : 1), angle_offset(config["angle_offset"].asInt()) {
    const auto address = config["address"].asString();
    const auto port_str = std::to_string(config["port"].asUInt());

    LOG(INFO) << "Connecting to sensor at " << address << " port " << port_str;

#ifdef _WIN32
    WSADATA winsock_data;
    CHECK_EQ(WSAStartup(MAKEWORD(2, 2), &winsock_data), 0) << "Error starting Winsock";
#endif

    addrinfo request{};
    request.ai_family = AF_INET;
    request.ai_socktype = SOCK_STREAM;
    request.ai_protocol = 0;

    addrinfo* response;
    const auto rc = getaddrinfo(address.c_str(), port_str.c_str(), &request, &response);
    CHECK_EQ(rc, 0) << "Error getting sensor address: " << gai_strerror(rc);

    socket_fd = socket(response->ai_family, response->ai_socktype, response->ai_protocol);
    WHISKER_CHECK_SOCKET_VALID(socket_fd) << "Error opening socket to sensor";
    WHISKER_CHECK_SOCKET(connect(socket_fd, response->ai_addr, response->ai_addrlen)) << "Error connecting to sensor";

    freeaddrinfo(response);

    PopulateSensorProperties(config);

    SendCommand(cmd_log_in);
    SendCommand(cmd_set_config);
    SendCommand(cmd_laser_on);
    SendCommand(cmd_log_out);
    sensor_thread = std::thread{&Sick::ProcessSensorData, this};
}

Sick::~Sick() {
    run_sensor_thread = false;
    sensor_thread.join();
    SendCommand(cmd_log_in);
    SendCommand(cmd_laser_off);
    SendCommand(cmd_log_out);
#ifdef _WIN32
    closesocket(socket_fd);
    WSACleanup();
#else
    close(socket_fd);
#endif
}

whisker::proto::LidarSensorProperties Sick::GetSensorProperties() const {
    return sensor_properties;
}

whisker::proto::ObservationMessage Sick::GetLatestObservation() {
    whisker::proto::ObservationMessage observation_message;

    observation_buffer.Read([this, &observation_message](const auto response) {
        auto cursor = response->payload.data();

        // skip to timestamp (10th value in payload)
        for (auto i = 0; i < 9; ++i) {
            cursor = std::strchr(++cursor, ' ');
        }
        const std::uint32_t sensor_time = std::strtoull(++cursor, &cursor, 16);  // microseconds
        observation_message.set_timestamp(sensor_time_sync.GetAdjustedTime(sensor_time, response->host_time) / 1000);

        // skip to beginning of distance measurements
        cursor = std::strstr(cursor, "DIST1") + 5;

        // convert the hex string into a binary representation then access it directly as a 32-bit IEEE 754 float
        const std::uint32_t scale_factor_binary = std::strtoul(++cursor, &cursor, 16);
        float scale_factor;
        std::memcpy(&scale_factor, &scale_factor_binary, sizeof(scale_factor_binary));

        // we don't care about the next 3 values so skip over them
        for (auto i = 0; i < 3; ++i) {
            cursor = std::strchr(++cursor, ' ');
        }

        std::uint16_t num_readings = std::strtoul(++cursor, &cursor, 16);

        const auto lidar_observation = observation_message.mutable_lidar_observation();
        lidar_observation->mutable_measurements()->Reserve(num_readings);
        for (; num_readings > 0; --num_readings) {
            const std::uint16_t reading = std::strtoul(++cursor, &cursor, 16);
            lidar_observation->add_measurements(reading < 16 ? 0 : reading * scale_factor);  // values < 16 are errors
        }
    });

    return observation_message;
}

void Sick::PopulateSensorProperties(const Json::Value& config) {
    SendCommand(cmd_get_properties);

    Response response;
    ReadResponse(sensor_properties_response_prefix, &response);

    auto cursor = response.payload.data();

    // skip to scan frequency (3rd value in payload)
    for (auto i = 0; i < 2; ++i) {
        cursor = std::strchr(++cursor, ' ');
    }
    const std::uint32_t scan_frequency = std::strtoull(++cursor, &cursor, 16);
    sensor_properties.set_rotations_per_second(scan_frequency / 100);

    // we don't care about the next value (number of sectors) so skip over it
    cursor = std::strchr(++cursor, ' ');

    const std::uint32_t angular_resolution = std::strtoull(++cursor, &cursor, 16);
    sensor_properties.set_angular_resolution(angular_resolution / 10'000.0 * direction * M_PI / 180);

    std::int32_t starting_angle = std::strtoll(++cursor, &cursor, 16);
    starting_angle -= (angle_offset * 10'000);
    sensor_properties.set_starting_angle(starting_angle / 10'000.0 * M_PI / 180);

    const auto position = sensor_properties.mutable_position();
    position->set_x(config["position"]["x"].asDouble());
    position->set_y(config["position"]["y"].asDouble());
    position->set_r(config["position"]["r"].asDouble() * M_PI / 180);
}

void Sick::ProcessSensorData() {
    SendCommand(cmd_start_measurement);
    while (run_sensor_thread) {
        observation_buffer.Write([this](const auto response) { ReadResponse(scan_response_prefix, response); });
    }
    SendCommand(cmd_stop_measurement);
}

void Sick::SendCommand(const std::string_view& command) {
    LOG(INFO) << "Sending command: " << command;
    WHISKER_CHECK_SOCKET(send(socket_fd, &payload_start_marker, 1, 0));
    WHISKER_CHECK_SOCKET(send(socket_fd, command.data(), command.size(), 0));
    WHISKER_CHECK_SOCKET(send(socket_fd, &payload_end_marker, 1, 0));
}

void Sick::ReadResponse(const std::string_view& expected_prefix, Response* response) {
    do {
        response->payload.clear();

        // locate the beginning of the payload
        while (true) {
            if (buf_cursor == buf_end) {
                const auto num_bytes_recv = recv(socket_fd, response_buf.get(), response_buf_size, 0);
                WHISKER_CHECK_SOCKET(num_bytes_recv) << "Error reading from sensor";
                buf_end = response_buf.get() + num_bytes_recv;
                buf_cursor = response_buf.get();
            }
            if (*(buf_cursor++) == payload_start_marker) {
                break;
            }
        }

        response->host_time = std::chrono::system_clock::now();

        // 'buf_cursor' is now pointing to the first char of the payload, so read until we find the end delimiter
        while (true) {
            if (buf_cursor == buf_end) {
                const auto num_bytes_recv = recv(socket_fd, response_buf.get(), response_buf_size, 0);
                WHISKER_CHECK_SOCKET(num_bytes_recv) << "Error reading from sensor";
                buf_end = response_buf.get() + num_bytes_recv;
                buf_cursor = response_buf.get();
            }
            auto payload_end = buf_cursor;
            for (; payload_end < buf_end; ++payload_end) {
                if (*payload_end == payload_end_marker) {
                    break;
                }
            }
            // at this point 'payload_end' is either pointing to the delimiter or 'buf_end' if not found
            response->payload.append(buf_cursor, payload_end - buf_cursor);
            if (payload_end == buf_end) {
                buf_cursor = buf_end;
            } else {
                // delimiter found, finished with this payload
                buf_cursor = payload_end + 1;
                break;
            }
        }
    } while (response->payload.compare(0, expected_prefix.size(), expected_prefix) != 0);
}
