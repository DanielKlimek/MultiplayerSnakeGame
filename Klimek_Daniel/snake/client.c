#include "shared.h"
#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

void *input_thread(void *arg) {
    ClientData *data = (ClientData *)arg;
    unsigned char c;
    GameWorld *world = data->world;

    while (1) {
        c = getch();
        pthread_mutex_lock(&world->mutex);
        Snake *snake = &world->snakes[data->player_id];
        if (!snake->active) {
            pthread_mutex_unlock(&world->mutex);
            break;
        }
        if (snake->paused) {
            if (c == 'p') {
                snake->paused = false;
                mvprintw(world->height + 2, 0, "Pokračujeme v hre za 3 sekundy...");
                refresh();
                pthread_mutex_unlock(&world->mutex);
                sleep(3);
                continue;
            }
            else if (c == 'q') {
                // Hráč odchádza z hry
                remove_snake(world, data->player_id);
                pthread_mutex_unlock(&world->mutex);
                break;
            }
        } else {
            if ((c == 'w' || c == 'W') && snake->direction != 'D')
                snake->direction = 'U';
            else if ((c == 's' || c == 'S') && snake->direction != 'U')
                snake->direction = 'D';
            else if ((c == 'a' || c == 'A') && snake->direction != 'R')
                snake->direction = 'L';
            else if ((c == 'd' || c == 'D') && snake->direction != 'L')
                snake->direction = 'R';
                        else if (c == 'p' || c == 'P') {
                snake->paused = true;
                mvprintw(world->height + 2, 0, "Hra pozastavená. Stlač 'p' pre pokračovanie alebo 'q' pre odchod.");
                refresh();
            }
        }
        pthread_mutex_unlock(&world->mutex);
        usleep(100000);
    }

    pthread_exit(NULL);
}

void *render_thread(void *arg) {
    ClientData *data = (ClientData *)arg;
    GameWorld *world = data->world;

    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);

    while (1) {
        pthread_mutex_lock(&world->mutex);
        if (!world->snakes[data->player_id].active || world->game_over) {
            pthread_mutex_unlock(&world->mutex);
            break;
        }
        display_world(world, data->player_id);
        pthread_mutex_unlock(&world->mutex);
        usleep(100000);
    }

    endwin();
    printf("Hra skončila. Tvoje skóre: %d\n", world->snakes[data->player_id].score);
    pthread_exit(NULL);
}

void display_world(GameWorld *world, int player_id) {
    clear();
    for (int y = 0; y < world->height; y++) {
        for (int x = 0; x < world->width; x++) {
            switch (world->cells[y][x]) {
                case EMPTY:
                    mvprintw(y, x, " ");
                    break;
                case SNAKE:
                    mvprintw(y, x, "S");
                    break;
                case FRUIT:
                    mvprintw(y, x, "F");
                    break;
                case OBSTACLE:
                    mvprintw(y, x, "#");
                    break;
            }
        }
    }
    mvprintw(world->height, 0, "Skóre: %d   Čas: %d", world->snakes[player_id].score, world->time_left);
    refresh();
}

int main() {
    int shm_fd;
    GameWorld *world;
    ClientData data;
    char game_id[256];
    char shm_name[256];

    while (1) {
        printf("=== Hlavné Menu ===\n");
        printf("1. Vytvoriť hru\n");
        printf("2. Pripojiť sa k hre\n");
        printf("3. Koniec\n");
        printf("Vyber možnosť: ");
        int choice;
        scanf("%d", &choice);

        if (choice == 1) {

            pid_t pid = fork();
            if (pid == 0) {

		 char *args[] = {"./server", NULL};
		 execv("./server", args); 
		 perror("Nepodarilo sa spustiť server"); 
		 exit(EXIT_FAILURE);               
            } else {

                sleep(1);


                printf("Zadaj ID novej hry: ");
                scanf("%s", game_id);
                snprintf(shm_name, sizeof(shm_name), "/snake_game_shm_%s", game_id);
            }
        } else if (choice == 2) {

            printf("Zadaj ID hry: ");
            scanf("%s", game_id);
            snprintf(shm_name, sizeof(shm_name), "/snake_game_shm_%s", game_id);
        } else if (choice == 3) {
            printf("Ukončujem hru...\n");
            exit(0);
        } else {
            printf("Neplatná voľba. Skúste znova.\n");
            continue;
        }

        shm_fd = shm_open(shm_name, O_RDWR, 0666);
        if (shm_fd == -1) {
            printf("Nepodarilo sa pripojiť k hre s ID '%s'. Skúste znova.\n", game_id);
            continue;
        }

        world = mmap(NULL, sizeof(GameWorld), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (world == MAP_FAILED) {
            perror("Chyba pri mapovaní zdieľanej pamäte");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&world->mutex);
        int player_id = assign_player_id(world);
        if (player_id == -1) {
            printf("Hra je plná, nemôžete sa pripojiť.\n");
            pthread_mutex_unlock(&world->mutex);
            munmap(world, sizeof(GameWorld));
            continue;
        }
        data.player_id = player_id;
        data.world = world;
        place_snake(world, &world->snakes[player_id]);
        world->num_players++;
        pthread_mutex_unlock(&world->mutex);


        pthread_t input_t, render_t;
        pthread_create(&input_t, NULL, input_thread, (void *)&data);
        pthread_create(&render_t, NULL, render_thread, (void *)&data);

        pthread_join(input_t, NULL);
        pthread_join(render_t, NULL);

        munmap(world, sizeof(GameWorld));


        close(shm_fd);
    }

    return 0;
}

