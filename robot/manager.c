#include <joyos.h>
#include <math.h>

#include "inc/manager.h"

#include "inc/sensors.h"
#include "inc/control.h"
#include "inc/util_math.h"
#include "inc/debug.h"

#include "activities/capture.c"
#include "activities/dump.c"
#include "activities/explore.c"
#include "activities/mine.c"

#define ACT_EXPLORE	0
#define ACT_PLAY	1
#define ACT_DUMP	2

#define MOVE_SPEED	96
#define BALLS_MAX	15

int act;
unsigned char points[4];

int get_best(void);

void play(void) {
	/*points[CAPTURE] = 100;
	points[DUMP]	= 40;
	points[EXPLORE]	= 30;
	points[MINE]	= 40;*/

    act = ACT_EXPLORE;

	for(unsigned char i=0; i<6; i++) capture(i);
	halt();

	//main game loop
	while(true) {
		switch(act) {
			case ACT_EXPLORE:
				explore();
				act = 1;
				break;
			case ACT_PLAY:
				//circle finished to 20s left, dump balls if too many.
				vps_update();
				if(vps_get_owner(bot.territory)!=team) {		//if not ours yet
					capture(bot.territory);			//capture it
				} else if(vps_get_balls(bot.territory)>0) {	//if it is ours and there are balls
					mine(bot.territory);						//get the balls
				} else {
					//move to different territory NOT containg opponent
					go_territory(get_best(), MOVE_SPEED);
				}
				break;
			case ACT_DUMP:
				//TODO emergency ball dump before end
				//dump balls anyway even if we think we don't have any?
				break;
		}
	}
}

/*
   returns the minimum # of intermediate territories
*/
int num_hops(unsigned char id) {
	int error = id-bot.territory;
	if(error==0) return 0;
	return abs(id-bot.territory)%3;
}

int get_best(void){
	unsigned char best = 0;
	unsigned char best_pts = 0;

    for(int id=0; id<6; id++) {
        int pts = 0;
        
        //find territories you/enemy aren't in, penalize them below anything else
        if(id==bot.territory || id==other_bot.territory) {
            pts = -1;	//shouldn't be chosen
		} else {
            if(arena[id].owner!=robot_id)	//is it not ours?
                pts += 100;					//we could capture it
            pts += 40*arena[id].balls;		//points for mining it
            pts -= 40*num_hops(id);			//penalize for being far away

			if(pts<0) pts = 0;
        }

        if(pts>best_pts){
            best_pts = pts; //pick best
            best = id;
        }
    }
    return best;
}

void visit_one(unsigned char id) {
	capture(id);
	mine(id);
}
