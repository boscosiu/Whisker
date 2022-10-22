## Server Component

`whisker_server`

The server is the central computation and communications hub in a Whisker deployment.  It is primarily responsible for performing SLAM from sensor client data and maintaining system state.

[Cartographer](https://github.com/cartographer-project/cartographer) is used as the SLAM backend, so many of the data structures in the codebase have ties to Cartographer terms and concepts.

### Supported Platforms

Linux (GCC, Clang), macOS (Xcode Clang), Windows (MSVC)

### Command Line Options

- `--config=[config file path]`
  - Use JSON configuration file at the given path (default: `config.json` in current directory)

### Configuration

In addition to this JSON config file, a [Lua config file](#cartographer-config) for Cartographer must be provided.  Samples of these files are available at [/config/config.json](../../config/config.json) and [/config/cartographer.lua](../../config/cartographer.lua).

```json5
// config.json
{
  "server": {
    "resource_dir": "./resources",
    "cartographer": {
      "config_file": "./cartographer.lua",
      "base_config_dir": "./cartographer_base_config",
      "pure_localization_num_submaps": 3,
      "overlapping_trimmer_fresh_submaps_count": 1,
      "overlapping_trimmer_min_covered_area": 2,
      "overlapping_trimmer_min_added_submaps_count": 5
    },
    "client_service": {
      "websocket": {
        "enabled": false,
        "port": 9002
      },
      "zeromq": {
        "enabled": true,
        "bind_address": "tcp://*:9000"
      }
    },
    "console_service": {
      "websocket": {
        "enabled": true,
        "port": 9001,
        "root": "./whisker_console.zip"
      },
      "zeromq": {
        "enabled": false,
        "bind_address": "tcp://*:9003"
      }
    }
  }
}
```

| Key               | Type   |                                                                                                         |
|-------------------|--------|---------------------------------------------------------------------------------------------------------|
| `resource_dir`    | string | Directory from which to read/write saved maps and observation logs                                      |
| `cartographer`    | object | Config object for Cartographer (see [Cartographer Config](#cartographer-config))                        |
| `client_service`  | object | Config object for handler of client messages (see [Message Handler Configs](#message-handler-configs))  |
| `console_service` | object | Config object for handler of console messages (see [Message Handler Configs](#message-handler-configs)) |

#### Cartographer Config

| Key                                           | Type   |                                                                                                                           |
|-----------------------------------------------|--------|---------------------------------------------------------------------------------------------------------------------------|
| `config_file`                                 | string | Location of Cartographer config file <sup>1</sup>                                                                         |
| `base_config_dir`                             | string | Location of the default set of Cartographer config files <sup>2</sup>                                                     |
| `pure_localization_num_submaps`               | number | Localization Trimmer: Number of most recent submaps to keep                                                               |
| `overlapping_trimmer_fresh_submaps_count`     | number | Overlapping Trimmer: If an area has more than this number of submaps, the stale ones are considered to not cover the area |
| `overlapping_trimmer_min_covered_area`        | number | Overlapping Trimmer: Trim submaps which cover an area less than this number of square meters                              |
| `overlapping_trimmer_min_added_submaps_count` | number | Overlapping Trimmer: Number of added submaps before trimmer is invoked                                                    |

<sup>1</sup> This is a Lua config file used by the Cartographer SLAM backend, parts of which is documented in the [Cartographer docs](https://google-cartographer.readthedocs.io/en/latest/configuration.html).\
<sup>2</sup> These config files are copied to the output directory during the build process.

#### Message Handler Configs

##### WebSocket

| Key       | Type    |                                                                                                        |
|-----------|---------|--------------------------------------------------------------------------------------------------------|
| `enabled` | boolean | Whether this handler should accept WebSocket connections                                               |
| `port`    | number  | Port number to accept connections on                                                                   |
| `root`    | string  | Directory or archive to serve files from for HTTP requests (console message handler only) <sup>1</sup> |

<sup>1</sup> Files from a directory or archive can be served if an HTTP request is received on the WebSocket port.  This is useful for serving the console app that the `whisker_console` build process produces.

##### ZeroMQ

| Key            | Type    |                                                       |
|----------------|---------|-------------------------------------------------------|
| `enabled`      | boolean | Whether this handler should accept ZeroMQ connections |
| `bind_address` | string  | ZeroMQ bind endpoint to accept connections on         |

### Cartographer Performance

In general, local SLAM is sensitive to the system's single-threaded performance.  Global SLAM constraint building and loop closure are more dependent on the number of cores brought to bear on the optimization problem.  The Cartographer docs have [more information](https://google-cartographer-ros.readthedocs.io/en/latest/tuning.html#low-latency) on the tradeoffs one could make to tune its performance with the Lua config.

The `MAP_BUILDER.num_background_threads = 4` value in particular is worth adjusting if the server has many cores and few maps.
