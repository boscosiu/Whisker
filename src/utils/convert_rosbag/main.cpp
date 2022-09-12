#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <rosbag/bag.h>
#include <rosbag/query.h>
#include <rosbag/view.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/MultiEchoLaserScan.h>
#include <whisker/init.h>
#include <whisker/message_log.h>
#include <client.pb.h>

DEFINE_string(in_bag, "input.bag", "Input ROS bag file");
DEFINE_string(topic, "horizontal_lidar", "ROS topic (of Imu/LaserScan/MultiEchoLaserScan type) to convert");
DEFINE_string(out_log, "output.obslog", "Output Whisker observation log file");

void PopulateSensorProperties(const sensor_msgs::Imu& sensor_msg, whisker::proto::SensorClientInitMessage& proto_msg) {
    // no conversion required besides setting an empty IMU properties object in 'proto_msg'
    proto_msg.mutable_imu_properties();
}

void PopulateSensorProperties(const sensor_msgs::LaserScan& sensor_msg,
                              whisker::proto::SensorClientInitMessage& proto_msg) {
    const auto lidar_properties = proto_msg.mutable_lidar_properties();
    lidar_properties->set_starting_angle(sensor_msg.angle_min);
    lidar_properties->set_angular_resolution(sensor_msg.angle_increment);
    lidar_properties->set_rotations_per_second(1 / sensor_msg.scan_time);
}

void PopulateSensorProperties(const sensor_msgs::MultiEchoLaserScan& sensor_msg,
                              whisker::proto::SensorClientInitMessage& proto_msg) {
    const auto lidar_properties = proto_msg.mutable_lidar_properties();
    lidar_properties->set_starting_angle(sensor_msg.angle_min);
    lidar_properties->set_angular_resolution(sensor_msg.angle_increment);
    lidar_properties->set_rotations_per_second(1 / sensor_msg.scan_time);
}

void PopulateObservation(const sensor_msgs::Imu& sensor_msg, whisker::proto::ObservationMessage& proto_msg) {
    const auto imu_observation = proto_msg.mutable_imu_observation();
    imu_observation->set_linear_acceleration_x(sensor_msg.linear_acceleration.x);
    imu_observation->set_linear_acceleration_y(sensor_msg.linear_acceleration.y);
    imu_observation->set_linear_acceleration_z(sensor_msg.linear_acceleration.z);
    imu_observation->set_angular_velocity_x(sensor_msg.angular_velocity.x);
    imu_observation->set_angular_velocity_y(sensor_msg.angular_velocity.y);
    imu_observation->set_angular_velocity_z(sensor_msg.angular_velocity.z);
}

void PopulateObservation(const sensor_msgs::LaserScan& sensor_msg, whisker::proto::ObservationMessage& proto_msg) {
    const auto lidar_observation = proto_msg.mutable_lidar_observation();
    lidar_observation->mutable_measurements()->Reserve(sensor_msg.ranges.size());
    for (const auto& distance : sensor_msg.ranges) {
        if (distance <= sensor_msg.range_min || distance >= sensor_msg.range_max) {
            lidar_observation->add_measurements(0);
        } else {
            lidar_observation->add_measurements(distance * 1000);
        }
    }
}

void PopulateObservation(const sensor_msgs::MultiEchoLaserScan& sensor_msg,
                         whisker::proto::ObservationMessage& proto_msg) {
    const auto lidar_observation = proto_msg.mutable_lidar_observation();
    lidar_observation->mutable_measurements()->Reserve(sensor_msg.ranges.size());
    for (const auto& measurement : sensor_msg.ranges) {
        if (measurement.echoes.empty()) {
            lidar_observation->add_measurements(0);
        } else {
            const auto distance = measurement.echoes.at(0);
            if (distance <= sensor_msg.range_min || distance >= sensor_msg.range_max) {
                lidar_observation->add_measurements(0);
            } else {
                lidar_observation->add_measurements(distance * 1000);
            }
        }
    }
}

template <typename SensorMsgType>
void ProcessBag(const rosbag::Bag& bag) {
    rosbag::View input_view(bag, rosbag::TopicQuery{FLAGS_topic});
    const auto output_log = whisker::MessageLogWriter::CreateInstance(FLAGS_out_log);
    unsigned int num_processed = 0;

    for (const auto& input_message : input_view) {
        const auto msg = input_message.instantiate<SensorMsgType>();
        CHECK(msg) << "Error instantiating sensor message";

        // the first message in the observation log is always the SensorClientInitMessage describing the sensor
        if (num_processed == 0) {
            auto init_message = std::make_shared<whisker::proto::SensorClientInitMessage>();
            PopulateSensorProperties(*msg, *init_message);
            init_message->set_keep_out_radius(0.5);  // this data's not in the bag so just assign a sane arbitrary value
            output_log->Write(std::move(init_message));
        }

        auto observation_message = std::make_shared<whisker::proto::ObservationMessage>();
        observation_message->set_timestamp(msg->header.stamp.toNSec() / 1'000'000);
        PopulateObservation(*msg, *observation_message);
        output_log->Write(std::move(observation_message));

        ++num_processed;
        LOG_EVERY_T(INFO, 5) << "Converted " << 100.0 * num_processed / input_view.size() << "%";

        if (output_log->GetNumPendingWrites() > 500) {
            LOG(INFO) << "Pausing to allow MessageLogWriter queue to drain";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    LOG(INFO) << "Converted " << num_processed << " messages";
}

int main(int argc, char* argv[]) {
    whisker::Init::InitLogging(&argc, &argv);

    LOG(INFO) << "Converting sensor messages from topic '" << FLAGS_topic << "' in ROS bag file '" << FLAGS_in_bag
              << "' to Whisker observation log file '" << FLAGS_out_log << "'";

    const rosbag::Bag input_bag(FLAGS_in_bag);
    std::string message_type;

    LOG(INFO) << "ROS bag contains the following topics (types):";
    for (const auto connection : rosbag::View{input_bag}.getConnections()) {
        LOG(INFO) << "- " << connection->topic << " (" << connection->datatype << ")";
        if (connection->topic == FLAGS_topic) {
            LOG(INFO) << "  ^ selected";
            message_type = connection->datatype;
        }
    }

    if (message_type == "sensor_msgs/Imu") {
        ProcessBag<sensor_msgs::Imu>(input_bag);
    } else if (message_type == "sensor_msgs/LaserScan") {
        ProcessBag<sensor_msgs::LaserScan>(input_bag);
    } else if (message_type == "sensor_msgs/MultiEchoLaserScan") {
        ProcessBag<sensor_msgs::MultiEchoLaserScan>(input_bag);
    } else if (message_type.empty()) {
        LOG(FATAL) << "Topic '" << FLAGS_topic << "' not found in bag";
    } else {
        LOG(FATAL) << "Unsupported message type '" << message_type << "' in given topic";
    }

    return 0;
}
