#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <pthread.h>

// Struttura del Giocatore
typedef struct Player {
    int socket_fd;
    char nickname[32];
    int current_game_id;
    bool is_playing;
    struct Player* next;
} Player;

// Variabili globali esterne
extern Player* players_list;
extern pthread_mutex_t players_mutex;

// Funzioni per gestione giocatori
Player* player_create(int socket_fd, const char* nickname);
void    player_destroy(Player* player);
Player* player_find_by_nickname(const char* nickname);

// Funzioni per gestione lista giocatori
void players_list_add(Player* player);
void players_list_remove(Player* player);
void players_list_cleanup(void);

#endif // PLAYER_H
