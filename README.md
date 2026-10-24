# roselab-wheelcameras
ROS 2 drivers and launch files for the RoSE Lab CubeRover's hardware-synchronized wheel cameras


[![ROS 2 Jazzy](https://img.shields.io/badge/ROS%202-Jazzy-blue?logo=ros&logoColor=white)](https://docs.ros.org/en/jazzy/)
[![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu-24.04-E95420?logo=ubuntu&logoColor=white)](https://ubuntu.com/download/raspberry-pi)
[![Raspberry Pi 5](https://img.shields.io/badge/Raspberry%20Pi-5-A22846?logo=raspberrypi&logoColor=white)](https://www.raspberrypi.com/products/raspberry-pi-5/)


## 📷 Overview

This workspace runs **four Arducam OV9782 global-shutter USB cameras** from a Raspberry Pi 5. The Pi pulses a GPIO pin to trigger every camera at the same instant, and every frame is re-stamped with the time of the trigger pulse. Images from all four cameras taken at the same moment therefore carry the same timestamp.

```
                 GPIO pulse (shared trigger line)
  hardware_shutter ───────────────────────────────► cam_0 … cam_3 (OV9782)
        │                                                  │ USB (MJPEG)
        │ /wheelcam/imageheader                            ▼
        │ (trigger timestamp)                       usb_cam_node × 4
        │                                                  │ image_raw/compressed
        └──────────────────────► image_restamper × 4 ◄─────┘ camera_info
                                          │
                                          ▼
                          /wheelcam/cam_N/synced/image_raw/compressed
                          /wheelcam/cam_N/synced/camera_info
```


## 🚀 Quick Start

### 1. Prerequisites
- **Raspberry Pi 5** running **Ubuntu 24.04** (ROS 2 Jazzy's supported platform)
- **4× Arducam OV9782** USB cameras with their external trigger inputs wired to the Pi (see [Hardware Setup](#-hardware-setup))
- **Git**, with internet access for `apt` and `rosdep`

> [!NOTE]
> The workspace will build on any Ubuntu 24.04 machine, but the launch file expects the Pi 5 USB port layout and GPIO chip. It will not run without the camera hardware.

### 2. Install ROS 2 Jazzy
*Skip this step if `/opt/ros/jazzy` already exists.*

The included `rosinstall.sh` adds the ROS 2 apt repository and installs `ros-jazzy-ros-base`:
```bash
./rosinstall.sh
```

Then install the ROS developer tools (`colcon`, `rosdep`), which `ros-base` does not include:
```bash
sudo apt install ros-dev-tools
```

### 3. Clone the Repository
This repo uses a **git submodule** for the lab's fork of [`usb_cam`](https://github.com/RoSE-Lab-Admin/usb_cam). Clone with `--recurse-submodules`:
```bash
git clone --recurse-submodules https://github.com/RoSE-Lab-Admin/roselab-wheelcameras.git
cd roselab-wheelcameras
```

> [!WARNING]
> If you already cloned without submodules, `src/usb_cam` will be empty and the build will fail. Fix it with:
> ```bash
> git submodule update --init --recursive
> ```

### 4. Install Dependencies & Build
`setup.sh` does the following:
1. Sources ROS 2 Jazzy.
2. Initializes `rosdep` (only on first run).
3. Registers the custom `rosdep.yaml` source, which maps `v4l-util` → `v4l-utils`.
4. Installs all package dependencies with `rosdep`.
5. Builds the workspace with `colcon build`.

```bash
./setup.sh
```

> [!NOTE]
> `setup.sh` uses `sudo` for `rosdep init`, for registering the rosdep source, and for installing apt packages. Expect a password prompt.

### 5. Source the Workspace
```bash
source install/setup.bash
```

> [!TIP]
> To source ROS and the workspace automatically in every new terminal, add these lines to `~/.bashrc`:
> ```bash
> source /opt/ros/jazzy/setup.bash
> source ~/roselab-wheelcameras/install/setup.bash
> ```

### 6. Launch the Cameras
```bash
ros2 launch wheelcam_drivers camera_startup.launch.py
```


## 🏗️ System Architecture

### 💠 Packages

| Package | Path | Role |
| :--- | :--- | :--- |
| **wheelcam_drivers** | `src/wheelcam_drivers` | Hardware trigger node, timestamp re-stamper, launch and config files |
| **usb_cam** *(submodule)* | `src/usb_cam` | RoSE Lab fork of [ros-drivers/usb_cam](https://github.com/ros-drivers/usb_cam). V4L2 camera driver that publishes images |

### 🧩 Nodes

| Node | Executable | Description |
| :--- | :--- | :--- |
| `hardware_shutter` | `wheelcam_drivers/hardware_shutter` | Pulses a GPIO line through `libgpiod` at a fixed rate to trigger all cameras. Publishes a `std_msgs/Header` with the pulse timestamp on each trigger. |
| `cam_0` … `cam_3` | `usb_cam/usb_cam_node_exe` | One driver per camera. Publishes MJPEG images and `camera_info` using the per-camera calibration files. |
| `cam_N_restamper` | `wheelcam_drivers/image_restamper` | Matches each frame to the most recent trigger within a latency window and replaces the frame's stamp with the trigger stamp. Unmatched frames are dropped by default. |

### 📡 Topics

| Topic | Type | Description |
| :--- | :--- | :--- |
| `/wheelcam/imageheader` | `std_msgs/Header` | Trigger timestamps from `hardware_shutter` |
| `/wheelcam/cam_N/image_raw/compressed` | `sensor_msgs/CompressedImage` | Raw frames with the driver's stamps (time each frame reached the Pi) |
| `/wheelcam/cam_N/camera_info` | `sensor_msgs/CameraInfo` | Calibration with the driver's stamps |
| `/wheelcam/cam_N/synced/image_raw/compressed` | `sensor_msgs/CompressedImage` | **Frames stamped with the trigger time. Use these.** |
| `/wheelcam/cam_N/synced/camera_info` | `sensor_msgs/CameraInfo` | Calibration stamped with the trigger time |

*`N` = `0`–`3`*

### 🚦 Launch Sequence
`camera_startup.launch.py` runs the following steps in order:
1. `camera_setup.sh` puts `/dev/video0` into manual-exposure, external-trigger mode.
2. After that script exits, it starts `hardware_shutter`, the four `usb_cam` nodes and the four restampers.
3. After 2 seconds, it applies `v4l2-ctl` image settings to every camera: brightness, contrast, saturation, fixed white balance (6000 K), manual exposure and trigger mode.


## 🔌 Hardware Setup

### 🔗 USB Port Mapping
The launch file finds each camera by its **physical USB port** (`/dev/v4l/by-path/...`) rather than by `/dev/videoN`, so camera IDs stay the same across reboots. **Each camera must be plugged into the correct port.**

| Camera | USB by-path |
| :--- | :--- |
| `cam_0` | `platform-xhci-hcd.1-usb-0:1:1.0-video-index0` |
| `cam_1` | `platform-xhci-hcd.0-usb-0:1:1.0-video-index0` |
| `cam_2` | `platform-xhci-hcd.1-usb-0:2:1.0-video-index0` |
| `cam_3` | `platform-xhci-hcd.0-usb-0:2:1.0-video-index0` |

To verify which port each camera is on:
```bash
ls -l /dev/v4l/by-path/
v4l2-ctl --list-devices
```

### ⚡ Trigger Wiring
- Connect the external trigger input of every camera to **BCM GPIO 3** (physical header pin 5) and connect the camera grounds to a Pi **GND** pin.
- On the Pi 5, the header GPIOs live on `/dev/gpiochip4`. Some newer Pi 5 kernels expose them as `/dev/gpiochip0`. Run `gpiodetect` to check, and update `gpio_chip` in `hardware_shutter.yaml` if needed.

### 🔐 Permissions
Your user needs access to the camera and GPIO devices:
```bash
sudo usermod -aG video,dialout $USER
# Log out and log back in for this to take effect!
```
If `hardware_shutter` reports `Failed to open GPIO chip`, check the device permissions with `ls -l /dev/gpiochip*`. You may need a `gpio` group or a udev rule that grants your user access.


## ⚙️ Configuration

All config files are in `src/wheelcam_drivers/bringup/config/`.

| File | Purpose | Key Parameters |
| :--- | :--- | :--- |
| `hardware_shutter.yaml` | Trigger timing and GPIO | `framerate` (30 Hz), `gpio_chip`, `gpio_pin`, `pulse_duration_us`, `output_topic` |
| `camera_params.yaml` | Shared `usb_cam` driver settings for all 4 cameras | `image_width`/`image_height` (1280×800), `framerate`, `pixel_format` (`raw_mjpeg`), `exposure` |
| `camera_config/cam_N_calib.yaml` | Per-camera intrinsic calibration (`plumb_bob`) | `camera_matrix`, `distortion_coefficients`, `projection_matrix` |

Restamper parameters are set inline in the launch file:

| Parameter | Default | Description |
| :--- | :--- | :--- |
| `min_latency` / `max_latency` | `0.0` / `0.1` s | Allowed delay between a trigger and the frame arriving |
| `trigger_buffer_size` | `30` | Number of recent trigger stamps kept for matching |
| `drop_unmatched` | `true` | Drop frames with no matching trigger instead of publishing them with their original stamp |

> [!NOTE]
> The image settings applied with `v4l2-ctl` at the end of the launch (white balance, exposure, etc.) are **hard-coded in `camera_startup.launch.py`** and override `camera_params.yaml`. Edit the launch file to change them.

> [!IMPORTANT]
> The `framerate` in `hardware_shutter.yaml` sets the actual capture rate in trigger mode. The `usb_cam` `framerate` only needs to be high enough not to limit it.

After changing any config or launch file, rebuild so the installed copies update:
```bash
colcon build --packages-select wheelcam_drivers
```


## 🧪 Verifying It Works

1. **Check the trigger is running:**
    ```bash
    ros2 topic hz /wheelcam/imageheader
    ```
2. **Check each camera is publishing synced frames:**
    ```bash
    ros2 topic hz /wheelcam/cam_0/synced/image_raw/compressed
    ```
3. **View the images:**
    ```bash
    ros2 run rqt_image_view rqt_image_view
    ```
    *(GUI tools such as `rqt_image_view` are not in `ros-base`. Run them on a dev machine on the same network, or install `ros-jazzy-rqt-image-view`.)*

> [!TIP]
> `trigger.py` in `src/wheelcam_drivers` is a standalone Python script that toggles GPIO 3 with `RPi.GPIO`. It was used for early trigger tests and is not part of the launch.


## 🛠️ Troubleshooting
- **`usb_cam` package not found / build fails:** The submodule is not checked out. Run `git submodule update --init --recursive`.
- **Camera fails to open / wrong camera order:** A camera is in the wrong USB port, or the by-path names differ on your hardware. Compare `ls -l /dev/v4l/by-path/` with the paths in `camera_startup.launch.py`.
- **No images on `synced/` topics but `image_raw` works:** Frames are not matching a trigger. Check that `/wheelcam/imageheader` is publishing, and try a larger `max_latency`.
- **`No trigger stamp within latency window` warnings:** The camera-to-Pi latency is outside `[min_latency, max_latency]`, or the cameras are free-running instead of waiting for the trigger.
- **`Trigger stamp matched more than one frame` warnings:** The cameras are capturing faster than the trigger rate, so they are not in external trigger mode. Re-run the launch, or apply the `v4l2-ctl` settings manually.
- **Washed-out or tinted images:** Adjust `white_balance_temperature`, `exposure_time_absolute`, etc. in the `set_trigger_mode` block of the launch file. To list a camera's available controls:
    ```bash
    v4l2-ctl -d /dev/video0 --list-ctrls
    ```
- **`Failed to open GPIO chip`:** Wrong `gpio_chip` path or missing permissions. See [Trigger Wiring](#-trigger-wiring) and [Permissions](#-permissions).
- **`rosdep` cannot resolve `v4l-util`:** The custom rosdep source is not registered. Re-run `./setup.sh`.


## 📁 Repository Layout
```
roselab-wheelcameras/
├── rosinstall.sh            # Installs ROS 2 Jazzy (ros-base) from apt
├── setup.sh                 # rosdep install + colcon build
├── rosdep.yaml              # Custom rosdep key: v4l-util → v4l-utils
└── src/
    ├── usb_cam/             # Submodule: RoSE Lab fork of usb_cam
    └── wheelcam_drivers/
        ├── src/
        │   ├── hardware_shutter.cpp   # GPIO trigger + timestamp publisher
        │   └── image_restamper.cpp    # Re-stamps frames with trigger time
        ├── bringup/
        │   ├── config/                # Driver, trigger, and calibration YAMLs
        │   └── launch/                # camera_startup.launch.py, camera_setup.sh
        └── trigger.py                 # Standalone GPIO trigger test script
```
