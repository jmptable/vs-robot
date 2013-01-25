#include <joyos.h>
#include <math.h>

#include "inc/control.h"
#include "inc/debug.h"
#include "inc/sensors.h"
#include "inc/util_math.h"
#include "inc/manager.h"

#define GYRO_PORT		8
#define LSB_US_PER_DEG	1400000

unsigned char team;

int usetup(void) {
	blue_init(MYUBBR);

	//set our team # for the VPS
	extern volatile uint8_t robot_id;
	robot_id = 12;

	//initialize the gyro
	gyro_init(GYRO_PORT, LSB_US_PER_DEG, 500L);
	pause(100);

	//figure out what team
	if(vps_y>0)
		team = TEAM_BLUE;
	else
		team = TEAM_RED;

	return 0;
}

int umain(void) {
	sense_init();
	nav_init();

	go_territory(0, frob_read_range(0, 245));
	while(true) { NOTHING; }

    return 0;
}
