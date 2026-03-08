#ifndef GAME_H
#define GAME_H

#include <pthread.h>
#include <time.h>
#include "player.h"

// Enum per gli stati della partita
typedef enum {
    STATE_NEW,
    STATE_WAITING,
    STATE_PLAYING,
    STATE_TERMINATED
} GameState;

// Enum per l'esito della partita
typedef enum {
    RESULT_NONE,
    RESULT_WIN,
    RESULT_LOSS,
    RESULT_DRAW
} GameResult;

// Struttura della Partita
typedef struct Game {
    int game_id;
    struct Player* owner;
    struct Player* guest;
    char board[3][3];
    char current_turn;
    GameState state;
    pthread_mutex_t mutex;
    struct Game* next;
    time_t last_move_time;
} Game;

// Variabili globali esterne
extern Game* games_list;
extern pthread_mutex_t games_mutex;

// Funzioni per gestione partite
Game* game_create(int game_id, Player* owner);
void  game_destroy(Game* game);
Game* game_find_by_id(int game_id);
Game* game_find_by_player(Player* player);
int   game_get_next_id(void);

// Funzioni per gestione lista partite
void games_list_add(Game* game);
void games_list_remove(Game* game);
void games_list_cleanup(void);

#endif // GAME_H