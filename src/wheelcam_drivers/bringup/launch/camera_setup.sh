#!/bin/bash
# No set -e: a control failing on one camera should not stop the rest from being configured

# Set the video devices - pass one or more device paths, defaults to /dev/video0
DEVICES=("${@:-/dev/video0}")

for DEVICE in "${DEVICES[@]}"; do
  echo "Configuring Arducam OV9782 on $DEVICE for external trigger mode"

  # Image settings
  v4l2-ctl -d "$DEVICE" -c brightness=0
  v4l2-ctl -d "$DEVICE" -c contrast=32
  v4l2-ctl -d "$DEVICE" -c saturation=64
  # Fixed white balance (auto must be disabled before setting the temperature)
  v4l2-ctl -d "$DEVICE" -c white_balance_automatic=0
  v4l2-ctl -d "$DEVICE" -c white_balance_temperature=6000
  # Disable auto exposure (required before setting manual exposure time)
  v4l2-ctl -d "$DEVICE" -c auto_exposure=1
  # Set exposure time (units = 100µs, so 1000 = 100ms). Tune for lighting conditions.
  v4l2-ctl -d "$DEVICE" -c exposure_time_absolute=1000
  # Enable external trigger snapshot mode
  v4l2-ctl -d "$DEVICE" -c exposure_dynamic_framerate=1
done

echo "Camera setup complete"
