message("Processing ${CMAKE_CURRENT_LIST_FILE}")

add_subdirectory(drive_pca9685)

add_subdirectory(imu_mpu6050)

add_subdirectory(lidar_hokuyo)

add_subdirectory(lidar_sick)

add_subdirectory(observation_playback)
