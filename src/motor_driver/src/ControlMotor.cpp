// // ĐANG CHAY ĐƯỢC 26/4

// #include "rclcpp/rclcpp.hpp"
// #include "rcl_interfaces/srv/set_parameters.hpp"
// #include "std_msgs/msg/float32.hpp"
// #include "std_msgs/msg/bool.hpp"
// #include <vector>
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <functional>
// #include <stdexcept>
// #include "can_interface/caninterface.hpp"

// // Giao diện trừu tượng cho CAN
// class ICANInterface {
// public:
//     virtual bool setOrigin(uint8_t id, uint8_t mode) = 0;
//     virtual bool setPositionSpeed(uint8_t id, float pos, int speed, int accel) = 0;
//     virtual bool setCurrent(uint8_t id, float current) = 0;
//     virtual bool receive(uint32_t& id, std::vector<uint8_t>& data) = 0;
//     virtual void decodeMotorData(const std::vector<uint8_t>& data, float& pos, float& vel, 
//                                 float& cur, int8_t& temp, int8_t& err) = 0;
//     virtual ~ICANInterface() = default;
// };

// // Triển khai CANInterface kế thừa ICANInterface
// class CANInterfaceWrapper : public ICANInterface {
// public:
//     CANInterfaceWrapper(const std::string& interface_name) : can_(interface_name) {}

//     bool setOrigin(uint8_t id, uint8_t mode) override {
//         try {
//             can_.setOrigin(id, mode);
//             return true;
//         } catch (const std::exception& e) {
//             std::cerr << "SetOrigin failed: " << e.what() << std::endl;
//             return false;
//         }
//     }

//     bool setPositionSpeed(uint8_t id, float pos, int speed, int accel) override {
//         try {
//             can_.setPositionSpeed(id, pos, speed, accel);
//             return true;
//         } catch (const std::exception& e) {
//             std::cerr << "SetPositionSpeed failed: " << e.what() << std::endl;
//             return false;
//         }
//     }

//     bool setCurrent(uint8_t id, float current) override {
//         try {
//             can_.setCurrent(id, current);
//             return true;
//         } catch (const std::exception& e) {
//             std::cerr << "SetCurrent failed: " << e.what() << std::endl;
//             return false;
//         }
//     }

//     bool receive(uint32_t& id, std::vector<uint8_t>& data) override {
//         return can_.receive(id, data);
//     }

//     void decodeMotorData(const std::vector<uint8_t>& data, float& pos, float& vel, 
//                          float& cur, int8_t& temp, int8_t& err) override {
//         can_.decodeMotorData(data, pos, vel, cur, temp, err);
//     }

// private:
//     CANInterface can_;
// };

// // Cấu trúc dữ liệu động cơ
// struct MotorData {
//     float position = 0.0;  // degrees
//     float velocity = 0.0;  // rpm
//     float current = 0.0;   // mA
//     int8_t temperature = 0; // °C
//     int8_t error = 0;      // error code
// };

// // Giao diện điều khiển động cơ
// class IMotorController {
// public:
//     virtual void control(const MotorData& data) = 0;
//     virtual void setParameters(const std::vector<rcl_interfaces::msg::Parameter>& parameters) = 0;
//     virtual bool isOriginSet() const = 0;
//     virtual bool isTargetReached() const = 0;
//     virtual void stop() = 0;
//     virtual uint8_t motorId() const = 0;
//     virtual ~IMotorController() = default;
// };

// // Lớp kiểm tra an toàn
// class SafetyChecker {
// public:
//     SafetyChecker(float min_angle, float max_angle, int max_temperature)
//         : min_angle_(min_angle), max_angle_(max_angle), max_temperature_(max_temperature) {}

//     bool check(const MotorData& data, const rclcpp::Logger& logger) const {
//         if (data.temperature > max_temperature_ || data.error != 0) {
//             RCLCPP_ERROR(logger, "Motor issue! Temp: %d°C, Error: %d", data.temperature, data.error);
//             return false;
//         }
//         if (data.position < min_angle_ || data.position > max_angle_) {
//             RCLCPP_ERROR(logger, "Position %.2f out of bounds [%.2f, %.2f]", 
//                          data.position, min_angle_, max_angle_);
//             return false;
//         }
//         return true;
//     }

// private:
//     float min_angle_;
//     float max_angle_;
//     int max_temperature_;
// };

// // Lớp xuất bản trạng thái
// class StatePublisher {
// public:
//     StatePublisher(rclcpp::Node* node, const std::string& name)
//         : position_publisher_(node->create_publisher<std_msgs::msg::Float32>(name + "_position", 10)),
//           velocity_publisher_(node->create_publisher<std_msgs::msg::Float32>(name + "_vel_actual", 10)),
//           reached_publisher_(node->create_publisher<std_msgs::msg::Bool>(name + "_target_reached", 10)) {}

//     void publish(const MotorData& data, bool target_reached) {
//         std_msgs::msg::Float32 pos_msg;
//         pos_msg.data = data.position;
//         position_publisher_->publish(pos_msg);

//         std_msgs::msg::Float32 vel_msg;
//         vel_msg.data = data.velocity;
//         velocity_publisher_->publish(vel_msg);

//         std_msgs::msg::Bool reached_msg;
//         reached_msg.data = target_reached;
//         reached_publisher_->publish(reached_msg);
//     }

// private:
//     rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr position_publisher_;
//     rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr velocity_publisher_;
//     rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr reached_publisher_;
// };

// // Lớp điều khiển động cơ
// class MotorController : public IMotorController {
// public:
//     MotorController(uint8_t motor_id, std::shared_ptr<ICANInterface> can, 
//                     rclcpp::Node* node, const std::string& name)
//         : motor_id_(motor_id), can_(can), node_(node), logger_(node->get_logger()),
//           state_(State::INITIALIZING), is_origin_set_(false), is_home_(false),
//           current_target_index_(0), reach_counter_(0), target_reached_(true),
//           speed_(10000), accel_(1000), min_angle_(0.0), max_angle_(90.0),
//           reach_count_max_(100), safety_checker_(min_angle_, max_angle_, MAX_MOTOR_TEMPERATURE),
//           state_publisher_(node, name), param_prefix_(name + ".") {
//         initializeParameters();
//         setOrigin();
//     }

//     void control(const MotorData& data) override {
//         if (!safety_checker_.check(data, logger_)) {
//             stop();
//             return;
//         }

//         state_publisher_.publish(data, target_reached_);

//         switch (state_) {
//             case State::INITIALIZING:
//                 if (std::abs(data.position) < POSITION_TOLERANCE && 
//                     std::abs(data.velocity) < VELOCITY_TOLERANCE) {
//                     is_origin_set_ = true;
//                     transitionTo(State::HOMING);
//                 }
//                 break;
//             case State::HOMING:
//                 can_->setPositionSpeed(motor_id_, 0.0, speed_, accel_);
//                 if (std::abs(data.position) < POSITION_TOLERANCE && 
//                     std::abs(data.velocity) < VELOCITY_TOLERANCE) {
//                     is_home_ = true;
//                     target_reached_ = true;
//                     RCLCPP_INFO(logger_, "Motor 0x%02X reached Home Position!", motor_id_);
//                     transitionTo(State::MOVING);
//                 }
//                 break;
//             case State::MOVING: {
//                 if (target_positions_.empty()) {
//                     RCLCPP_ERROR(logger_, "Motor 0x%02X: target_positions is empty!", motor_id_);
//                     stop();
//                     transitionTo(State::STOPPED);
//                     break;
//                 }
//                 float target = target_positions_[current_target_index_];
//                 can_->setPositionSpeed(motor_id_, target, speed_, accel_);
//                 bool is_reached = std::abs(data.position - target) < POSITION_TOLERANCE;
//                 if (is_reached) {
//                     reach_counter_++;
//                     if (reach_counter_ >= reach_count_max_) {
//                         reach_counter_ = 0;
//                         target_reached_ = true;
//                         current_target_index_++;
//                         if (current_target_index_ >= target_positions_.size()) {
//                             current_target_index_ = target_positions_.size() - 1;
//                             RCLCPP_INFO(logger_, "Motor 0x%02X: All targets reached.", motor_id_);
//                         }
//                     }
//                 } else {
//                     reach_counter_ = 0;
//                     target_reached_ = false;
//                 }
//                 break;
//             }
//             case State::STOPPED:
//                 stop();
//                 break;
//             case State::ERROR:
//                 break;
//         }
//     }

