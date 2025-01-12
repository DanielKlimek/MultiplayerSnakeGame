#include "shared.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>


void generate_game_id(char *game_id, size_t size) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(NULL) ^ getpid());
    for (size_t i = 0; i < size - 1; i++) {
        int key = rand() % (int)(sizeof(charset) - 1);
        game_id[i] = charset[key];
    }
    game_id[size - 1] = '\0';
}

void init_game_world(GameWorld *world, int game_mode, int world_type, int time_limit, int width, int height) {
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&world->mutex, &mutexattr);

    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&world->cond, &condattr);

    world->num_players = 0;
    world->game_mode = game_mode;
    world->world_type = world_type;
    world->time_left = time_limit;
    world->total_time = time_limit;
    world->width = width;
    world->height = height;
    world->game_over = false;


    for (int i = 0; i < world->height; i++) {
        for (int j = 0; j < world->width; j++) {
            world->cells[i][j] = EMPTY;
        }
    }

    if (world_type == 1) {
        generate_random_world(world);
    }
}

void generate_random_world(GameWorld *world) {
    int num_obstacles = (world->width * world->height) / 10;
    srand(time(NULL));
    int attempts = 0;

    for (int i = 0; i < num_obstacles; i++) {
        int x = rand() % world->width;
        int y = rand() % world->height;

        if (world->cells[y][x] == EMPTY) {
            world->cells[y][x] = OBSTACLE;
        } else {
            i--; 
            attempts++;
            if (attempts > 1000)
                break; 
        }
    }
}

int assign_player_id(GameWorld *world) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!world->snakes[i].active) {
            return i;
        }
    }
    return -1; 
}

bool is_position_free(GameWorld *world, int x, int y) {
    if (world->cells[y][x] == EMPTY || world->cells[y][x] == FRUIT) {
        return true;
    }
    return false;
}

void place_snake(GameWorld *world, Snake *snake) {
    srand(time(NULL) + snake->player_id); 
    for (int attempts = 0; attempts < 1000; attempts++) {
        int x = rand() % world->width;
        int y = rand() % world->height;
        if (is_position_free(world, x, y)) {
            snake->positions[0].x = x;
            snake->positions[0].y = y;
            snake->length = 1;
            snake->direction = 'R'; 
            snake->active = true;
            snake->paused = false;
            snake->score = 0;
            world->cells[y][x] = SNAKE;
            break;
        }
    }
}

void remove_snake(GameWorld *world, int player_id) {
    Snake *snake = &world->snakes[player_id];
    for (int i = 0; i < snake->length; i++) {
        int x = snake->positions[i].x;
        int y = snake->positions[i].y;
        if (x >= 0 && x < world->width && y >= 0 && y < world->height)
            if (world->cells[y][x] == SNAKE)
                world->cells[y][x] = EMPTY;
    }
    snake->active = false;
    world->num_players--;
}

void spawn_fruits(GameWorld *world) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!world->fruits[i].active) {
            for (int attempts = 0; attempts < 1000; attempts++) {
                int x = rand() % world->width;
                int y = rand() % world->height;
                if (is_position_free(world, x, y)) {
                    world->fruits[i].position.x = x;
                    world->fruits[i].position.y = y;
                    world->fruits[i].active = true;
                    world->cells[y][x] = FRUIT;
                    break;
                }
            }
        }
    }
}

void move_snakes(GameWorld *world) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Snake *snake = &world->snakes[i];
        if (!snake->active || snake->paused)
            continue;

        Position new_head = snake->positions[0];


        switch (snake->direction) {
            case 'U':
                new_head.y -= 1;
                break;
            case 'D':
                new_head.y += 1;
                break;
            case 'L':
                new_head.x -= 1;
                break;
            case 'R':
                new_head.x += 1;
                break;
        }


        if (new_head.x < 0)
            new_head.x = world->width - 1;
        else if (new_head.x >= world->width)
            new_head.x = 0;
        if (new_head.y < 0)
            new_head.y = world->height - 1;
        else if (new_head.y >= world->height)
            new_head.y = 0;


        bool collision = false;
        for (int j = 0; j < snake->length; j++) {
            if (snake->positions[j].x == new_head.x && snake->positions[j].y == new_head.y) {
                collision = true;
                break;
            }
        }

 
        CellType cell = world->cells[new_head.y][new_head.x];
        if (cell == OBSTACLE || cell == SNAKE) {
            collision = true;
        }

        if (collision) {

            remove_snake(world, i);
            continue;
        }

        Position tail = snake->positions[snake->length - 1];
        world->cells[tail.y][tail.x] = EMPTY;


        for (int j = snake->length - 1; j > 0; j--) {
            snake->positions[j] = snake->positions[j - 1];
        }
        snake->positions[0] = new_head;

        world->cells[new_head.y][new_head.x] = SNAKE;


        if (cell == FRUIT) {
            snake->score += 10;
            snake->length++;
            if (snake->length > MAX_SNAKE_LENGTH)
                snake->length = MAX_SNAKE_LENGTH;
            world->fruits[i].active = false;
        }
    }
}

void update_time(GameWorld *world) {
    if (world->game_mode == 1) {
        world->time_left--;
        if (world->time_left <= 0) {
            world->time_left = 0;
            world->game_over = true;
        }
    }
}

void *server_loop(void *arg) {
    GameWorld *world = (GameWorld *)arg;
    while (!world->game_over) {
        pthread_mutex_lock(&world->mutex);
        move_snakes(world);
        spawn_fruits(world);
        update_time(world);
        pthread_cond_broadcast(&world->cond);
        pthread_mutex_unlock(&world->mutex);
        sleep(1);
    }
    pthread_exit(NULL);
}

int main() {

    char game_id[7];
    generate_game_id(game_id, sizeof(game_id));


    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/snake_game_shm_%s", game_id);

    printf("====================================\n");
    printf("Vytvorená nová hra s ID: %s\n", game_id);
    printf("Zdieľané meno pamäte: %s\n", shm_name);
    printf("Poznačte si toto ID a poskytnite ho ostatným hráčom pre pripojenie.\n");
    printf("====================================\n");

    int game_mode, world_type, time_limit, width, height;

    printf("Zadaj herný režim (0 - štandardný, 1 - časový): ");
    scanf("%d", &game_mode);

    printf("Zadaj typ sveta (0 - bez prekážok, 1 - s prekážkami): ");
    scanf("%d", &world_type);

    time_limit = 0;
    if (game_mode == 1) {
        printf("Zadaj čas hry v sekundách: ");
        scanf("%d", &time_limit);
    }

    printf("Zadaj šírku sveta: ");
    scanf("%d", &width);

    printf("Zadaj výšku sveta: ");
    scanf("%d", &height);

    if (width > MAX_WORLD_WIDTH || height > MAX_WORLD_HEIGHT) {
        printf("Maximálna veľkosť sveta je %dx%d\n", MAX_WORLD_WIDTH, MAX_WORLD_HEIGHT);
        exit(EXIT_FAILURE);
    }


    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Chyba pri vytváraní zdieľanej pamäte");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(GameWorld));


    GameWorld *world = mmap(NULL, sizeof(GameWorld), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (world == MAP_FAILED) {
        perror("Chyba pri mapovaní zdieľanej pamäte");
        exit(EXIT_FAILURE);
    }

    init_game_world(world, game_mode, world_type, time_limit, width, height);


    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_loop, (void *)world);
    pthread_join(server_thread, NULL);

    munmap(world, sizeof(GameWorld));
    shm_unlink(shm_name);

    return 0;
}

