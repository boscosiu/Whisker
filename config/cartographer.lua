include "map_builder.lua"
include "trajectory_builder.lua"

MAP_BUILDER.use_trajectory_builder_2d = true
MAP_BUILDER.num_background_threads = 4
MAP_BUILDER.collate_by_trajectory = true
MAP_BUILDER.pose_graph.constraint_builder.log_matches = false

TRAJECTORY_BUILDER_2D.min_range = 0.02
TRAJECTORY_BUILDER_2D.max_range = 60.

return {
    map_builder = MAP_BUILDER,
    trajectory_builder = TRAJECTORY_BUILDER
}
