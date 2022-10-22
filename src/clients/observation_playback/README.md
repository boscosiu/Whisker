## Observation Log Playback Client

`whisker_client_observation_playback`

This component reads in Whisker observation log (`.obslog`) files and replays them to the server as if they came from connected sensor clients.

### Supported Platforms

Linux (GCC, Clang), macOS (Xcode Clang), Windows (MSVC)

### Command Line Options

- `--config=[config file path]`
  - Use JSON configuration file at the given path (default: `config.json` in current directory)
- `--id=[client ID]`
  - Specify the vehicle-wide unique ID used to identify this component to the server (default: `playback0`)
- `--logs=[log file paths]`
  - Comma-separated paths of log files to play back (default: `input.obslog` in current directory)
- `--startoffset=[offset]`
  - Number of milliseconds from log start at which to start playback (default: `0`)
- `--stopoffset=[offset]`
  - Number of milliseconds from log start at which to stop playback (0 = no limit) (default: `0`)
- `--realtime`/`--norealtime`
  - Whether to play back observations at the rate at which they were recorded (default: `--realtime`)

### Configuration

There are no component-specific settings.  However, a configuration file with the [vehicle settings](../../../README.md#vehiclesclients) still needs to be provided.

### Playback Rate

In `--realtime` mode, the timing at which observation messages are played back to the server matches the log's timing.  The result is a reconstruction of sensor readings as seen by the server over the recording period.  If the client is unable to keep up with the log's message rate, then messages delayed by latency will be dropped to preserve the timing.

In `--norealtime` mode, the entirety of the message log is sent to the server at once with no consideration for timing.  The server will then process this data like an offline SLAM system.

### Synchronized Log Playback

If multiple log files are given, then messages from each log will be played back simultaneously synchronized by their timestamps.  This is accomplished by inserting an appropriate delay before the start of each log so that their messages are aligned in time.  Because of this, the provided logs should be recorded at the same time from the same vehicle or [converted](../../utils/convert_rosbag/README.md) from the same ROS bag.

The `--startoffset` and `--stopoffset` values are applied to each log individually.
