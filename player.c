#include "player.h"
#include <stdlib.h>
#include <string.h>

// Definizione variabili globali
Player* players_list = NULL;
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Funzione interna (non esposta nell'header) ---

/**
 * @brief Rimuove un giocatore dalla lista senza acquisire il mutex.
 * @param player Puntatore al giocatore da rimuovere.
 */
static void players_list_remove_unlocked(Player* player) {
    Player* current = players_list;
    Player* prev = NULL;

    while (current) {
        if (current == player) {
            if (prev) {
                prev->next = current->next;
            } else {
                players_list = current->next;
            }
            return;
        }
        prev = current;
        current = current->next;
    }
}

/**
 * @brief Copia il nickname in modo sicuro nel buffer di destinazione (max 31 char).
 * @param dst Buffer di destinazione (32 byte).
 * @param src Stringa sorgente; se NULL o vuota, viene usato "Player".
 */
static void safe_copy_nickname(char dst[32], const char* src) {
    if (!src || src[0] == '\0') {
        strncpy(dst, "Player", 32);
    } else {
        strncpy(dst, src, 31);
    }
    dst[31] = '\0';
}

// --- API pubblica ---

/**
 * @brief Crea un nuovo Player allocato in heap, con socket e nickname inizializzati.
 * @param socket_fd File descriptor del socket del client.
 * @param nickname  Nickname iniziale del giocatore.
 * @return Puntatore al Player creato, oppure NULL in caso di errore.
 */
Player* player_create(int socket_fd, const char* nickname) {
    Player* player = malloc(sizeof(Player));
    if (!player) {
        return NULL;
    }

    player->socket_fd = socket_fd;
    safe_copy_nickname(player->nickname, nickname);
    player->current_game_id = -1;
    player->is_playing = false;
    player->next = NULL;
    return player;
}

/**
 * @brief Libera la memoria occupata da un Player.
 * @param player Puntatore al giocatore da distruggere.
 */
void player_destroy(Player* player) {
    free(player);
}

/**
 * @brief Cerca un giocatore nella lista globale tramite il nickname (thread-safe).
 * @param nickname Nickname da cercare.
 * @return Puntatore al Player trovato, oppure NULL se non presente.
 */
Player* player_find_by_nickname(const char* nickname) {
    if (!nickname || nickname[0] == '\0') {
        return NULL;
    }

    pthread_mutex_lock(&players_mutex);
    Player* current = players_list;
    while (current) {
        if (strcmp(current->nickname, nickname) == 0) {
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&players_mutex);
    return current;
}

/**
 * @brief Aggiunge un giocatore in testa alla lista globale (thread-safe).
 * @param player Puntatore al giocatore da aggiungere.
 */
void players_list_add(Player* player) {
    if (!player) {
        return;
    }
    pthread_mutex_lock(&players_mutex);
    player->next = players_list;
    players_list = player;
    pthread_mutex_unlock(&players_mutex);
}

/**
 * @brief Rimuove un giocatore dalla lista globale (thread-safe, non dealloca).
 * @param player Puntatore al giocatore da rimuovere.
 */
void players_list_remove(Player* player) {
    if (!player) {
        return;
    }
    pthread_mutex_lock(&players_mutex);
    players_list_remove_unlocked(player);
    pthread_mutex_unlock(&players_mutex);
}

/**
 * @brief Distrugge tutti i giocatori nella lista globale e resetta la lista (thread-safe).
 */
void players_list_cleanup(void) {
    pthread_mutex_lock(&players_mutex);
    while (players_list) {
        Player* next = players_list->next;
        player_destroy(players_list);
        players_list = next;
    }
    pthread_mutex_unlock(&players_mutex);
}
