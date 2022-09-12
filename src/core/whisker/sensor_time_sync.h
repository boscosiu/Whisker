#ifndef WHISKER_SENSOR_TIME_SYNC_H
#define WHISKER_SENSOR_TIME_SYNC_H

#include <chrono>

namespace whisker {

// Based on an algorithm presented in the paper
//
// E. Olson, "A passive solution to the sensor synchronization problem," 2010 IEEE/RSJ International Conference on
// Intelligent Robots and Systems, 2010, pp. 1059-1064, doi: 10.1109/IROS.2010.5650579.
//
// https://april.eecs.umich.edu/pdfs/olson2010.pdf

template <typename DurationUnit>
class SensorTimeSync final {
    using RepType = typename DurationUnit::rep;

  public:
    SensorTimeSync(float drift_ratio = 0.02) : drift_ratio(drift_ratio) {}

    RepType GetAdjustedTime(
            RepType sensor_time,
            const std::chrono::system_clock::time_point& host_time_point = std::chrono::system_clock::now()) {
        const auto host_time = std::chrono::duration_cast<DurationUnit>(host_time_point.time_since_epoch()).count();
        const auto current_offset = sensor_time - host_time;

        if (first_run) {
            first_run = false;
            estimate_sensor_time = sensor_time;
            estimate_offset = current_offset;
            return host_time;
        }

        const auto drift_offset =
                estimate_offset - static_cast<RepType>((sensor_time - estimate_sensor_time) * drift_ratio);

        if (current_offset >= drift_offset) {
            estimate_sensor_time = sensor_time;
            estimate_offset = current_offset;
            return host_time;
        } else {
            return sensor_time - drift_offset;
        }
    }

  private:
    const float drift_ratio;
    bool first_run = true;
    RepType estimate_sensor_time;
    RepType estimate_offset;
};

}  // namespace whisker

#endif  // WHISKER_SENSOR_TIME_SYNC_H
