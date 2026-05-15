#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include <gpiod.h>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/header.hpp"

using namespace std::chrono_literals;

/*
 The purpose of this node is to trigger the hardware shutter on the Arducam OV9782 cameras. This allows the cameras
 to be hardware synced by the PI via the shared GPIO pin. This node will also publish a timestamp that another node will stamp onto images
 to ensure all images are synced on the robot side.
*/

class HardwareShutter : public rclcpp::Node
{
public:
  HardwareShutter()
  : Node("hardware_shutter")
  {
    // Parameters
    this->declare_parameter<double>("framerate", 30.0);
    this->declare_parameter<std::string>("output_topic", "image_header");
    this->declare_parameter<std::string>("gpio_chip", "/dev/gpiochip4");
    this->declare_parameter<int>("gpio_pin", 3);
    this->declare_parameter<int>("pulse_duration_us", 1000);

    double framerate          = this->get_parameter("framerate").as_double();
    std::string output_topic  = this->get_parameter("output_topic").as_string();
    std::string gpio_chip_dev = this->get_parameter("gpio_chip").as_string();
    gpio_pin_                 = static_cast<unsigned int>(this->get_parameter("gpio_pin").as_int());
    pulse_duration_us_        = this->get_parameter("pulse_duration_us").as_int();

    // GPIO setup
    chip_ = gpiod_chip_open(gpio_chip_dev.c_str());
    if (!chip_) {
      throw std::runtime_error("Failed to open GPIO chip: " + gpio_chip_dev);
    }
    line_ = gpiod_chip_get_line(chip_, gpio_pin_);
    if (!line_) {
      gpiod_chip_close(chip_);
      throw std::runtime_error("Failed to get GPIO line");
    }
    if (gpiod_line_request_output(line_, "hardware_shutter", 0) < 0) {
      gpiod_chip_close(chip_);
      throw std::runtime_error("Failed to request GPIO line as output");
    }

    // Publisher
    publisher_ = this->create_publisher<std_msgs::msg::Header>(output_topic, 10);

    // Timer — period derived from framerate
    auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(1.0 / framerate));
    timer_ = this->create_wall_timer(
      period, std::bind(&HardwareShutter::timer_callback, this));

    RCLCPP_INFO(this->get_logger(),
      "HardwareShutter started: %.1f Hz, topic '%s', GPIO chip '%s' pin %u, pulse %d us",
      framerate, output_topic.c_str(), gpio_chip_dev.c_str(), gpio_pin_, pulse_duration_us_);
  }

  ~HardwareShutter()
  {
    if (line_) {
      gpiod_line_set_value(line_, 0);
      gpiod_line_release(line_);
    }
    if (chip_) {
      gpiod_chip_close(chip_);
    }
  }

private:
  void timer_callback()
  {
    // Capture timestamp at the rising edge
    auto msg = std_msgs::msg::Header();
    msg.stamp = this->now();
    msg.frame_id = "camera";

    // Pulse the GPIO pin
    gpiod_line_set_value(line_, 1);
    rclcpp::sleep_for(std::chrono::microseconds(pulse_duration_us_));
    gpiod_line_set_value(line_, 0);

    // Publish header
    publisher_->publish(msg);
  }

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::Header>::SharedPtr publisher_;
  gpiod_chip * chip_;
  gpiod_line * line_;
  unsigned int gpio_pin_;
  int pulse_duration_us_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<HardwareShutter>());
  rclcpp::shutdown();
  return 0;
}