//     void setParameters(const std::vector<rcl_interfaces::msg::Parameter>& parameters) override {
//         RCLCPP_INFO(logger_, "Motor 0x%02X: Received set_parameters request with %zu parameters", 
//                     motor_id_, parameters.size());
//         if (!is_origin_set_) {
//             RCLCPP_WARN(logger_, "Motor 0x%02X: Cannot update parameters until origin is set!", motor_id_);
//             return;
//         }
//         if (!target_reached_) {
//             RCLCPP_WARN(logger_, "Motor 0x%02X: Cannot update target_positions until current target is reached!", 
//                         motor_id_);
//             return;
//         }

//         for (const auto& param : parameters) {
//             RCLCPP_INFO(logger_, "Motor 0x%02X: Processing parameter: %s", motor_id_, param.name.c_str());
//             if (param.name == "target_positions") {
//                 auto temp_positions = param.value.double_array_value;
//                 if (temp_positions.empty()) {
//                     RCLCPP_ERROR(logger_, "Motor 0x%02X: Empty target_positions received!", motor_id_);
//                     return;
//                 }
//                 validatePositions(temp_positions);
//                 target_positions_.resize(temp_positions.size());
//                 std::transform(temp_positions.begin(), temp_positions.end(), 
//                                target_positions_.begin(),
//                                [](double x) { return static_cast<float>(x); });
//                 target_reached_ = false;
//                 current_target_index_ = 0;
//                 RCLCPP_INFO(logger_, "Motor 0x%02X: Updated target_positions: %ld targets", 
//                             motor_id_, temp_positions.size());
//             } else if (param.name == "speed") {
//                 speed_ = param.value.integer_value;
//                 validateSpeed();
//                 RCLCPP_INFO(logger_, "Motor 0x%02X: Updated speed: %d", motor_id_, speed_);
//             } else if (param.name == "accel") {
//                 accel_ = param.value.integer_value;
//                 validateAccel();
//                 RCLCPP_INFO(logger_, "Motor 0x%02X: Updated accel: %d", motor_id_, accel_);
//             } else {
//                 RCLCPP_WARN(logger_, "Motor 0x%02X: Unknown parameter: %s", motor_id_, param.name.c_str());
//             }
//         }
//     }

//     bool isOriginSet() const override { return is_origin_set_; }
//     bool isTargetReached() const override { return target_reached_; }
//     uint8_t motorId() const override { return motor_id_; }

//     void stop() override {
//         try {
//             can_->setCurrent(motor_id_, 0.0);
//             can_->setPositionSpeed(motor_id_, 0.0, 0, 0);
//             transitionTo(State::STOPPED);
//         } catch (const std::exception& e) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: Stop failed: %s", motor_id_, e.what());
//             transitionTo(State::ERROR);
//         }
//     }

// private:
//     enum class State { INITIALIZING, HOMING, MOVING, STOPPED, ERROR };

//     void initializeParameters() {
//         // Thêm tiền tố để tránh trùng lặp
//         node_->declare_parameter(param_prefix_ + "target_positions", std::vector<double>{0.0});
//         node_->declare_parameter(param_prefix_ + "speed", 10000);
//         node_->declare_parameter(param_prefix_ + "accel", 1000);
//         node_->declare_parameter(param_prefix_ + "min_angle", 0.0);
//         node_->declare_parameter(param_prefix_ + "max_angle", 90.0);
//         node_->declare_parameter(param_prefix_ + "reach_count_max", 100);

//         std::vector<double> temp_positions;
//         node_->get_parameter(param_prefix_ + "target_positions", temp_positions);
//         node_->get_parameter(param_prefix_ + "speed", speed_);
//         node_->get_parameter(param_prefix_ + "accel", accel_);
//         node_->get_parameter(param_prefix_ + "min_angle", min_angle_);
//         node_->get_parameter(param_prefix_ + "max_angle", max_angle_);
//         node_->get_parameter(param_prefix_ + "reach_count_max", reach_count_max_);

//         if (temp_positions.empty()) {
//             RCLCPP_WARN(logger_, "Motor 0x%02X: No target positions provided. Using HOME (0).", motor_id_);
//             temp_positions = {0.0};
//         }

//         validatePositions(temp_positions);
//         target_positions_.resize(temp_positions.size());
//         std::transform(temp_positions.begin(), temp_positions.end(), target_positions_.begin(),
//                        [](double x) { return static_cast<float>(x); });

//         validateSpeed();
//         validateAccel();
//     }

//     void validatePositions(const std::vector<double>& positions) {
//         for (double pos : positions) {
//             if (pos < min_angle_ || pos > max_angle_) {
//                 RCLCPP_ERROR(logger_, "Motor 0x%02X: Target position %.2f out of bounds [%.2f, %.2f]", 
//                              motor_id_, pos, min_angle_, max_angle_);
//                 throw std::runtime_error("Target position out of bounds");
//             }
//         }
//     }

//     void validateSpeed() {
//         if (speed_ <= 0 || speed_ > 50000) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: Speed %d out of bounds [1, 50000]", motor_id_, speed_);
//             throw std::runtime_error("Speed out of bounds");
//         }
//     }

//     void validateAccel() {
//         if (accel_ <= 0 || accel_ > 10000) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: Accel %d out of bounds [1, 10000]", motor_id_, accel_);
//             throw std::runtime_error("Accel out of bounds");
//         }
//     }

//     void setOrigin() {
//         try {
//             RCLCPP_INFO(logger_, "Motor 0x%02X: Setting motor origin...", motor_id_);
//             if (!can_->setOrigin(motor_id_, 0)) {
//                 throw std::runtime_error("SetOrigin failed");
//             }
//             RCLCPP_INFO(logger_, "Motor 0x%02X: SetOrigin completed.", motor_id_);
//         } catch (const std::exception& e) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: SetOrigin failed: %s", motor_id_, e.what());
//             stop();
//             throw std::runtime_error("Failed to set motor origin");
//         }
//     }

//     void transitionTo(State new_state) {
//         if (state_ != new_state) {
//             RCLCPP_INFO(logger_, "Motor 0x%02X: Transitioning from %d to %d", 
//                         motor_id_, static_cast<int>(state_), static_cast<int>(new_state));
//             state_ = new_state;
//         }
//     }

// public:
//     static constexpr float VELOCITY_CONVERSION_FACTOR = 2 * 180.0 / (64 * 21 * 60.0); // rpm
// private:
//     static constexpr float POSITION_TOLERANCE = 0.2; // degrees
//     static constexpr float VELOCITY_TOLERANCE = 0.1; // rpm
//     static constexpr int MAX_MOTOR_TEMPERATURE = 80; // °C

//     uint8_t motor_id_;
//     std::shared_ptr<ICANInterface> can_;
//     rclcpp::Node* node_;
//     rclcpp::Logger logger_;
//     State state_;
//     bool is_origin_set_;
//     bool is_home_;
//     std::vector<float> target_positions_;
//     int current_target_index_;
//     int reach_counter_;
//     int reach_count_max_;
//     int speed_;  // pulses per second
//     int accel_;  // pulses per second^2
//     bool target_reached_;
//     float min_angle_;
//     float max_angle_;
//     SafetyChecker safety_checker_;
//     StatePublisher state_publisher_;
//     std::string param_prefix_; // Tiền tố cho tham số
// };

