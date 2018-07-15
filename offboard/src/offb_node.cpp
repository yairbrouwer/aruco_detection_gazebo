/**
 * @file offb_node.cpp
 * @brief Offboard control example node, written with MAVROS version 0.19.x, PX4 Pro Flight
 * Stack and tested in Gazebo SITL
 */

#include <ros/ros.h>
// local position and velocity from FCU
#include <geometry_msgs/PoseStamped.h>
// supports matrix and vector support
#include <Eigen/Core>
// changing arming status
#include <mavros_msgs/CommandBool.h>
// Set FCU operation mode
#include <mavros_msgs/SetMode.h>
// source of arming event
#include <mavros_msgs/State.h>
// including tf library
#include <tf/tf.h>
// supports transformations quaternions
#include <Eigen/Geometry>
// imu data orientation computed by FCU
#include <sensor_msgs/Imu.h>
// file input  output stream
#include <iostream>

// this provides scope for identifiers
using namespace std;
using namespace Eigen;

// declaring variables
geometry_msgs::PoseStamped pose;
geometry_msgs::PoseStamped gps_pose;
mavros_msgs::State current_state;
// declaring a 3*3 matrix
Matrix3f R;

// getting the state into a pointer
void state_cb(const mavros_msgs::State::ConstPtr& msg){
    current_state = *msg;
}

// vectors to store position before and after
Vector3f positionbe;
Vector3f positionaf;

// storing gps data in pointer
void gpsCallback(const geometry_msgs::PoseStamped::ConstPtr &msg){
    gps_pose=*msg;
}


void imuCallback(const sensor_msgs::Imu::ConstPtr &msg){

  double x,y,z,w;

  x=msg->orientation.x;
  y=msg->orientation.y;
  z=msg->orientation.z;
  w=msg->orientation.w;
    
//     making a quaternion of position
    Quaternionf quat;
    quat=Eigen::Quaternionf(x,y,z,w);
  
//     making rotation matrix from quaternion
    R=quat.toRotationMatrix();
}


void posecallback(const geometry_msgs::PoseStamped::ConstPtr& msg)
{
//     converting the pose to ground frame
  positionbe[1]=-(msg->pose.position.x);
  positionbe[0]=-(msg->pose.position.y);
  positionbe[2]=-(msg->pose.position.z);
  positionaf=R*positionbe;

  double x=positionaf[0]+gps_pose.pose.position.x;
  double y=positionaf[1]+gps_pose.pose.position.y;
  double z=positionaf[2]+gps_pose.pose.position.z;

 if(z!=0)
 {
   pose.pose.position.x = x;
   pose.pose.position.y = y;
   pose.pose.position.z= z+2;
 }
//cout<<"Aruco "<< x <<'\t'<< y << '\t' << z<<"position"<<pose.pose.position.x<<'\t'<<pose.pose.position.y<<'\t'<<pose.pose.position.z<<endl;
}



int main(int argc, char **argv)
{
    ros::init(argc, argv, "offb_node");
    ros::NodeHandle nh;

    ros::Subscriber state_sub = nh.subscribe<mavros_msgs::State>
            ("mavros/state", 10, state_cb);
    ros::Subscriber imu_sub = nh.subscribe<sensor_msgs::Imu>
            ("/mavros/imu/data",10,imuCallback);
    ros::Subscriber gps_sub = nh.subscribe<geometry_msgs::PoseStamped>
            ("/mavros/local_position/pose",100,gpsCallback);
    ros::Publisher local_pos_pub = nh.advertise<geometry_msgs::PoseStamped>
            ("mavros/setpoint_position/local", 10);
    ros::Subscriber sub = nh.subscribe<geometry_msgs::PoseStamped>
            ("aruco_single/pose", 1000, posecallback);
    ros::ServiceClient arming_client = nh.serviceClient<mavros_msgs::CommandBool>
            ("mavros/cmd/arming");
    ros::ServiceClient set_mode_client = nh.serviceClient<mavros_msgs::SetMode>
            ("mavros/set_mode");

    //the setpoint publishing rate MUST be faster than 2Hz
    ros::Rate rate(20.0);

    // wait for FCU connection
    while(ros::ok() && !current_state.connected){
        ros::spinOnce();
        rate.sleep();
    }


    pose.pose.position.x = 0;
    pose.pose.position.y = 0;
    pose.pose.position.z = 2;

    //send a few setpoints before starting
    for(int i = 100; ros::ok() && i > 0; --i){
        local_pos_pub.publish(pose);
        ros::spinOnce();
        rate.sleep();
    }

    mavros_msgs::SetMode offb_set_mode;
    offb_set_mode.request.custom_mode = "OFFBOARD";

    mavros_msgs::CommandBool arm_cmd;
    arm_cmd.request.value = true;

    ros::Time last_request = ros::Time::now();

    while(ros::ok()){
        if( current_state.mode != "OFFBOARD" &&
            (ros::Time::now() - last_request > ros::Duration(5.0))){
            if( set_mode_client.call(offb_set_mode) &&
                offb_set_mode.response.mode_sent){
                ROS_INFO("Offboard enabled");
            }
            last_request = ros::Time::now();
        } else {
            if( !current_state.armed &&
                (ros::Time::now() - last_request > ros::Duration(5.0))){
                if( arming_client.call(arm_cmd) &&
                    arm_cmd.response.success){
                    ROS_INFO("Vehicle armed");
                }
                last_request = ros::Time::now();
            }
        }

        local_pos_pub.publish(pose);

        ros::spinOnce();
        rate.sleep();
    }

    return 0;
}
