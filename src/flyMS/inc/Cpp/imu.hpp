/**
 * @file imu.hpp
 * @brief Source code to read data from the IMU and process it accordingly 
 *
 * @author Mike Sardonini
 * @date 10/15/2018
 */
#ifndef IMU_H
#define IMU_H

// choice of 1,2,4,8,16 oversampling. Here we use 16 and sample at 25hz which
// is close to the update rate specified in robotics_cape.h for that oversample.
#define OVERSAMPLE  BMP_OVERSAMPLE_16
// choice of OFF, 2, 4, 8, 16 filter constants. Here we turn off the filter and 
// opt to use our own 2nd order filter instead.
#define INTERNAL_FILTER	BMP_FILTER_8
#define BMP_CHECK_HZ	1
#define DEG_TO_RAD	0.01744
#define MICROTESLA_TO_GAUSS 0.01f
#define DT_US 5000

//System Includes
#include <iostream>
#include <thread>
#include <mutex>
#include <string>
#include <vector>

//Package Includes
#include <Eigen/Dense>
#include "Fusion.h"
#include "roboticscape.h"
#include "filter.h"

//Ours
#include "ekf.hpp"


//TODO convert these to Eigen vectors and matrices
typedef struct transform_matrix_t{
	rc_matrix_t 	IMU_to_drone; 
	rc_vector_t 	mag_imu;
	rc_vector_t 	gyro_imu;
	rc_vector_t 	accel_imu;
	rc_vector_t 	mag_drone;
	rc_vector_t		gyro_drone;
	rc_vector_t 	accel_drone;
}transform_matrix_t;

typedef struct state_t{
	float	euler[3];					// Euler angles of aircraft (in roll, pitch, yaw)
	float	eulerPrevious[3];			// 1 Timestampe previousEuler angles of aircraft (in roll, pitch, yaw)
	float	eulerRate[3];				// First derivative of euler angles (in roll/s, pitch/s, yaw/s)
	
	float accel[3];
	float gyro[3];
	float mag[3];

	float barometerAltitude;
	float compassHeading;

	int		num_wraps;				// Number of spins in Yaw
	float	initialYaw;
}state_t;


class imu
{
public:
	
	//Default Constructor
	imu(bool enableBarometer);

	//Default Descructor
	~imu();

	/************************************************************************
	*							Initialize the IMU                          *
	************************************************************************/
	int initializeImu();

	/************************************************************************
		imu_handler()
			Does all the parsing and interpretting of the IMU
			5 main tasks
				1. Reads the data from the IMU using Robotics_Cape API
				2. Performs a coordinate system transformation from imu -> drone
				3. Unwraps the yaw value for proper PID control
				4. Reads Barometer for altitude measurement
				5. Sends data to the EKF for position control

	************************************************************************/
	int update();




	/************************************************************************
	*					   Update the EKF with GPS Data                     *
	************************************************************************/
	int update_ekf_gps();

	/************************************************************************
					Get the Latest IMU data from the Object
	************************************************************************/
	int getImuData(state_t* state);



private:
	void init_fusion();
	void updateFusion();
	void read_transform_imu();
	void initializeRotationMatrices();

	//Variables to control the imu thread
	std::thread imuThread;
	std::mutex imuMutex;

	bool enableBarometer;

	ekf_filter_t ekfContainer;

	//Struct to keep all the state information of the aircraft in the body frame
	state_t stateBody;

	//Variables for transforming data between different coordinate systems
	transform_matrix_t transform;

	//Struct to get passed to the roboticsCape API for interfacing with the imu
	rc_imu_data_t imu_data;

	//Boolean to indicate if we are currently initializing the fusion algorithm
	bool isInitializingFusion;

	//Variables which control the Fusion of IMU data for Euler Angle estimation
	FusionVector3 gyroscope;
	FusionVector3 accelerometer;
	FusionVector3 magnetometer;
	FusionAhrs  fusionAhrs;
	FusionEulerAngles eulerAngles;
	FusionBias fusionBias;

};

#endif //IMU_H