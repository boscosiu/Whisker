## ROS Bag Conversion Utility

`whisker_util_convert_rosbag`

This utility reads IMU or lidar messages from a ROS 1 bag file then converts and writes them to a Whisker observation log.  This allows previously recorded data to be migrated to Whisker or used as tests against ROS systems.

### Supported Platforms

Linux (GCC, Clang), Windows (MSVC)

⚠ The ROS packages `rosbag_storage` and `sensor_msgs` need to be installed prior to initial CMake configuration.  Unlike other Whisker dependencies, these packages are not downloaded and built by CMake.

### Command Line Options

- `--in_bag=[input bag path]`
  - Path to input ROS bag file to read (default: `input.bag` in current directory)
- `--topic=[topic]`
  - ROS topic of messages to read (default: `horizontal_lidar`)
- `--out_log=[output log path]`
  - Path to output observation log file to write (default: `output.obslog` in current directory)

### ROS Messages

The ROS messages [sensor_msgs/Imu](https://docs.ros.org/api/sensor_msgs/html/msg/Imu.html), [sensor_msgs/LaserScan](https://docs.ros.org/api/sensor_msgs/html/msg/LaserScan.html), and [sensor_msgs/MultiEchoLaserScan](https://docs.ros.org/api/sensor_msgs/html/msg/MultiEchoLaserScan.html) can be converted.  Timestamps of the original messages are preserved in the Whisker observation log.

### ROS Topics

This utility displays the topics found in the ROS bag to assist in selection.  One observation log is produced per topic.  Multiple topics can be converted separately and have their observation logs replayed simultaneously by the `whisker_client_observation_playback` client.
