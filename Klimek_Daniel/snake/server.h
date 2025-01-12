#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <pthread.h>

#define MAX_PLAYERS 10
#define MAX_SNAKE_LENGTH 100
#define MAX_WORLD_WIDTH 80
#define MAX_WORLD_HEIGHT 40

typedef enum { EMPTY, SNAKE, FRUIT, OBSTACLE } CellType;

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    int player_id;
    Position positions[MAX_SNAKE_LENGTH];
    int length;
    char direction;
    bool active;
    bool paused;
    int score;
} Snake;

typedef struct {
    Position position;
    bool active;
} Fruit;

typedef struct {
    CellType cells[MAX_WORLD_HEIGHT][MAX_WORLD_WIDTH];
    int width;
    int height;
    Snake snakes[MAX_PLAYERS];
    Fruit fruits[MAX_PLAYERS];
    int num_players;
    int game_mode;  
    int world_type; 
    int time_left;
    int total_time;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool game_over;
} GameWorld;


void generate_game_id(char *game_id, size_t size);
void init_game_world(GameWorld *world, int game_mode, int world_type, int time_limit, int width, int height);
void generate_random_world(GameWorld *world);
int assign_player_id(GameWorld *world);
void place_snake(GameWorld *world, Snake *snake);
void remove_snake(GameWorld *world, int player_id);
bool is_position_free(GameWorld *world, int x, int y);
void spawn_fruits(GameWorld *world);
void move_snakes(GameWorld *world);
void update_time(GameWorld *world);
void *server_loop(void *arg);

#endif 

