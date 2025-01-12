#ifndef SHARED_H
#define SHARED_H

#include "server.h"


void remove_snake(GameWorld *world, int player_id);
int assign_player_id(GameWorld *world);
void place_snake(GameWorld *world, Snake *snake);

#endif 