// // Lớp chính điều phối
// class MotorControlNode : public rclcpp::Node {
// public:
//     MotorControlNode() : Node("motor_control_node") {
//         can_ = std::make_shared<CANInterfaceWrapper>("can0");
//         timer_ = create_wall_timer(std::chrono::milliseconds(2), 
//                                   std::bind(&MotorControlNode::controlLoop, this));

//         // Khởi tạo hai động cơ
//         controllers_.push_back(std::make_unique<MotorController>(
//             0x68, can_, this, "/motor1_control_node"));
//         controllers_.push_back(std::make_unique<MotorController>(
//             0x69, can_, this, "/motor2_control_node"));

//         // Tạo service cho mỗi động cơ
//         services_.push_back(create_service<rcl_interfaces::srv::SetParameters>(
//             "/motor1_control_node/set_parameters",
//             std::bind(&MotorControlNode::setParametersCallback, this, std::placeholders::_1,
//                       std::placeholders::_2, 0)));
//         services_.push_back(create_service<rcl_interfaces::srv::SetParameters>(
//             "/motor2_control_node/set_parameters",
//             std::bind(&MotorControlNode::setParametersCallback, this, std::placeholders::_1,
//                       std::placeholders::_2, 1)));

//         rclcpp::on_shutdown(std::bind(&MotorControlNode::safeShutdown, this));
//     }

// private:
//     void controlLoop() {
//         uint32_t id;
//         std::vector<uint8_t> raw_data;
//         try {
//             if (can_->receive(id, raw_data)) {
//                 for (const auto& controller : controllers_) {
//                     if (id == controller->motorId()) {
//                         MotorData data;
//                         can_->decodeMotorData(raw_data, data.position, data.velocity, data.current,
//                                               data.temperature, data.error);
//                         data.velocity *= MotorController::VELOCITY_CONVERSION_FACTOR;
//                         controller->control(data);
//                     }
//                 }
//             }
//         } catch (const std::exception& e) {
//             RCLCPP_ERROR(get_logger(), "CAN error: %s", e.what());
//             for (const auto& controller : controllers_) {
//                 controller->stop();
//             }
//         }
//     }

//     void setParametersCallback(
//         const std::shared_ptr<rcl_interfaces::srv::SetParameters::Request> request,
//         std::shared_ptr<rcl_interfaces::srv::SetParameters::Response> response,
//         size_t controller_index) {
//         RCLCPP_INFO(get_logger(), "Received set_parameters request for controller index: %zu", controller_index);
//         if (controller_index >= controllers_.size()) {
//             RCLCPP_ERROR(get_logger(), "Invalid controller index: %zu", controller_index);
//             response->results.resize(request->parameters.size());
//             for (size_t i = 0; i < request->parameters.size(); ++i) {
//                 response->results[i].successful = false;
//                 response->results[i].reason = "Invalid controller index";
//             }
//             return;
//         }
//         controllers_[controller_index]->setParameters(request->parameters);
//         response->results.resize(request->parameters.size());
//         for (size_t i = 0; i < request->parameters.size(); ++i) {
//             response->results[i].successful = true;
//             response->results[i].reason = "";
//         }
//     }

//     void safeShutdown() {
//         RCLCPP_WARN(get_logger(), "ROS shutdown detected! Stopping motors.");
//         for (const auto& controller : controllers_) {
//             controller->stop();
//         }
//     }

//     std::shared_ptr<ICANInterface> can_;
//     rclcpp::TimerBase::SharedPtr timer_;
//     std::vector<std::unique_ptr<IMotorController>> controllers_;
//     std::vector<rclcpp::Service<rcl_interfaces::srv::SetParameters>::SharedPtr> services_;
// };

// int main(int argc, char** argv) {
//     rclcpp::init(argc, argv);
//     try {
//         rclcpp::spin(std::make_shared<MotorControlNode>());
//     } catch (const std::exception& e) {
//         RCLCPP_ERROR(rclcpp::get_logger("motor_control_node"), "Fatal error: %s", e.what());
//     }
//     rclcpp::shutdown();
//     return 0;
// }
















// // // ĐANG CHAY ĐƯỢC 26/4
// #include "rclcpp/rclcpp.hpp"
// #include "rcl_interfaces/srv/set_parameters.hpp"
// #include "std_msgs/msg/float32.hpp"
// #include "std_msgs/msg/bool.hpp"
// #include "std_msgs/msg/string.hpp" // Thêm cho trạng thái
// #include <vector>
// #include <chrono>
// #include <cmath>
// #include <memory>
// #include <functional>
// #include <stdexcept>
// #include "can_interface/caninterface.hpp"

// // Giao diện trừu tượng cho CAN
// class ICANInterface {
// public:
//     virtual bool setOrigin(uint8_t id, uint8_t mode) = 0;
//     virtual bool setPositionSpeed(uint8_t id, float pos, int speed, int accel) = 0;
//     virtual bool setCurrent(uint8_t id, float current) = 0;
//     virtual bool receive(uint32_t& id, std::vector<uint8_t>& data) = 0;
//     virtual void decodeMotorData(const std::vector<uint8_t>& data, float& pos, float& vel, 
//                                 float& cur, int8_t& temp, int8_t& err) = 0;
//     virtual ~ICANInterface() = default;
// };

// // Triển khai CANInterface kế thừa ICANInterface
// class CANInterfaceWrapper : public ICANInterface {
// public:
//     CANInterfaceWrapper(const std::string& interface_name) : can_(interface_name) {}

//     bool setOrigin(uint8_t id, uint8_t mode) override {
//         try {
//             can_.setOrigin(id, mode);
//             return true;
//         } catch (const std::exception& e) {
//             std::cerr << "SetOrigin failed: " << e.what() << std::endl;
//             return false;
//         }
//     }

//     bool setPositionSpeed(uint8_t id, float pos, int speed, int accel) override {
//         try {
//             can_.setPositionSpeed(id, pos, speed, accel);
//             return true;
//         } catch (const std::exception& e) {
//             std::cerr << "SetPositionSpeed failed: " << e.what() << std::endl;
//             return false;
//         }
//     }

//     bool setCurrent(uint8_t id, float current) override {
//         try {
//             can_.setCurrent(id, current);
//             return true;
//         } catch (const std::exception& e) {
//             std::cerr << "SetCurrent failed: " << e.what() << std::endl;
//             return false;
//         }
//     }

//     bool receive(uint32_t& id, std::vector<uint8_t>& data) override {
//         return can_.receive(id, data);
//     }

//     void decodeMotorData(const std::vector<uint8_t>& data, float& pos, float& vel, 
//                          float& cur, int8_t& temp, int8_t& err) override {
//         can_.decodeMotorData(data, pos, vel, cur, temp, err);
//     }

// private:
//     CANInterface can_;
// };

// // Cấu trúc dữ liệu động cơ
// struct MotorData {
//     float position = 0.0;  // degrees
//     float velocity = 0.0;  // rpm
//     float current = 0.0;   // mA
//     int8_t temperature = 0; // °C
//     int8_t error = 0;      // error code
// };

// // Giao diện điều khiển động cơ
// class IMotorController {
// public:
//     virtual void control(const MotorData& data) = 0;
//     virtual void setParameters(const std::vector<rcl_interfaces::msg::Parameter>& parameters) = 0;
//     virtual bool isOriginSet() const = 0;
//     virtual bool isTargetReached() const = 0;
//     virtual void stop() = 0;
//     virtual uint8_t motorId() const = 0;
//     virtual ~IMotorController() = default;
// };

