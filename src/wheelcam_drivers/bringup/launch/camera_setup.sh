#!/bin/bash
set -e

# Set the video device - change if camera is on a different node
DEVICE=${1:-/dev/video4}

echo "Configuring Arducam OV9782 on $DEVICE for external trigger mode"

# Disable auto exposure (required before setting manual exposure time)
v4l2-ctl -d "$DEVICE" -c auto_exposure=1
# Set exposure time (units = 100µs, so 500 = 5ms). Tune for lighting conditions.
v4l2-ctl -d "$DEVICE" -c exposure_time_absolute=500
# Enable external trigger snapshot mode
v4l2-ctl -d "$DEVICE" -c exposure_dynamic_framerate=1

echo "Camera setup complete"