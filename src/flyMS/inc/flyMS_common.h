/*
Copyright (c) 2014, Mike Sardonini
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project.
*/
#ifndef FLYMS_COMMON_H
#define FLYMS_COMMON_H

#pragma once
#include "roboticscape.h"
#include "pru_handler_client.h"
#include "Fusion.h"
#include "filter.h"
#include "config.h"
#include "gps.h"



typedef struct setpoint_t{
	float	pitch_ref, roll_ref, yaw_ref[2];	// Reference (Desired) Position
	float	filt_pitch_ref, filt_roll_ref;		// LPF of pitch and roll (because they are a func of yaw)
	float	yaw_rate_ref[2];
	float	Aux[2];
	double	lat_setpoint, lon_setpoint;			// Controller Variables for Autonomous Flight
	float	altitudeSetpointRate;
	float	altitudeSetpoint;
}setpoint_t;


typedef struct ekf_filter_input_t{
	uint64_t IMU_timestamp;
	float mag[3];
	float gyro[3];
	float accel[3];
	
	uint64_t barometer_timestamp; 
	uint8_t barometer_updated;
	float barometer_alt;

	uint8_t gps_updated;
	uint64_t gps_timestamp;
	double gps_latlon[3];
	uint8_t gps_fix;
	uint8_t nsats;

	uint8_t vehicle_land;

}ekf_filter_input_t;


typedef struct ekf_filter_output_t{
	double ned_pos[3];
	double ned_vel[3];
	double ned_acc[3];

	float vertical_time_deriv;
	float gyro[3];

}ekf_filter_output_t;

typedef struct ekf_filter_t{
	ekf_filter_input_t input;
	ekf_filter_output_t output;

}ekf_filter_t;


typedef struct fusion_data_t
{
	FusionVector3 gyroscope;
	FusionVector3 accelerometer;
	FusionVector3 magnetometer;
	FusionAhrs  fusionAhrs;
	FusionEulerAngles eulerAngles;
	FusionBias fusionBias;
}fusion_data_t;

typedef struct transform_matrix_t{
	rc_matrix_t 	IMU_to_drone_dmp, IMU_to_drone_gyro, IMU_to_drone_accel;
	rc_vector_t 	dmp_imu, gyro_imu, accel_imu;
	rc_vector_t 	dmp_drone, gyro_drone, accel_drone;
}transform_matrix_t;



typedef struct control_variables_t{
	float	euler[3];					// Euler angles of aircraft (in roll, pitch, yaw)
	float	euler_previous[3];			// 1 Timestampe previousEuler angles of aircraft (in roll, pitch, yaw)
	float	euler_rate[3];				// First derivative of euler angles (in roll/s, pitch/s, yaw/s)
	float	compass_heading;
	float	d_pitch_f, d_roll_f, d_yaw_f; 		// Filtered First derivative of Eulter Angles	
	float	mag[3];
	float	dpitch_setpoint, droll_setpoint;	// Desired attitude
	int		num_wraps;				// Number of spins in Yaw
	float	unwrapped_yaw[2];					// Some Yaw Varibles
	float	initial_yaw;
	float 	throttle;				
	float	droll_err_integrator;
	float	dpitch_err_integrator;
	float	dyaw_err_integrator;
	float 	uyaw, upitch, uroll, uthrottle;		// Controller effort for each state variable
	float	u[4]; 								// Duty Cycle to send to each motor
	float	time; 								// Time since execution of the program
	float	yaw_ref_offset;
	// float 	alt_rate_ref, d_alt_filt, alt_ref;	//Height Variables for control with Lidar
	// float	height_damping;
	float	baro_alt;							// Barometer Altitude
	// double	initial_pos_lon, initial_pos_lat; 	// Lat & Long positions from GPS
	// double	lat_error, lon_error;
	float 	kill_switch[2];
 
	float	standing_throttle, alt_error;
	transform_matrix_t transform;
	fusion_data_t fusion;
	core_config_t flight_config;
	ekf_filter_t ekf_filter;
	setpoint_t setpoint;
}control_variables_t;




#endif