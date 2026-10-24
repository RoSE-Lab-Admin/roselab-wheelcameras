#include <deque>
#include <memory>
#include <string>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "std_msgs/msg/header.hpp"

/*
 The purpose of this node is to replace the stamp on a single camera's compressed images and camera_info with the stamp
 published by the hardware_shutter node at the moment the trigger pulse was sent. usb_cam stamps each frame with the V4L2
 buffer time (when the frame reached the PI), which lags the actual exposure. Each frame is matched to the most recent
 trigger stamp that is at least min_latency and at most max_latency older than the frame's original stamp.
 usb_cam gives an image and its camera_info the same stamp, so whichever arrives first resolves the match and the other
 reuses it. Messages are taken as unique_ptrs and moved into publish so image data is never copied.
 Run one instance per camera.
*/

class ImageRestamper : public rclcpp::Node
{
public:
  ImageRestamper()
  : Node("image_restamper")
  {
    // Parameters
    this->declare_parameter<std::string>("input_image_topic", "image_raw/compressed");
    this->declare_parameter<std::string>("input_info_topic", "camera_info");
    this->declare_parameter<std::string>("output_image_topic", "synced/image_raw/compressed");
    this->declare_parameter<std::string>("output_info_topic", "synced/camera_info");
    this->declare_parameter<std::string>("trigger_topic", "/wheelcam/imageheader");
    this->declare_parameter<double>("min_latency", 0.0);
    this->declare_parameter<double>("max_latency", 0.1);
    this->declare_parameter<int>("trigger_buffer_size", 30);
    this->declare_parameter<bool>("drop_unmatched", true);

    std::string input_image_topic  = this->get_parameter("input_image_topic").as_string();
    std::string input_info_topic   = this->get_parameter("input_info_topic").as_string();
    std::string output_image_topic = this->get_parameter("output_image_topic").as_string();
    std::string output_info_topic  = this->get_parameter("output_info_topic").as_string();
    std::string trigger_topic      = this->get_parameter("trigger_topic").as_string();
    min_latency_ = rclcpp::Duration::from_seconds(this->get_parameter("min_latency").as_double());
    max_latency_ = rclcpp::Duration::from_seconds(this->get_parameter("max_latency").as_double());
    buffer_size_ = static_cast<size_t>(this->get_parameter("trigger_buffer_size").as_int());
    drop_unmatched_ = this->get_parameter("drop_unmatched").as_bool();

    // Publishers
    image_pub_ = this->create_publisher<sensor_msgs::msg::CompressedImage>(
      output_image_topic, rclcpp::SensorDataQoS());
    info_pub_ = this->create_publisher<sensor_msgs::msg::CameraInfo>(
      output_info_topic, rclcpp::SensorDataQoS());

    // Subscribers
    trigger_sub_ = this->create_subscription<std_msgs::msg::Header>(
      trigger_topic, 10,
      [this](std_msgs::msg::Header::UniquePtr msg) {
        triggers_.emplace_back(msg->stamp);
        if (triggers_.size() > buffer_size_) {
          triggers_.pop_front();
        }
      });
    image_sub_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
      input_image_topic, rclcpp::SensorDataQoS(),
      [this](sensor_msgs::msg::CompressedImage::UniquePtr msg) {restamp_and_publish(std::move(msg), image_pub_);});
    info_sub_ = this->create_subscription<sensor_msgs::msg::CameraInfo>(
      input_info_topic, rclcpp::SensorDataQoS(),
      [this](sensor_msgs::msg::CameraInfo::UniquePtr msg) {restamp_and_publish(std::move(msg), info_pub_);});

    RCLCPP_INFO(this->get_logger(),
      "ImageRestamper started: '%s' -> '%s', '%s' -> '%s' using trigger '%s', latency window [%.3f, %.3f] s",
      image_sub_->get_topic_name(), image_pub_->get_topic_name(),
      info_sub_->get_topic_name(), info_pub_->get_topic_name(),
      trigger_sub_->get_topic_name(), min_latency_.seconds(), max_latency_.seconds());
  }

private:
  struct Resolved
  {
    rclcpp::Time original;
    rclcpp::Time trigger;
    bool matched;
  };

  template<typename MsgT>
  void restamp_and_publish(
    std::unique_ptr<MsgT> msg, const typename rclcpp::Publisher<MsgT>::SharedPtr & pub)
  {
    const Resolved & r = resolve(rclcpp::Time(msg->header.stamp));
    if (r.matched) {
      msg->header.stamp = r.trigger;
    } else if (drop_unmatched_) {
      return;
    }
    pub->publish(std::move(msg));
  }

  // Look up the trigger stamp for an original frame stamp, matching it against the trigger buffer the first time it is seen
  const Resolved & resolve(const rclcpp::Time & original)
  {
    for (auto it = resolved_.rbegin(); it != resolved_.rend(); ++it) {
      if (it->original == original) {
        return *it;
      }
    }

    // Find the newest trigger inside the latency window before this frame
    Resolved r{original, original, false};
    for (auto it = triggers_.rbegin(); it != triggers_.rend(); ++it) {
      const rclcpp::Duration latency = original - *it;
      if (latency < min_latency_) {
        continue;
      }
      if (latency <= max_latency_) {
        r.trigger = *it;
        r.matched = true;
      }
      break;
    }

    if (!r.matched) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        "No trigger stamp within latency window for frame at %.6f", original.seconds());
    } else if (has_last_trigger_ && r.trigger == last_trigger_) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 2000,
        "Trigger stamp %.6f matched more than one frame", r.trigger.seconds());
      r.matched = false;
    } else {
      RCLCPP_DEBUG(this->get_logger(), "Frame latency after trigger: %.6f s", (original - r.trigger).seconds());
      last_trigger_ = r.trigger;
      has_last_trigger_ = true;
    }

    resolved_.push_back(r);
    if (resolved_.size() > kResolvedHistory) {
      resolved_.pop_front();
    }
    return resolved_.back();
  }

  static constexpr size_t kResolvedHistory = 8;

  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr image_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_;
  rclcpp::Subscription<std_msgs::msg::Header>::SharedPtr trigger_sub_;
  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr image_sub_;
  rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr info_sub_;
  std::deque<rclcpp::Time> triggers_;
  std::deque<Resolved> resolved_;
  rclcpp::Time last_trigger_;
  bool has_last_trigger_ = false;
  rclcpp::Duration min_latency_{0, 0};
  rclcpp::Duration max_latency_{0, 0};
  size_t buffer_size_;
  bool drop_unmatched_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ImageRestamper>());
  rclcpp::shutdown();
  return 0;
}
