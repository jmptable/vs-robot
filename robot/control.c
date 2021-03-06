#include <joyos.h>
#include <math.h>

#include "inc/debug.h"
#include "inc/sensors.h"
#include "inc/control.h"
#include "inc/manager.h"
#include "inc/util_math.h"

#define MOVE_R			1000
#define MAX_SPEED		245		//the fastest the robot will go (leave room for PID)
#define ACCEL_ENCODER	10.0

#define GATE_OPEN		110
#define GATE_CLOSED		511

#define LEVER_UP		440
#define LEVER_MIDDLE	245
#define LEVER_DOWN		170

static pid_data pid_linear;

bool navigating = true;

float angle_to_target(int x, int y) {
	return angle_between(bot.x, bot.y, x, y);
}

void go_territory(unsigned char target, int vel) {
	unsigned char current = get_territory(bot.x, bot.y);
	unsigned char error = target-current;
	
	if(error==0 && distance(bot.x, bot.y, arena[current].center.x, arena[current].center.y)<CLOSE_ENOUGH) return;
	if(abs(error>3))
		circle_to(target, vel, CLOCKWISE);
	else
		circle_to(target, vel, COUNTERCLOCKWISE);
}

void go_to(int x, int y, int vel) {
	GRAPH("t %d %d\n", x, y);

	//do {
		PRINT("go_to: going to (%d, %d)\n", x, y);
		float dist = vps_to_encoder(distance(bot.x, bot.y, x, y));
		nav_turn_to(angle_to_target(x, y));
		
		nav_set_velocity(vel);
		int ticks_start = encoder_read_avg();
		while(encoder_read_avg()-ticks_start<dist) {
			nav_set_heading(angle_to_target(x, y));
			yield();
		}	//drive for that length

		nav_stop();
		pause(10);
	//} while(distance(bot.x, bot.y, x, y)>CLOSE_ENOUGH);

	PRINT("go_to: success!\n");
	PRINT("go_to: i think i am at (%d, %d)\n", bot.x, bot.y);
}

float pid_calc_g(pid_data* prefs, float current, float target) {
	float error = target - current;

	if(abs(error) > prefs->epsilon)
		prefs->integral = prefs->integral + error*prefs->dt;
	float derivative = (error - prefs->pre_error)/prefs->dt;
	float output = prefs->Kp*error + prefs->Ki*prefs->integral + prefs->Kd*derivative;

	//Update error
	prefs->pre_error = error;

	return output;
}

void circle_to(unsigned char id, int vel, unsigned char dir) {
	pid_data pid_circle;
	pid_circle.epsilon	= 0.01;
	pid_circle.dt		= 0.01;
	pid_circle.Kp		= 1.00;
	pid_circle.Kd		= 0.1;
	pid_circle.Ki		= 0.01;

	float anglecenter = 360.0*atan2(bot.y, bot.x)/(2.0*M_PI);
	float tangent = anglecenter+(dir==COUNTERCLOCKWISE)? 90: -90;
	nav_turn_to(tangent);
	nav_set_velocity(vel);

	while(bot.territory!=id) {
		anglecenter = 360.0*atan2(bot.y, bot.x)/(2.0*M_PI);
		tangent = anglecenter+(dir==COUNTERCLOCKWISE)? 90: -90;

		float correction = pid_calc_g(&pid_circle, distance(0, 0, bot.x, bot.y), MOVE_R);
		if(correction>45) correction = 45;
		if(correction<-45) correction = -45;

		nav_set_heading(tangent-correction);

		yield();
	}
}

float pid_calc(pid_data* prefs, float current, float target) {
	float error = within(-180, target - current, 180);

	if(abs(error) > prefs->epsilon)
		prefs->integral = prefs->integral + error*prefs->dt;
	float derivative = (error - prefs->pre_error)/prefs->dt;
	float output = prefs->Kp*error + prefs->Ki*prefs->integral + prefs->Kd*derivative;

	//Update error
	prefs->pre_error = error;

	return output;
}

/*
   Motor control abstraction layer
*/

robot bot;

void nav_init(void) {
	//init pid settings
	pid_linear.epsilon	= 0.00;
	pid_linear.dt		= 0.1;
	pid_linear.Kp		= 3.0;
	pid_linear.Kd		= 0.1;
	//pid_linear.Ki		= 0.05;
	pid_linear.Ki		= 0.014;

	//init the settings
	bot.accel = 2;
	bot.deccel = 1;
	bot.w = 5;

	create_thread(&navigator, STACK_DEFAULT, 0, "nav_thread");
}

void nav_set_heading(float heading) {
	bot.target_heading = heading;
}

void nav_set_velocity(int v) {
	bot.target_velocity = v;
}

void nav_stop(void) {
	nav_set_velocity(0);
	bot.velocity = 0;
	motor_brake(MOTOR_LEFT);
	motor_brake(MOTOR_RIGHT);
}

void nav_straight(int distance, int v) {
	nav_set_velocity(v);
	int ticks_start = encoder_read_avg();
	while(encoder_read_avg()-ticks_start<distance) { NOTHING; yield(); }	//drive for that length
}

void nav_turn_to(float heading) {
	nav_set_velocity(0);
	while(abs(bot.velocity)>bot.accel) { NOTHING; yield(); }	//wait until stopped
	gyro_zero();	//take the chance to update
	nav_set_heading(heading);
	while(abs(within(-180, bot.heading-heading, 180))>6) { NOTHING; }	//wait until we face that direction
}

void nav_off(void) {
	navigating = false;
}

void nav_on(void) {
	navigating = true;
}

void tick_motion(void) {
	//forward acceleration
	if(bot.velocity<bot.target_velocity) {
		bot.velocity += bot.accel;
	} else if(bot.velocity>bot.target_velocity) {
		bot.velocity -= bot.deccel;
	}
	if((bot.velocity>=bot.target_velocity-max(bot.accel, bot.deccel))
		   	&& (bot.velocity<=bot.target_velocity+max(bot.accel, bot.deccel)))
		bot.velocity = bot.target_velocity;

	float output = pid_calc(&pid_linear, bot.heading, bot.target_heading);

	#define MAX_TURN	96
	if(output<-MAX_TURN) output = -MAX_TURN;
	if(output>MAX_TURN) output = MAX_TURN;

	motor_set_vel(MOTOR_LEFT, bound(-255, bot.velocity - output, 255));
	motor_set_vel(MOTOR_RIGHT, bound(-255, bot.velocity + output, 255));
}

int navigator(void) {
	for(;;) {
		if(navigating)
			tick_motion();

		yield();
	}

	return 0;
}

void gate_open(void) {
	servo_set_pos(SERVO_GATE, GATE_OPEN);
}

void gate_close(void) {
	servo_set_pos(SERVO_GATE, GATE_CLOSED);
}

void lever_up(void) {
	servo_set_pos(SERVO_LEVER, LEVER_UP);
}

void lever_middle(void) {
	servo_set_pos(SERVO_LEVER, LEVER_MIDDLE);
}

void lever_down(void) {
	servo_set_pos(SERVO_LEVER, LEVER_DOWN);
}

void wheel_capture(int turnspeed){
    motor_set_vel(MOTOR_CAPTURE, turnspeed);
}

