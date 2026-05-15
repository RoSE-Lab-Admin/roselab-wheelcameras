#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source ROS 2
source /opt/ros/jazzy/setup.bash

# Initialize rosdep (skip if already done)
if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then
  sudo rosdep init
fi

# Register custom rosdep source
echo "yaml file://${SCRIPT_DIR}/rosdep.yaml" \
  | sudo tee /etc/ros/rosdep/sources.list.d/50-wheelcam.list

rosdep update

# Install dependencies
rosdep install --from-paths "${SCRIPT_DIR}/src" --ignore-src -r -y

# Build
cd "${SCRIPT_DIR}"
colcon build