// // Lớp kiểm tra an toàn
// class SafetyChecker {
// public:
//     SafetyChecker(float min_angle, float max_angle, int max_temperature)
//         : min_angle_(min_angle), max_angle_(max_angle), max_temperature_(max_temperature) {}

//     bool check(const MotorData& data, const rclcpp::Logger& logger) const {
//         if (data.temperature > max_temperature_ || data.error != 0) {
//             RCLCPP_ERROR(logger, "Motor issue! Temp: %d°C, Error: %d", data.temperature, data.error);
//             return false;
//         }
//         if (data.position < min_angle_ || data.position > max_angle_) {
//             RCLCPP_ERROR(logger, "Position %.2f out of bounds [%.2f, %.2f]", 
//                          data.position, min_angle_, max_angle_);
//             return false;
//         }
//         return true;
//     }

// private:
//     float min_angle_;
//     float max_angle_;
//     int max_temperature_;
// };

// // Lớp xuất bản trạng thái
// class StatePublisher {
// public:
//     StatePublisher(rclcpp::Node* node, const std::string& name)
//         : position_publisher_(node->create_publisher<std_msgs::msg::Float32>(name + "/_position", 10)),
//           velocity_publisher_(node->create_publisher<std_msgs::msg::Float32>(name + "/_vel_actual", 10)),
//           reached_publisher_(node->create_publisher<std_msgs::msg::Bool>(name + "/_target_reached", 10)),
//           state_publisher_(node->create_publisher<std_msgs::msg::String>(name + "/_state", 10)) {}

//     void publish(const MotorData& data, bool target_reached, const std::string& state) {
//         std_msgs::msg::Float32 pos_msg;
//         pos_msg.data = data.position;
//         position_publisher_->publish(pos_msg);

//         std_msgs::msg::Float32 vel_msg;
//         vel_msg.data = data.velocity;
//         velocity_publisher_->publish(vel_msg);

//         std_msgs::msg::Bool reached_msg;
//         reached_msg.data = target_reached;
//         reached_publisher_->publish(reached_msg);

//         std_msgs::msg::String state_msg;
//         state_msg.data = state;
//         state_publisher_->publish(state_msg);
//     }

// private:
//     rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr position_publisher_;
//     rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr velocity_publisher_;
//     rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr reached_publisher_;
//     rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_publisher_; // Publisher cho trạng thái
// };

// // Lớp điều khiển động cơ
// class MotorController : public IMotorController {
// public:
//     MotorController(uint8_t motor_id, std::shared_ptr<ICANInterface> can, 
//                     rclcpp::Node* node, const std::string& name)
//         : motor_id_(motor_id), can_(can), node_(node), logger_(node->get_logger()),
//           state_(State::INITIALIZING), is_origin_set_(false), is_home_(false),
//           current_target_index_(0), reach_counter_(0), target_reached_(true),
//           speed_(10000), accel_(1000), min_angle_(0.0), max_angle_(90.0),
//           reach_count_max_(100), safety_checker_(min_angle_, max_angle_, MAX_MOTOR_TEMPERATURE),
//           state_publisher_(node, name), param_prefix_(name + ".") {
//         initializeParameters();
//         setOrigin();
//     }

//     void control(const MotorData& data) override {
//         if (!safety_checker_.check(data, logger_)) {
//             stop();
//             return;
//         }

//         // Chuyển đổi trạng thái thành chuỗi
//         std::string state_str;
//         switch (state_) {
//             case State::INITIALIZING: state_str = "INITIALIZING"; break;
//             case State::HOMING: state_str = "HOMING"; break;
//             case State::MOVING: state_str = "MOVING"; break;
//             case State::STOPPED: state_str = "STOPPED"; break;
//             case State::ERROR: state_str = "ERROR"; break;
//         }

//         state_publisher_.publish(data, target_reached_, state_str);

//         switch (state_) {
//             case State::INITIALIZING:
//                 if (std::abs(data.position) < POSITION_TOLERANCE && 
//                     std::abs(data.velocity) < VELOCITY_TOLERANCE) {
//                     is_origin_set_ = true;
//                     transitionTo(State::HOMING);
//                 }
//                 break;
//             case State::HOMING:
//                 can_->setPositionSpeed(motor_id_, 0.0, speed_, accel_);
//                 if (std::abs(data.position) < POSITION_TOLERANCE && 
//                     std::abs(data.velocity) < VELOCITY_TOLERANCE) {
//                     is_home_ = true;
//                     target_reached_ = true;
//                     RCLCPP_INFO(logger_, "Motor 0x%02X reached Home Position!", motor_id_);
//                     transitionTo(State::MOVING);
//                 }
//                 break;
//             case State::MOVING: {
//                 if (target_positions_.empty()) {
//                     RCLCPP_ERROR(logger_, "Motor 0x%02X: target_positions is empty!", motor_id_);
//                     stop();
//                     transitionTo(State::STOPPED);
//                     break;
//                 }
//                 float target = target_positions_[current_target_index_];
//                 can_->setPositionSpeed(motor_id_, target, speed_, accel_);
//                 bool is_reached = std::abs(data.position - target) < POSITION_TOLERANCE;
//                 if (is_reached) {
//                     reach_counter_++;
//                     if (reach_counter_ >= reach_count_max_) {
//                         reach_counter_ = 0;
//                         target_reached_ = true;
//                         current_target_index_++;
//                         if (current_target_index_ >= target_positions_.size()) {
//                             current_target_index_ = target_positions_.size() - 1;
//                             RCLCPP_INFO(logger_, "Motor 0x%02X: All targets reached.", motor_id_);
//                         }
//                     }
//                 } else {
//                     reach_counter_ = 0;
//                     target_reached_ = false;
//                 }
//                 break;
//             }
//             case State::STOPPED:
//                 stop();
//                 break;
//             case State::ERROR:
//                 break;
//         }
//     }

//     void setParameters(const std::vector<rcl_interfaces::msg::Parameter>& parameters) override {
//         RCLCPP_INFO(logger_, "Motor 0x%02X: Received set_parameters request with %zu parameters", 
//                     motor_id_, parameters.size());
//         if (!is_origin_set_) {
//             RCLCPP_WARN(logger_, "Motor 0x%02X: Cannot update parameters until origin is set!", motor_id_);
//             return;
//         }
//         if (!target_reached_) {
//             RCLCPP_WARN(logger_, "Motor 0x%02X: Cannot update target_positions until current target is reached!", 
//                         motor_id_);
//             return;
//         }

//         for (const auto& param : parameters) {
//             RCLCPP_INFO(logger_, "Motor 0x%02X: Processing parameter: %s", motor_id_, param.name.c_str());
//             if (param.name == "target_positions") {
//                 auto temp_positions = param.value.double_array_value;
//                 if (temp_positions.empty()) {
//                     RCLCPP_ERROR(logger_, "Motor 0x%02X: Empty target_positions received!", motor_id_);
//                     return;
//                 }
//                 validatePositions(temp_positions);
//                 target_positions_.resize(temp_positions.size());
//                 std::transform(temp_positions.begin(), temp_positions.end(), 
//                                target_positions_.begin(),
//                                [](double x) { return static_cast<float>(x); });
//                 target_reached_ = false;
//                 current_target_index_ = 0;
//                 RCLCPP_INFO(logger_, "Motor 0x%02X: Updated target_positions: %ld targets", 
//                             motor_id_, temp_positions.size());
//             } else if (param.name == "speed") {
//                 speed_ = param.value.integer_value;
//                 validateSpeed();
//                 RCLCPP_INFO(logger_, "Motor 0x%02X: Updated speed: %d", motor_id_, speed_);
//             } else if (param.name == "accel") {
//                 accel_ = param.value.integer_value;
//                 validateAccel();
//                 RCLCPP_INFO(logger_, "Motor 0x%02X: Updated accel: %d", motor_id_, accel_);
//             } else {
//                 RCLCPP_WARN(logger_, "Motor 0x%02X: Unknown parameter: %s", motor_id_, param.name.c_str());
//             }
//         }
//     }

