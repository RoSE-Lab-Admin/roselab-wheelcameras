import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, RegisterEventHandler, ExecuteProcess, TimerAction
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, FindExecutable, PathJoinSubstitution, LaunchConfiguration

from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():

    # get camera port paths based on usb indexes
    cam_0_dev = os.path.realpath("/dev/v4l/by-path/platform-xhci-hcd.1-usb-0:1:1.0-video-index0")
    cam_1_dev = os.path.realpath("/dev/v4l/by-path/platform-xhci-hcd.0-usb-0:1:1.0-video-index0")
    cam_2_dev = os.path.realpath("/dev/v4l/by-path/platform-xhci-hcd.1-usb-0:2:1.0-video-index0")
    cam_3_dev = os.path.realpath("/dev/v4l/by-path/platform-xhci-hcd.0-usb-0:2:1.0-video-index0")

    best_effort_qos = {
    "qos_overrides.image_raw.publisher.reliability": "best_effort",
    "qos_overrides.image_raw.publisher.durability": "volatile",
    "qos_overrides.image_raw.publisher.depth": 1,
    "qos_overrides.camera_info.publisher.reliability": "best_effort",
    "qos_overrides.camera_info.publisher.durability": "volatile",
    "qos_overrides.camera_info.publisher.depth": 1,
    }

    # get camera hardware parameters
    hardware_params = PathJoinSubstitution(
        [
            FindPackageShare("wheelcam_drivers"),
            "bringup",
            "config",
            "hardware_shutter.yaml",
        ]
    )

    camera_params = PathJoinSubstitution(
        [
            FindPackageShare("wheelcam_drivers"),
            "bringup",
            "config",
            "camera_params.yaml",
        ]
    )

    #get path to camera setup script
    camera_script = PathJoinSubstitution(
        [
            FindPackageShare("wheelcam_drivers"),
            "bringup",
            "launch",
            "camera_setup.sh",
        ]
    )

    #camera setup shell script
    camera_startup = ExecuteProcess(
            cmd=[camera_script],
            shell=True,
            output='screen'
        )

    shutter = Node(
        package="wheelcam_drivers",
        executable="hardware_shutter",
        parameters=[hardware_params],
        output="both",
    )

    # cam_params = {
    #     "pixel_format": "mjpeg2rgb",
    #     "image_width": 800,
    #     "image_height": 600,
    #     "framerate": 60.0,
    #     "autoexposure": False,
    #     "exposure": 500,
    # }

    cam_0 = Node(
        package="usb_cam",
        executable="usb_cam_node_exe",
        name="cam_0",
        namespace="wheelcam/cam_0",
        parameters=[camera_params, {"video_device": cam_0_dev}, best_effort_qos],
        output="both",
    )

    cam_1 = Node(
        package="usb_cam",
        executable="usb_cam_node_exe",
        name="cam_1",
        namespace="wheelcam/cam_1",
        parameters=[camera_params, {"video_device": cam_1_dev}, best_effort_qos],
        output="both",
    )

    cam_2 = Node(
        package="usb_cam",
        executable="usb_cam_node_exe",
        name="cam_2",
        namespace="wheelcam/cam_2",
        parameters=[camera_params, {"video_device": cam_2_dev}, best_effort_qos],
        output="both",
    )

    cam_3 = Node(
        package="usb_cam",
        executable="usb_cam_node_exe",
        name="cam_3",
        namespace="wheelcam/cam_3",
        parameters=[camera_params, {"video_device": cam_3_dev}, best_effort_qos],
        output="both",
    )

    set_trigger_mode = TimerAction(
        period=2.0,
        actions=[
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_0_dev, '-c', 'auto_exposure=1'], output='screen'),
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_1_dev, '-c', 'auto_exposure=1'], output='screen'),
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_2_dev, '-c', 'auto_exposure=1'], output='screen'),
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_3_dev, '-c', 'auto_exposure=1'], output='screen'),

            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_0_dev, '-c', 'exposure_time_absolute=500'], output='screen'),
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_1_dev, '-c', 'exposure_time_absolute=500'], output='screen'),
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_2_dev, '-c', 'exposure_time_absolute=500'], output='screen'),
            # ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_3_dev, '-c', 'exposure_time_absolute=500'], output='screen'),

            ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_0_dev, '-c', 'exposure_dynamic_framerate=1'], output='screen'),
            ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_1_dev, '-c', 'exposure_dynamic_framerate=1'], output='screen'),
            ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_2_dev, '-c', 'exposure_dynamic_framerate=1'], output='screen'),
            ExecuteProcess(cmd=['v4l2-ctl', '-d', cam_3_dev, '-c', 'exposure_dynamic_framerate=1'], output='screen'),
        ]
    )

    return LaunchDescription([
        camera_startup,
        RegisterEventHandler(
            OnProcessExit(
                target_action=camera_startup,
                on_exit=[
                    shutter,
                    cam_0,
                    cam_1,
                    cam_2,
                    cam_3,
                    set_trigger_mode,
                ],
            )
        ),
    ])