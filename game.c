#include "game.h"
#include <stdlib.h>
#include <string.h>

// Definizione variabili globali
Game* games_list = NULL;
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;

static int next_game_id = 1;

// --- Funzione interna (non esposta nell'header) ---

/**
 * @brief Rimuove una partita dalla lista senza acquisire il mutex.
 * @param game Puntatore alla partita da rimuovere.
 */
static void games_list_remove_unlocked(Game* game) {
    Game* current = games_list;
    Game* prev = NULL;

    while (current) {
        if (current == game) {
            if (prev) {
                prev->next = current->next;
            } else {
                games_list = current->next;
            }
            return;
        }
        prev = current;
        current = current->next;
    }
}

// --- API pubblica ---

/**
 * @brief Crea una nuova partita allocata in heap con owner e board vuota.
 * @param id    Identificativo univoco della partita.
 * @param owner Puntatore al giocatore proprietario (non può essere NULL).
 * @return Puntatore alla Game creata, oppure NULL in caso di errore.
 */
Game* game_create(int id, Player* owner) {
    if (!owner) {
        return NULL;
    }

    Game* game = malloc(sizeof(Game));
    if (!game) {
        return NULL;
    }

    game->game_id = id;
    game->owner = owner;
    game->guest = NULL;
    game->state = STATE_NEW;
    game->current_turn = 'X';
    game->next = NULL;
    game->last_move_time = time(NULL);
    memset(game->board, ' ', sizeof(game->board));
    pthread_mutex_init(&game->mutex, NULL);
    return game;
}

/**
 * @brief Distrugge una partita: rilascia il mutex interno e libera la memoria.
 * @param game Puntatore alla partita da distruggere.
 */
void game_destroy(Game* game) {
    if (!game) {
        return;
    }
    pthread_mutex_destroy(&game->mutex);
    free(game);
}

/**
 * @brief Cerca una partita nella lista globale tramite il suo ID (thread-safe).
 * @param id Identificativo della partita da cercare.
 * @return Puntatore alla Game trovata, oppure NULL.
 */
Game* game_find_by_id(int id) {
    pthread_mutex_lock(&games_mutex);
    Game* current = games_list;
    while (current) {
        if (current->game_id == id) {
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&games_mutex);
    return current;
}

/**
 * @brief Cerca una partita in cui il giocatore è owner o guest (thread-safe).
 * @param p Puntatore al giocatore da cercare.
 * @return Puntatore alla Game trovata, oppure NULL.
 */
Game* game_find_by_player(Player* p) {
    if (!p) {
        return NULL;
    }

    pthread_mutex_lock(&games_mutex);
    Game* current = games_list;
    while (current) {
        if (current->owner == p || current->guest == p) {
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&games_mutex);
    return current;
}

/**
 * @brief Restituisce e incrementa il contatore globale degli ID partita.
 * @return Il prossimo ID univoco disponibile.
 */
int game_get_next_id(void) {
    return next_game_id++;
}

/**
 * @brief Aggiunge una partita in testa alla lista globale (thread-safe).
 * @param game Puntatore alla partita da aggiungere.
 */
void games_list_add(Game* game) {
    if (!game) {
        return;
    }
    pthread_mutex_lock(&games_mutex);
    game->next = games_list;
    games_list = game;
    pthread_mutex_unlock(&games_mutex);
}

/**
 * @brief Rimuove una partita dalla lista globale (thread-safe, non dealloca).
 * @param game Puntatore alla partita da rimuovere.
 */
void games_list_remove(Game* game) {
    if (!game) {
        return;
    }
    pthread_mutex_lock(&games_mutex);
    games_list_remove_unlocked(game);
    pthread_mutex_unlock(&games_mutex);
}

/**
 * @brief Distrugge tutte le partite nella lista globale e resetta la lista (thread-safe).
 */
void games_list_cleanup(void) {
    pthread_mutex_lock(&games_mutex);
    while (games_list) {
        Game* next = games_list->next;
        game_destroy(games_list);
        games_list = next;
    }
    pthread_mutex_unlock(&games_mutex);
}