//     bool isOriginSet() const override { return is_origin_set_; }
//     bool isTargetReached() const override { return target_reached_; }
//     uint8_t motorId() const override { return motor_id_; }

//     void stop() override {
//         try {
//             can_->setCurrent(motor_id_, 0.0);
//             can_->setPositionSpeed(motor_id_, 0.0, 0, 0);
//             transitionTo(State::STOPPED);
//         } catch (const std::exception& e) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: Stop failed: %s", motor_id_, e.what());
//             transitionTo(State::ERROR);
//         }
//     }

// private:
//     enum class State { INITIALIZING, HOMING, MOVING, STOPPED, ERROR };

//     void initializeParameters() {
//         node_->declare_parameter(param_prefix_ + "target_positions", std::vector<double>{0.0});
//         node_->declare_parameter(param_prefix_ + "speed", 10000);
//         node_->declare_parameter(param_prefix_ + "accel", 1000);
//         node_->declare_parameter(param_prefix_ + "min_angle", 0.0);
//         node_->declare_parameter(param_prefix_ + "max_angle", 90.0);
//         node_->declare_parameter(param_prefix_ + "reach_count_max", 100);

//         std::vector<double> temp_positions;
//         node_->get_parameter(param_prefix_ + "target_positions", temp_positions);
//         node_->get_parameter(param_prefix_ + "speed", speed_);
//         node_->get_parameter(param_prefix_ + "accel", accel_);
//         node_->get_parameter(param_prefix_ + "min_angle", min_angle_);
//         node_->get_parameter(param_prefix_ + "max_angle", max_angle_);
//         node_->get_parameter(param_prefix_ + "reach_count_max", reach_count_max_);

//         if (temp_positions.empty()) {
//             RCLCPP_WARN(logger_, "Motor 0x%02X: No target positions provided. Using HOME (0).", motor_id_);
//             temp_positions = {0.0};
//         }

//         validatePositions(temp_positions);
//         target_positions_.resize(temp_positions.size());
//         std::transform(temp_positions.begin(), temp_positions.end(), target_positions_.begin(),
//                        [](double x) { return static_cast<float>(x); });

//         validateSpeed();
//         validateAccel();
//     }

//     void validatePositions(const std::vector<double>& positions) {
//         for (double pos : positions) {
//             if (pos < min_angle_ || pos > max_angle_) {
//                 RCLCPP_ERROR(logger_, "Motor 0x%02X: Target position %.2f out of bounds [%.2f, %.2f]", 
//                              motor_id_, pos, min_angle_, max_angle_);
//                 throw std::runtime_error("Target position out of bounds");
//             }
//         }
//     }

//     void validateSpeed() {
//         if (speed_ <= 0 || speed_ > 50000) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: Speed %d out of bounds [1, 50000]", motor_id_, speed_);
//             throw std::runtime_error("Speed out of bounds");
//         }
//     }

//     void validateAccel() {
//         if (accel_ <= 0 || accel_ > 10000) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: Accel %d out of bounds [1, 10000]", motor_id_, accel_);
//             throw std::runtime_error("Accel out of bounds");
//         }
//     }

//     void setOrigin() {
//         try {
//             RCLCPP_INFO(logger_, "Motor 0x%02X: Setting motor origin...", motor_id_);
//             if (!can_->setOrigin(motor_id_, 0)) {
//                 throw std::runtime_error("SetOrigin failed");
//             }
//             RCLCPP_INFO(logger_, "Motor 0x%02X: SetOrigin completed.", motor_id_);
//         } catch (const std::exception& e) {
//             RCLCPP_ERROR(logger_, "Motor 0x%02X: SetOrigin failed: %s", motor_id_, e.what());
//             stop();
//             throw std::runtime_error("Failed to set motor origin");
//         }
//     }

//     void transitionTo(State new_state) {
//         if (state_ != new_state) {
//             RCLCPP_INFO(logger_, "Motor 0x%02X: Transitioning from %d to %d", 
//                         motor_id_, static_cast<int>(state_), static_cast<int>(new_state));
//             state_ = new_state;
//         }
//     }

// public:
//     static constexpr float VELOCITY_CONVERSION_FACTOR = 2 * 180.0 / (64 * 21 * 60.0); // rpm
// private:
//     static constexpr float POSITION_TOLERANCE = 0.2; // degrees
//     static constexpr float VELOCITY_TOLERANCE = 0.1; // rpm
//     static constexpr int MAX_MOTOR_TEMPERATURE = 80; // °C

//     uint8_t motor_id_;
//     std::shared_ptr<ICANInterface> can_;
//     rclcpp::Node* node_;
//     rclcpp::Logger logger_;
//     State state_;
//     bool is_origin_set_;
//     bool is_home_;
//     std::vector<float> target_positions_;
//     int current_target_index_;
//     int reach_counter_;
//     int reach_count_max_;
//     int speed_;  // pulses per second
//     int accel_;  // pulses per second^2
//     bool target_reached_;
//     float min_angle_;
//     float max_angle_;
//     SafetyChecker safety_checker_;
//     StatePublisher state_publisher_;
//     std::string param_prefix_;
// };

// // Lớp chính điều phối
// class MotorControlNode : public rclcpp::Node {
// public:
//     MotorControlNode() : Node("motor_control_node") {
//         can_ = std::make_shared<CANInterfaceWrapper>("can0");
//         timer_ = create_wall_timer(std::chrono::milliseconds(2), 
//                                   std::bind(&MotorControlNode::controlLoop, this));

//         controllers_.push_back(std::make_unique<MotorController>(
//             0x68, can_, this, "/motor1_control_node"));
//         controllers_.push_back(std::make_unique<MotorController>(
//             0x69, can_, this, "/motor2_control_node"));

//         services_.push_back(create_service<rcl_interfaces::srv::SetParameters>(
//             "/motor1_control_node/set_parameters",
//             std::bind(&MotorControlNode::setParametersCallback, this, std::placeholders::_1,
//                       std::placeholders::_2, 0)));
//         services_.push_back(create_service<rcl_interfaces::srv::SetParameters>(
//             "/motor2_control_node/set_parameters",
//             std::bind(&MotorControlNode::setParametersCallback, this, std::placeholders::_1,
//                       std::placeholders::_2, 1)));

//         rclcpp::on_shutdown(std::bind(&MotorControlNode::safeShutdown, this));
//     }

// private:
//     void controlLoop() {
//         uint32_t id;
//         std::vector<uint8_t> raw_data;
//         try {
//             if (can_->receive(id, raw_data)) {
//                 for (const auto& controller : controllers_) {
//                     if (id == controller->motorId()) {
//                         MotorData data;
//                         can_->decodeMotorData(raw_data, data.position, data.velocity, data.current,
//                                               data.temperature, data.error);
//                         data.velocity *= MotorController::VELOCITY_CONVERSION_FACTOR;
//                         controller->control(data);
//                     }
//                 }
//             }
//         } catch (const std::exception& e) {
//             RCLCPP_ERROR(get_logger(), "CAN error: %s", e.what());
//             for (const auto& controller : controllers_) {
//                 controller->stop();
//             }
//         }
//     }

