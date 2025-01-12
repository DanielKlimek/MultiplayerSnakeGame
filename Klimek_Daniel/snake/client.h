#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

typedef struct {
    int player_id;
    GameWorld *world;
} ClientData;


void *input_thread(void *arg);
void *render_thread(void *arg);
void display_world(GameWorld *world, int player_id);

#endif 

