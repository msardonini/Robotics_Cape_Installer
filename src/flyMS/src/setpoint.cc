/**
 * @file setpoint.cpp
 * @brief Class for controlling the input reference angle to the inner loop controller
 *
 * @author Mike Sardonini
 * @date 10/15/2018
 */

#include "flyMS/setpoint.h"

#include <chrono>
#include <iostream>

#include "spdlog/spdlog.h"
#include "rc/start_stop.h"

Setpoint::Setpoint(const YAML::Node &config_params) :
  is_running_(true),
  ready_to_send_(false) {
    is_debug_mode_ = config_params["debug_mode"].as<bool>();
    flight_mode_ = config_params["flight_mode"].as<int>();
    delta_t_ = config_params["delta_t"].as<float>();

    YAML::Node setpoint_params = config_params["setpoint"];
    max_setpoints_stabilized_ = setpoint_params["max_setpoints_stabilized"].as<
      std::array<float, 3> >();
    max_setpoints_acro_ = setpoint_params["max_setpoints_acro"].as<std::array<float, 3> >();
    is_headless_mode_ = setpoint_params["headless_mode"].as<bool>();
    throttle_limits_ = setpoint_params["throttle_limits"].as<std::array<float, 2> >();

    position_controller = std::make_unique<PositionController> (config_params);
    int ret = rc_dsm_init();
    if (ret < 0) {
      throw std::invalid_argument("DSM failed to initialize");
    }

    // Zero out the container
    memset(&setpoint_data_, 0x00, sizeof(setpoint_data_));

    setpoint_thread_ = std::thread(&Setpoint::SetpointManager, this);
  }

Setpoint::~Setpoint() {
  is_running_.store(false);
  if (setpoint_thread_.joinable()) {
    setpoint_thread_.join();
  }
}


//Gets the data from the local thread. Returns zero if no new data is available
bool Setpoint::GetSetpointData(SetpointData* setpoint) {
  if (ready_to_send_.load()) {
    std::lock_guard<std::mutex> lock(setpoint_mutex_);
    *setpoint = setpoint_data_;
    ready_to_send_.store(false);
    return true;
  }
  return false;
}

/*
  setpoint_manager()
    Handles the setpoint values for roll/pitch/yaw to be fed into the flight controller

    2 Main sources of retreiving values
      1. Direct from remote flyMSData
      2. Calculated values from GPS navigation for autonomous flight
*/
int Setpoint::SetpointManager() {
  dsm2_timeout_ = 0;
  while (is_running_.load()) {
    /**********************************************************
    *           If there is new dsm2 data read it in       *
    *      and make a local copy from the driver's data  *
    **********************************************************/
    std::array<float, RC_MAX_DSM_CHANNELS> dsm2_data;
    if (rc_dsm_is_new_data()) {
      for (int i = 0; i < RC_MAX_DSM_CHANNELS; i++) {
        dsm2_data[i] = rc_dsm_ch_normalized(i + 1);
      }
      dsm2_timeout_ = 0;
    } else {
      if (!is_debug_mode_) {
        //check to make sure too much time hasn't gone by since hearing the RC
        if (RcErrHandler()) {
          return 0;
        }
      }
    }

    setpoint_mutex_.lock();
    HandleRcData(dsm2_data);
    setpoint_mutex_.unlock();
    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<unsigned int>(
      delta_t_ * 1.0E6)));
  }
  return 0;
}


int Setpoint::HandleRcData(std::array<float, RC_MAX_DSM_CHANNELS> dsm2_data) {
  /**********************************************************
  *           Read the RC Controller for Commands           *
  **********************************************************/
  // Set roll/pitch reference value
  // DSM2 Receiver is inherently positive to the left
  if (flight_mode_ == 1) { // Stabilized Flight Mode
    setpoint_data_.euler_ref[0] = -dsm2_data[1] * max_setpoints_stabilized_[0];
    setpoint_data_.euler_ref[1] = dsm2_data[2] * max_setpoints_stabilized_[1];
  } else if (flight_mode_ == 2) {
    setpoint_data_.euler_ref[0] = -dsm2_data[1] * max_setpoints_acro_[0];
    setpoint_data_.euler_ref[1] = dsm2_data[2] * max_setpoints_acro_[1];
  }
  // DSM2 Receiver is inherently positive upwards

  // Set Yaw, RC Controller acts on Yaw velocity, save a history for integration
  // Apply the integration outside of current if statement, needs to run at 200Hz
  setpoint_data_.yaw_rate_ref[1] = setpoint_data_.yaw_rate_ref[0];
  setpoint_data_.yaw_rate_ref[0] = dsm2_data[3] * max_setpoints_stabilized_[2];

  //If Specified by the config file, convert from Drone Coordinate System to User Coordinate System
  if (is_headless_mode_) {
    //TODO: Give this thread access to state information so it can fly in headless mode

    // float P_R_MAG=pow(pow(setpoint_data_.euler_ref[0],2)+pow(setpoint_data_.euler_ref[1],2),0.5);
    // float Theta_Ref=atan2f(setpoint_data_.euler_ref[0],setpoint_data_.euler_ref[1]);

    // setpoint_data_.euler_ref[1] =P_R_MAG*cos(Theta_Ref+state.euler[2]-setpoint_data_.yaw_ref_offset);
    // setpoint_data_.euler_ref[0]=P_R_MAG*sin(Theta_Ref+state.euler[2]-setpoint_data_.yaw_ref_offset);
  }

  // Apply a deadzone to keep integrator from wandering
  if (fabs(setpoint_data_.yaw_rate_ref[0]) < 0.05) {
    setpoint_data_.yaw_rate_ref[0] = 0;
  }

  // Kill Switch
  setpoint_data_.kill_switch[1] = setpoint_data_.kill_switch[0];
  setpoint_data_.kill_switch[0] = dsm2_data[4];

  // Auxillary Switch
  setpoint_data_.Aux[1] = setpoint_data_.Aux[0];
  setpoint_data_.Aux[0] = dsm2_data[5];

  // Set the throttle
  setpoint_data_.throttle = (dsm2_data[0] + 1.0) / 2.0 * (throttle_limits_[1] -
    throttle_limits_[0]) + throttle_limits_[0];

  // Finally Update the integrator on the yaw reference value
  setpoint_data_.euler_ref[2] = setpoint_data_.euler_ref[2] + (setpoint_data_.yaw_rate_ref[0] +
    setpoint_data_.yaw_rate_ref[1]) * delta_t_ / 2;

  ready_to_send_.store(true);

  return 0;
}

void Setpoint::SetYawRef(float ref) {
  setpoint_data_.euler_ref[2] = ref;
}

int Setpoint::RcErrHandler() {
  dsm2_timeout_++;
  if (dsm2_timeout_ > 1.5 / delta_t_) { //If packet hasn't been received for 1.5 seconds
    spdlog::critical("Lost Connection with Remote!! Shutting Down Immediately!");
    rc_set_state(EXITING);
    return -1;
  }
  return 0;
}