//     void setParametersCallback(
//         const std::shared_ptr<rcl_interfaces::srv::SetParameters::Request> request,
//         std::shared_ptr<rcl_interfaces::srv::SetParameters::Response> response,
//         size_t controller_index) {
//         RCLCPP_INFO(get_logger(), "Received set_parameters request for controller index: %zu", controller_index);
//         if (controller_index >= controllers_.size()) {
//             RCLCPP_ERROR(get_logger(), "Invalid controller index: %zu", controller_index);
//             response->results.resize(request->parameters.size());
//             for (size_t i = 0; i < request->parameters.size(); ++i) {
//                 response->results[i].successful = false;
//                 response->results[i].reason = "Invalid controller index";
//             }
//             return;
//         }
//         controllers_[controller_index]->setParameters(request->parameters);
//         response->results.resize(request->parameters.size());
//         for (size_t i = 0; i < request->parameters.size(); ++i) {
//             response->results[i].successful = true;
//             response->results[i].reason = "";
//         }
//     }

//     void safeShutdown() {
//         RCLCPP_WARN(get_logger(), "ROS shutdown detected! Stopping motors.");
//         for (const auto& controller : controllers_) {
//             controller->stop();
//         }
//     }

//     std::shared_ptr<ICANInterface> can_;
//     rclcpp::TimerBase::SharedPtr timer_;
//     std::vector<std::unique_ptr<IMotorController>> controllers_;
//     std::vector<rclcpp::Service<rcl_interfaces::srv::SetParameters>::SharedPtr> services_;
// };

// int main(int argc, char** argv) {
//     rclcpp::init(argc, argv);
//     try {
//         rclcpp::spin(std::make_shared<MotorControlNode>());
//     } catch (const std::exception& e) {
//         RCLCPP_ERROR(rclcpp::get_logger("motor_control_node"), "Fatal error: %s", e.what());
//     }
//     rclcpp::shutdown();
//     return 0;
// }





// CẬP NHÂT CHẾ ĐÔ VỀ HOME KHI CTRL C, KILL NODE VV
#include "rclcpp/rclcpp.hpp"
#include "rcl_interfaces/srv/set_parameters.hpp"
#include "std_msgs/msg/float32.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"
#include <vector>
#include <chrono>
#include <cmath>
#include <memory>
#include <functional>
#include <stdexcept>
#include <csignal>
#include <thread>
#include "can_interface/caninterface.hpp"

// Giao diện trừu tượng cho CAN
class ICANInterface {
public:
    virtual bool setOrigin(uint8_t id, uint8_t mode) = 0;
    virtual bool setPositionSpeed(uint8_t id, float pos, int speed, int accel) = 0;
    virtual bool setCurrent(uint8_t id, float current) = 0;
    virtual bool receive(uint32_t& id, std::vector<uint8_t>& data) = 0;
    virtual void decodeMotorData(const std::vector<uint8_t>& data, float& pos, float& vel, 
                                float& cur, int8_t& temp, int8_t& err) = 0;
    virtual ~ICANInterface() = default;
};

// Triển khai CANInterface kế thừa ICANInterface
class CANInterfaceWrapper : public ICANInterface {
public:
    explicit CANInterfaceWrapper(const std::string& interface_name) : can_(interface_name) {}

