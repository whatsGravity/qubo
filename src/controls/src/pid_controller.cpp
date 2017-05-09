#include "pid_controller.h"

using namespace std;
using namespace ros;


PIDController::PIDController(NodeHandle n, string control_topic):
	m_control_topic(control_topic){

	// Get params if specified in launch file or as params on command-line, set defaults
	n.param<double>("kp", m_kp, 1.0);
	n.param<double>("ki", m_ki, 0.0);
	n.param<double>("kd", m_kd, 0.0);
	n.param<double>("upper_limit", m_upper_limit, 1000.0);
	n.param<double>("lower_limit", m_lower_limit, -1000.0);
	n.param<double>("windup_limit", m_windup_limit, 1000.0);
	//n.param<double>("cutoff_frequency", m_cutoff_frequency, -1.0);
	n.param<bool>("angular_variable" , m_unwind_angle, false);
	n.param<bool>("filtering", m_filter, false);
	//TODO reconfigure bounds for the angle

	
	m_prev_time = ros::Time::now();
	
	//TODO have this be configurable?
	string qubo_namespace = "/qubo/";
	
	// Set up publishers and subscribers
	string sensor_topic = qubo_namespace + control_topic;
	m_sensor_sub = n.subscribe(sensor_topic, 1000, &PIDController::sensorCallback, this);
	
	string command_topic = qubo_namespace + control_topic + "_cmd";
	m_command_pub = n.advertise<std_msgs::Float64>(command_topic, 1000);

	
	f = boost::bind(&PIDController::configCallback, this, _1, _2);
	server.setCallback(f);
	
    
}

PIDController::~PIDController(){}


void PIDController::update() {
	//update our commanded and measured depth.
	ros::spinOnce();

	// Calculate time passed since previous loop
	ros::Duration dt = ros::Time::now() - m_prev_time;
	m_prev_time = ros::Time::now();


	//calculate error, update integrals and derivatives of the error
	m_error = m_desired  - m_current; //proportional term


	//if filtering is turned on we do the simplest possible low pass filter
	if(m_filter){
		m_error = m_error/2 + m_prev_error/2;
	}
			
	//if we are told to unwind our angle then we better do that. 
	if(m_unwind_angle){
		//makes sure we always take the smallest way around the circle
		if(m_error > PI){
			m_error = 2*PI - m_error;
		}else if(m_error < -PI){
			m_error = 2*PI + m_error;
		}
	}

	
	m_error_integral  += m_error * dt.toSec(); //integral term

	//if the integral value is past our windup limit just set it there. 
	if(m_error_integral > m_windup_limit){
		m_error_integral = m_windup_limit;
	}
	else if(m_error_integral < -m_windup_limit){
		m_error_integral = -m_windup_limit;
	}
	
	m_error_derivative = (m_error - m_prev_error)/dt.toSec();


	ROS_INFO("%s: ep = %f ei = %f ed = %f, dt = %f", m_control_topic.c_str(), m_error,  m_error_integral, m_error_derivative, dt.toSec());  
	//sum everything weighted by the given gains. 
	m_command_msg.data = (m_kp*m_error) + (m_ki*m_error_integral) + (m_kd*m_error_derivative);

	//make sure our error term is within limits
	if(m_command_msg.data > m_upper_limit){
		m_command_msg.data = m_upper_limit;
	}
	else if(m_command_msg.data < m_lower_limit){
		m_command_msg.data = m_lower_limit;
	}

	
	m_command_pub.publish(m_command_msg);
	
}

void PIDController::sensorCallback(const std_msgs::Float64::ConstPtr& msg) {
	m_current = msg->data;
	
}


void PIDController::configCallback(controls::TestConfig &config, uint32_t level) {
	ROS_INFO("Reconfigure Request: %f %f %f %f", config.kp, config.ki, config.kd, config.target);
	m_kp = config.kp;
	m_ki = config.ki;
	m_kd = config.kd;
	m_desired = config.target;


	
	m_error = 0;
	m_error_integral = 0; //reset the integral error every time we switch things up
	m_error_derivative = 0;
	
}



