#ifndef WHISKER_OBSERVATION_LOG_H
#define WHISKER_OBSERVATION_LOG_H

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <glog/logging.h>
#include <whisker/message_log.h>
#include <client.pb.h>

class ObservationLog final {
  public:
    using GetOffsetCallback = std::function<std::chrono::system_clock::duration()>;

    ObservationLog(const std::string& log_file_path,
                   const std::string& sensor_name,
                   std::uint64_t start_offset,
                   std::uint64_t stop_offset,
                   const GetOffsetCallback& get_offset_callback = {})
            : sensor_name(sensor_name),
              stop_offset(stop_offset),
              is_realtime(get_offset_callback),
              get_offset_callback(get_offset_callback) {
        log_reader = whisker::MessageLogReader::CreateInstance(log_file_path);

        CHECK(log_reader->Read(init_msg)) << "Error reading SensorClientInitMessage from observation log";

        if (init_msg.sensor_type_case() == whisker::proto::SensorClientInitMessage::kLidarProperties) {
            LOG(INFO) << sensor_name << ": rate " << init_msg.lidar_properties().rotations_per_second()
                      << " Hz, angular resolution " << init_msg.lidar_properties().angular_resolution() * 180 / M_PI
                      << " degrees";
        }

        ReadNextObservation();
        CHECK(cached_observation_valid) << "No observation messages in log";

        log_start_timestamp = cached_observation.timestamp();

        if (start_offset > 0) {
            AdvanceToOffset(start_offset);
        }

        playback_start_timestamp = cached_observation.timestamp();
    }

    ObservationLog(const ObservationLog&) = delete;
    ObservationLog& operator=(const ObservationLog&) = delete;

    whisker::proto::SensorClientInitMessage GetSensorClientInitMessage() const { return init_msg; }

    std::chrono::system_clock::time_point GetPlaybackStartTime() const {
        return std::chrono::system_clock::time_point{std::chrono::milliseconds{playback_start_timestamp}};
    }

    template <typename ObservationMessageCallback>
    void GetNextObservationMessage(const ObservationMessageCallback& callback) {
        bool retry_required;

        do {
            retry_required = false;

            if (cached_observation_valid &&
                ((stop_offset == 0) || (stop_offset > (cached_observation.timestamp() - log_start_timestamp)))) {
                ++num_read;

                if (is_realtime) {
                    if (num_read == 1) {
                        // get the shared time offset that is synchronized with other playback clients
                        time_offset = get_offset_callback();
                    }

                    const auto respond_time = std::chrono::system_clock::time_point{std::chrono::milliseconds{
                                                      cached_observation.timestamp()}} +
                                              time_offset;

                    if (respond_time >= std::chrono::system_clock::now()) {
                        std::this_thread::sleep_until(respond_time);
                    } else {
                        // couldn't keep up with message rate in observation log, so drop this message
                        retry_required = true;
                        ++num_dropped;
                    }
                }

                if (!retry_required) {
                    callback(cached_observation);
                }

                LOG_EVERY_T(INFO, 5) << sensor_name << ": " << log_reader->GetReadPercent() * 100 << "% of log, "
                                     << cached_observation.timestamp() - log_start_timestamp << " ms from start";

                ReadNextObservation();
            } else {
                LOG(INFO) << sensor_name << ": finished, played back " << num_read << " observations, dropped "
                          << num_dropped << " (" << 100.0 * num_dropped / num_read << "%)";
            }
        } while (retry_required);
    }

  private:
    void ReadNextObservation() { cached_observation_valid = log_reader->Read(cached_observation); }

    void AdvanceToOffset(std::uint64_t offset) {
        LOG(INFO) << sensor_name << ": advancing to " << offset << " ms from start";
        auto num_skipped = 0;
        auto timestamp = cached_observation.timestamp();
        while (timestamp < (log_start_timestamp + offset)) {
            ReadNextObservation();
            CHECK(cached_observation_valid) << "Advanced past the end of log file";
            ++num_skipped;
            timestamp = cached_observation.timestamp();
        }
        LOG(INFO) << sensor_name << ": advanced past " << num_skipped << " messages";
    }

    const std::string sensor_name;
    const std::uint64_t stop_offset;
    const bool is_realtime;
    const GetOffsetCallback get_offset_callback;

    std::shared_ptr<whisker::MessageLogReader> log_reader;
    whisker::proto::SensorClientInitMessage init_msg;
    std::uint64_t log_start_timestamp;       // timestamp of first message in log file
    std::uint64_t playback_start_timestamp;  // timestamp of first message to play back having applied start offset
    std::chrono::system_clock::duration time_offset;  // difference between system time and log message time

    unsigned int num_read = 0;
    unsigned int num_dropped = 0;

    // prefetch the next ObservationMessage in the log to reduce latency
    whisker::proto::ObservationMessage cached_observation;
    bool cached_observation_valid;
};

#endif  // WHISKER_OBSERVATION_LOG_H