    bool setOrigin(uint8_t id, uint8_t mode) override {
        try {
            can_.setOrigin(id, mode);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "SetOrigin failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool setPositionSpeed(uint8_t id, float pos, int speed, int accel) override {
        try {
            can_.setPositionSpeed(id, pos, speed, accel);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "SetPositionSpeed failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool setCurrent(uint8_t id, float current) override {
        try {
            can_.setCurrent(id, current);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "SetCurrent failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool receive(uint32_t& id, std::vector<uint8_t>& data) override {
        return can_.receive(id, data);
    }

    void decodeMotorData(const std::vector<uint8_t>& data, float& pos, float& vel, 
                         float& cur, int8_t& temp, int8_t& err) override {
        can_.decodeMotorData(data, pos, vel, cur, temp, err);
    }

private:
    CANInterface can_;
};

// Cấu trúc dữ liệu động cơ
struct MotorData {
    float position = 0.0;  // degrees
    float velocity = 0.0;  // rpm
    float current = 0.0;   // mA
    int8_t temperature = 0; // °C
    int8_t error = 0;      // error code
};

// Giao diện điều khiển động cơ
class IMotorController {
public:
    virtual void control(const MotorData& data) = 0;
    virtual void setParameters(const std::vector<rcl_interfaces::msg::Parameter>& parameters) = 0;
    virtual bool isOriginSet() const = 0;
    virtual bool isTargetReached() const = 0;
    virtual void stop() = 0;
    virtual uint8_t motorId() const = 0;
    virtual ~IMotorController() = default;
};

// Lớp kiểm tra an toàn
class SafetyChecker {
public:
    SafetyChecker(float min_angle, float max_angle, int max_temperature)
        : min_angle_(min_angle), max_angle_(max_angle), max_temperature_(max_temperature) {}

    bool check(const MotorData& data, const rclcpp::Logger& logger) const {
        if (data.temperature > max_temperature_ || data.error != 0) {
            RCLCPP_ERROR(logger, "Motor issue! Temp: %d°C, Error: %d", data.temperature, data.error);
            return false;
        }
        if (data.position < min_angle_ || data.position > max_angle_) {
            RCLCPP_ERROR(logger, "Position %.2f out of bounds [%.2f, %.2f]", 
                         data.position, min_angle_, max_angle_);
            return false;
        }
        return true;
    }

private:
    float min_angle_;
    float max_angle_;
    int max_temperature_;
};

// Lớp xuất bản trạng thái
class StatePublisher {
public:
    StatePublisher(rclcpp::Node* node, const std::string& name)
        : position_publisher_(node->create_publisher<std_msgs::msg::Float32>(name + "/_position", 10)),
          velocity_publisher_(node->create_publisher<std_msgs::msg::Float32>(name + "/_vel_actual", 10)),
          reached_publisher_(node->create_publisher<std_msgs::msg::Bool>(name + "/_target_reached", 10)),
          state_publisher_(node->create_publisher<std_msgs::msg::String>(name + "/_state", 10)) {}

    void publish(const MotorData& data, bool target_reached, const std::string& state) {
        std_msgs::msg::Float32 pos_msg;
        pos_msg.data = data.position;
        position_publisher_->publish(pos_msg);

        std_msgs::msg::Float32 vel_msg;
        vel_msg.data = data.velocity;
        velocity_publisher_->publish(vel_msg);

        std_msgs::msg::Bool reached_msg;
        reached_msg.data = target_reached;
        reached_publisher_->publish(reached_msg);

        std_msgs::msg::String state_msg;
        state_msg.data = state;
        state_publisher_->publish(state_msg);
    }

private:
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr position_publisher_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr velocity_publisher_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr reached_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_publisher_;
};

// Lớp điều khiển động cơ
class MotorController : public IMotorController {
public:
    MotorController(uint8_t motor_id, std::shared_ptr<ICANInterface> can, 
                    rclcpp::Node* node, const std::string& name)
        : motor_id_(motor_id), can_(can), node_(node), logger_(node->get_logger()),
          state_(State::INITIALIZING), is_origin_set_(false), is_home_(false),
          current_target_index_(0), reach_counter_(0), target_reached_(true),
          speed_(10000), accel_(1000), min_angle_(0.0), max_angle_(90.0),
          reach_count_max_(100), safety_checker_(min_angle_, max_angle_, MAX_MOTOR_TEMPERATURE),
          state_publisher_(node, name), param_prefix_(name + ".") {
        initializeParameters();
        setOrigin();
    }

    void control(const MotorData& data) override {
        if (!safety_checker_.check(data, logger_)) {
            stop();
            return;
        }

        std::string state_str;
        switch (state_) {
            case State::INITIALIZING: state_str = "INITIALIZING"; break;
            case State::HOMING: state_str = "HOMING"; break;
            case State::MOVING: state_str = "MOVING"; break;
            case State::STOPPED: state_str = "STOPPED"; break;
            case State::ERROR: state_str = "ERROR"; break;
        }

        state_publisher_.publish(data, target_reached_, state_str);

        switch (state_) {
            case State::INITIALIZING:
                if (std::abs(data.position) < POSITION_TOLERANCE && 
                    std::abs(data.velocity) < VELOCITY_TOLERANCE) {
                    is_origin_set_ = true;
                    transitionTo(State::HOMING);
                }
                break;
            case State::HOMING:
                can_->setPositionSpeed(motor_id_, 0.0, speed_, accel_);
                if (std::abs(data.position) < POSITION_TOLERANCE && 
                    std::abs(data.velocity) < VELOCITY_TOLERANCE) {
                    is_home_ = true;
                    target_reached_ = true;
                    RCLCPP_INFO(logger_, "Motor 0x%02X reached Home Position!", motor_id_);
                    transitionTo(State::MOVING);
                }
                break;
            case State::MOVING: {
                if (target_positions_.empty()) {
                    RCLCPP_ERROR(logger_, "Motor 0x%02X: target_positions is empty!", motor_id_);
                    stop();
                    transitionTo(State::STOPPED);
                    break;
                }
                float target = target_positions_[current_target_index_];
                can_->setPositionSpeed(motor_id_, target, speed_, accel_);
                bool is_reached = std::abs(data.position - target) < POSITION_TOLERANCE;
                if (is_reached) {
                    reach_counter_++;
                    if (reach_counter_ >= reach_count_max_) {
                        reach_counter_ = 0;
                        target_reached_ = true;
                        current_target_index_++;
                        if (current_target_index_ >= target_positions_.size()) {
                            current_target_index_ = target_positions_.size() - 1;
                            RCLCPP_INFO(logger_, "Motor 0x%02X: All targets reached.", motor_id_);
                        }
                    }
                } else {
                    reach_counter_ = 0;
                    target_reached_ = false;
                }
                break;
            }
            case State::STOPPED:
                stop();
                break;
            case State::ERROR:
                break;
        }
    }

    void setParameters(const std::vector<rcl_interfaces::msg::Parameter>& parameters) override {
        RCLCPP_INFO(logger_, "Motor 0x%02X: Received set_parameters request with %zu parameters", 
                    motor_id_, parameters.size());
        if (!is_origin_set_) {
            RCLCPP_WARN(logger_, "Motor 0x%02X: Cannot update parameters until origin is set!", motor_id_);
            return;
        }
        if (!target_reached_) {
            RCLCPP_WARN(logger_, "Motor 0x%02X: Cannot update target_positions until current target is reached!", 
                        motor_id_);
            return;
        }

        for (const auto& param : parameters) {
            RCLCPP_INFO(logger_, "Motor 0x%02X: Processing parameter: %s", motor_id_, param.name.c_str());
            if (param.name == "target_positions") {
                auto temp_positions = param.value.double_array_value;
                if (temp_positions.empty()) {
                    RCLCPP_ERROR(logger_, "Motor 0x%02X: Empty target_positions received!", motor_id_);
                    return;
                }
                validatePositions(temp_positions);
                target_positions_.resize(temp_positions.size());
                std::transform(temp_positions.begin(), temp_positions.end(), 
                               target_positions_.begin(),
                               [](double x) { return static_cast<float>(x); });
                target_reached_ = false;
                current_target_index_ = 0;
                RCLCPP_INFO(logger_, "Motor 0x%02X: Updated target_positions: %ld targets", 
                            motor_id_, temp_positions.size());
            } else if (param.name == "speed") {
                speed_ = param.value.integer_value;
                validateSpeed();
                RCLCPP_INFO(logger_, "Motor 0x%02X: Updated speed: %d", motor_id_, speed_);
            } else if (param.name == "accel") {
                accel_ = param.value.integer_value;
                validateAccel();
                RCLCPP_INFO(logger_, "Motor 0x%02X: Updated accel: %d", motor_id_, accel_);
            } else {
                RCLCPP_WARN(logger_, "Motor 0x%02X: Unknown parameter: %s", motor_id_, param.name.c_str());
            }
        }
    }

    bool isOriginSet() const override { return is_origin_set_; }
    bool isTargetReached() const override { return target_reached_; }
    uint8_t motorId() const override { return motor_id_; }

    void stop() override {
        try {
            can_->setCurrent(motor_id_, 0.0);
            can_->setPositionSpeed(motor_id_, 0.0, 0, 0);
            transitionTo(State::STOPPED);
        } catch (const std::exception& e) {
            RCLCPP_ERROR(logger_, "Motor 0x%02X: Stop failed: %s", motor_id_, e.what());
            transitionTo(State::ERROR);
        }
    }

private:
    enum class State { INITIALIZING, HOMING, MOVING, STOPPED, ERROR };

    void initializeParameters() {
        node_->declare_parameter(param_prefix_ + "target_positions", std::vector<double>{0.0});
        node_->declare_parameter(param_prefix_ + "speed", 10000);
        node_->declare_parameter(param_prefix_ + "accel", 1000);
        node_->declare_parameter(param_prefix_ + "min_angle", 0.0);
        node_->declare_parameter(param_prefix_ + "max_angle", 90.0);
        node_->declare_parameter(param_prefix_ + "reach_count_max", 100);

        std::vector<double> temp_positions;
        node_->get_parameter(param_prefix_ + "target_positions", temp_positions);
        node_->get_parameter(param_prefix_ + "speed", speed_);
        node_->get_parameter(param_prefix_ + "accel", accel_);
        node_->get_parameter(param_prefix_ + "min_angle", min_angle_);
        node_->get_parameter(param_prefix_ + "max_angle", max_angle_);
        node_->get_parameter(param_prefix_ + "reach_count_max", reach_count_max_);

        if (temp_positions.empty()) {
            RCLCPP_WARN(logger_, "Motor 0x%02X: No target positions provided. Using HOME (0).", motor_id_);
            temp_positions = {0.0};
        }

        validatePositions(temp_positions);
        target_positions_.resize(temp_positions.size());
        std::transform(temp_positions.begin(), temp_positions.end(), target_positions_.begin(),
                       [](double x) { return static_cast<float>(x); });

        validateSpeed();
        validateAccel();
    }

    void validatePositions(const std::vector<double>& positions) {
        for (double pos : positions) {
            if (pos < min_angle_ || pos > max_angle_) {
                RCLCPP_ERROR(logger_, "Motor 0x%02X: Target position %.2f out of bounds [%.2f, %.2f]", 
                             motor_id_, pos, min_angle_, max_angle_);
                throw std::runtime_error("Target position out of bounds");
            }
        }
    }

    void validateSpeed() {
        if (speed_ <= 0 || speed_ > 50000) {
            RCLCPP_ERROR(logger_, "Motor 0x%02X: Speed %d out of bounds [1, 50000]", motor_id_, speed_);
            throw std::runtime_error("Speed out of bounds");
        }
    }

    void validateAccel() {
        if (accel_ <= 0 || accel_ > 10000) {
            RCLCPP_ERROR(logger_, "Motor 0x%02X: Accel %d out of bounds [1, 10000]", motor_id_, accel_);
            throw std::runtime_error("Accel out of bounds");
        }
    }

    void setOrigin() {
        try {
            RCLCPP_INFO(logger_, "Motor 0x%02X: Setting motor origin...", motor_id_);
            if (!can_->setOrigin(motor_id_, 0)) {
                throw std::runtime_error("SetOrigin failed");
            }
            RCLCPP_INFO(logger_, "Motor 0x%02X: SetOrigin completed.", motor_id_);
        } catch (const std::exception& e) {
            RCLCPP_ERROR(logger_, "Motor 0x%02X: SetOrigin failed: %s", motor_id_, e.what());
            stop();
            throw std::runtime_error("Failed to set motor origin");
        }
    }

    void transitionTo(State new_state) {
        if (state_ != new_state) {
            RCLCPP_INFO(logger_, "Motor 0x%02X: Transitioning from %d to %d", 
                        motor_id_, static_cast<int>(state_), static_cast<int>(new_state));
            state_ = new_state;
        }
    }

public:
    static constexpr float VELOCITY_CONVERSION_FACTOR = 2 * 180.0 / (64 * 21 * 60.0); // rpm
    static constexpr float POSITION_TOLERANCE = 0.5; // SỬA: Chuyển thành public
    static constexpr float VELOCITY_TOLERANCE = 0.1; // rpm
    static constexpr int MAX_MOTOR_TEMPERATURE = 80; // °C

private:
    uint8_t motor_id_;
    std::shared_ptr<ICANInterface> can_;
    rclcpp::Node* node_;
    rclcpp::Logger logger_;
    State state_;
    bool is_origin_set_;
    bool is_home_;
    std::vector<float> target_positions_;
    int current_target_index_;
    int reach_counter_;
    int reach_count_max_;
    int speed_;
    int accel_;
    bool target_reached_;
    float min_angle_;
    float max_angle_;
    SafetyChecker safety_checker_;
    StatePublisher state_publisher_;
    std::string param_prefix_;
};

// Lớp chính điều phối
class MotorControlNode : public rclcpp::Node {
public:
    MotorControlNode() : Node("motor_control_node") {
        can_ = std::make_shared<CANInterfaceWrapper>("can0");
        timer_ = create_wall_timer(std::chrono::milliseconds(2), 
                                  std::bind(&MotorControlNode::controlLoop, this));

        controllers_.push_back(std::make_unique<MotorController>(
            0x68, can_, this, "/motor1_control_node"));
        controllers_.push_back(std::make_unique<MotorController>(
            0x69, can_, this, "/motor2_control_node"));

        services_.push_back(create_service<rcl_interfaces::srv::SetParameters>(
            "/motor1_control_node/set_parameters",
            std::bind(&MotorControlNode::setParametersCallback, this, std::placeholders::_1,
                      std::placeholders::_2, 0)));
        services_.push_back(create_service<rcl_interfaces::srv::SetParameters>(
            "/motor2_control_node/set_parameters",
            std::bind(&MotorControlNode::setParametersCallback, this, std::placeholders::_1,
                      std::placeholders::_2, 1)));

        rclcpp::on_shutdown(std::bind(&MotorControlNode::safeShutdown, this));
    }

    void handleSigint() {
        RCLCPP_WARN(get_logger(), "SIGINT (Ctrl+C) detected! Moving motors to HOME before shutdown.");
        returnToHome();
        safeShutdown();
        rclcpp::shutdown();
    }

private:
    void returnToHome() {
        RCLCPP_INFO(get_logger(), "Attempting to return motors to HOME position...");
        try {
            for (const auto& controller : controllers_) {
                MotorData data = getMotorData(controller->motorId());
                if (safety_checker_.check(data, get_logger())) {
                    if (!can_->setPositionSpeed(controller->motorId(), 0.0, 10000, 1000)) {
                        RCLCPP_ERROR(get_logger(), "Failed to set HOME position for motor 0x%02X", controller->motorId());
                    }
                } else {
                    RCLCPP_ERROR(get_logger(), "Motor 0x%02X unsafe to move to HOME (Temp: %d°C, Error: %d)", 
                                 controller->motorId(), data.temperature, data.error);
                }
            }

            auto start_time = std::chrono::steady_clock::now();
            bool all_at_home = false;
            while (!all_at_home && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(15)) {
                all_at_home = true;
                uint32_t id;
                std::vector<uint8_t> raw_data;
                if (can_->receive(id, raw_data)) {
                    for (const auto& controller : controllers_) {
                        if (id == controller->motorId()) {
                            MotorData data;
                            can_->decodeMotorData(raw_data, data.position, data.velocity, data.current,
                                                  data.temperature, data.error);
                            if (std::abs(data.position) > MotorController::POSITION_TOLERANCE) {
                                all_at_home = false;
                            }
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (all_at_home) {
                RCLCPP_INFO(get_logger(), "All motors reached HOME position.");
            } else {
                RCLCPP_ERROR(get_logger(), "Timeout waiting for motors to reach HOME position.");
            }
        } catch (const std::exception& e) {
            RCLCPP_ERROR(get_logger(), "Error while returning to HOME: %s", e.what());
        }
    }

    MotorData getMotorData(uint8_t motor_id) {
        MotorData data;
        uint32_t id;
        std::vector<uint8_t> raw_data;
        if (can_->receive(id, raw_data) && id == motor_id) {
            can_->decodeMotorData(raw_data, data.position, data.velocity, data.current,
                                  data.temperature, data.error);
        }
        return data;
    }

    void controlLoop() {
        uint32_t id;
        std::vector<uint8_t> raw_data;
        try {
            if (can_->receive(id, raw_data)) {
                for (const auto& controller : controllers_) {
                    if (id == controller->motorId()) {
                        MotorData data;
                        can_->decodeMotorData(raw_data, data.position, data.velocity, data.current,
                                              data.temperature, data.error);
                        data.velocity *= MotorController::VELOCITY_CONVERSION_FACTOR;
                        controller->control(data);
                    }
                }
            }
        } catch (const std::exception& e) {
            RCLCPP_ERROR(get_logger(), "CAN error: %s", e.what());
            returnToHome();
            for (const auto& controller : controllers_) {
                controller->stop();
            }
        }
    }

    void setParametersCallback(
        const std::shared_ptr<rcl_interfaces::srv::SetParameters::Request> request,
        std::shared_ptr<rcl_interfaces::srv::SetParameters::Response> response,
        size_t controller_index) {
        RCLCPP_INFO(get_logger(), "Received set_parameters request for controller index: %zu", controller_index);
        if (controller_index >= controllers_.size()) {
            RCLCPP_ERROR(get_logger(), "Invalid controller index: %zu", controller_index);
            response->results.resize(request->parameters.size());
            for (size_t i = 0; i < request->parameters.size(); ++i) {
                response->results[i].successful = false;
                response->results[i].reason = "Invalid controller index";
            }
            return;
        }
        controllers_[controller_index]->setParameters(request->parameters);
        response->results.resize(request->parameters.size());
        for (size_t i = 0; i < request->parameters.size(); ++i) {
            response->results[i].successful = true;
            response->results[i].reason = "";
        }
    }

    void safeShutdown() {
        RCLCPP_WARN(get_logger(), "ROS shutdown detected! Moving motors to HOME before stopping.");
        returnToHome();
        for (const auto& controller : controllers_) {
            controller->stop();
        }
    }

    std::shared_ptr<ICANInterface> can_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::vector<std::unique_ptr<IMotorController>> controllers_;
    std::vector<rclcpp::Service<rcl_interfaces::srv::SetParameters>::SharedPtr> services_;
    SafetyChecker safety_checker_ = SafetyChecker(0.0, 90.0, 80);
};

static std::shared_ptr<MotorControlNode> global_node;

void signalHandler(int signum) {
    if (global_node) {
        global_node->handleSigint();
    }
}

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    try {
        global_node = std::make_shared<MotorControlNode>();
        std::signal(SIGINT, signalHandler);
        rclcpp::spin(global_node);
    } catch (const std::exception& e) {
        RCLCPP_ERROR(rclcpp::get_logger("motor_control_node"), "Fatal error: %s", e.what());
    }
    global_node.reset();
    rclcpp::shutdown();
    return 0;
}