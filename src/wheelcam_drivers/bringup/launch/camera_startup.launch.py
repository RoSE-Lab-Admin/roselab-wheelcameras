from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, RegisterEventHandler, ExecuteProcess, TimerAction
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, FindExecutable, PathJoinSubstitution, LaunchConfiguration

from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():

    # get camera hardware parameters
    hardware_params = PathJoinSubstitution(
        [
            FindPackageShare("wheelcam_drivers"),
            "bringup",
            "config",
            "hardware_shutter.yaml",
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

    v4l2_cam_1 = Node(
        package="usb_cam",
        executable="usb_cam_node_exe",
        parameters=[{
            "video_device": "/dev/video0",
            "pixel_format": "mjpeg2rgb",
            "image_width": 800,
            "image_height": 600,
            "autoexposure": False,
            "exposure": 500,
        }],
        output="both",
    )

    return LaunchDescription([
        camera_startup,
        RegisterEventHandler(
            OnProcessExit(
                target_action=camera_startup,
                on_exit=[
                    shutter,
                    v4l2_cam_1,
                    # Re-apply exposure_dynamic_framerate after usb_cam init
                    # (not exposed as a usb_cam parameter)
                    TimerAction(
                        period=2.0,
                        actions=[ExecuteProcess(
                            cmd=['v4l2-ctl', '-d', '/dev/video0',
                                 '-c', 'exposure_dynamic_framerate=1'],
                            output='screen'
                        )]
                    ),
                ],
            )
        ),
    ])