#include "protocol_lobby.h"
#include "protocol_io.h"
#include "protocol.h"
#include "protocol_match.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

/**
 * @brief Notifica i giocatori in coda su una partita che è già iniziata con un altro.
 *        Li riporta in stato idle e invia loro un errore e la lista aggiornata.
 * @param game_id ID della partita appena avviata.
 * @param owner   Puntatore all'owner della partita (escluso dalla notifica).
 * @param guest   Puntatore al guest accettato (escluso dalla notifica).
 */
static void notify_pending_joiners_game_started(int game_id, Player* owner, Player* guest) {
    pthread_mutex_lock(&players_mutex);
    Player* current = players_list;
    while (current) {
        if (current->current_game_id == game_id &&
            current != owner &&
            current != guest &&
            !current->is_playing) {
            current->current_game_id = -1;
            send_error(current->socket_fd, "Partita iniziata con un altro giocatore");
            send_games_update(current->socket_fd);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&players_mutex);
}

/**
 * @brief Invia a tutti i giocatori non in partita la lista aggiornata delle partite.
 *        Itera la lista globale dei giocatori con mutex (thread-safe).
 */
void broadcast_games_update(void) {
    pthread_mutex_lock(&players_mutex);
    Player* current = players_list;
    while (current) {
        if (!current->is_playing) {
            send_games_update(current->socket_fd);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&players_mutex);
}

/**
 * @brief Gestisce il comando CREATE: crea una nuova partita e la mette in attesa.
 *        Assegna il giocatore come owner e notifica tutti della nuova partita.
 * @param player Puntatore al giocatore che richiede la creazione.
 * @param args   Argomenti del comando (non usati).
 */
void handle_create_game(Player* player, const char* args) {
    (void)args;
    if (!player) return;

    if (player->is_playing) {
        send_error(player->socket_fd, "Sei già in una partita attiva");
        return;
    }

    int   game_id = game_get_next_id();
    Game* game    = game_create(game_id, player);
    if (!game) {
        send_error(player->socket_fd, "Errore creazione partita");
        return;
    }

    game->state            = STATE_WAITING;
    player->current_game_id = game_id;
    games_list_add(game);

    send_ok(player->socket_fd);
    send_message(player->socket_fd, "%s Partita %d creata. In attesa...", RSP_MESSAGE, game_id);
    broadcast_games_update();
}

/**
 * @brief Gestisce il comando LIST: invia al giocatore la lista delle partite disponibili.
 * @param player Puntatore al giocatore che richiede la lista.
 */
void handle_list_games(Player* player) {
    if (!player) return;
    send_games_update(player->socket_fd);
}

/**
 * @brief Gestisce il comando JOIN: invia una richiesta di join all'owner della partita.
 *        Valida ID, stato della partita e che il giocatore non sia già occupato.
 * @param player Puntatore al giocatore che vuole unirsi.
 * @param args   Stringa contenente l'ID della partita.
 */
void handle_join_game(Player* player, const char* args) {
    if (!player || !args) return;

    if (player->is_playing) {
        send_error(player->socket_fd, "Sei già in una partita");
        return;
    }

    int game_id = -1;
    if (sscanf(args, "%d", &game_id) != 1 || game_id <= 0) {
        send_error(player->socket_fd, "ID partita non valido");
        return;
    }

    Game* game = game_find_by_id(game_id);
    if (!game || !game->owner) {
        send_error(player->socket_fd, "Partita non trovata");
        return;
    }

    pthread_mutex_lock(&game->mutex);
    if (game->state != STATE_WAITING) {
        pthread_mutex_unlock(&game->mutex);
        send_error(player->socket_fd, "Partita non disponibile");
        return;
    }
    pthread_mutex_unlock(&game->mutex);

    if (game->owner == player) {
        send_error(player->socket_fd, "Non puoi unirti alla tua partita");
        return;
    }
    if (game->owner->is_playing) {
        send_error(player->socket_fd, "Owner occupato in un'altra partita");
        return;
    }

    player->current_game_id = game_id;
    send_join_request(game->owner->socket_fd, game_id, player->nickname);
    send_waiting(player->socket_fd, "Richiesta inviata. Attendi...");
}

/**
 * @brief Gestisce il comando ANSWER: l'owner accetta o rifiuta una richiesta di join.
 *        Se accettata, avvia la partita con i due giocatori e notifica tutti.
 * @param player Puntatore all'owner della partita.
 * @param args   Stringa con formato "yes|no <game_id> <guest_nick>".
 */
void handle_answer_join(Player* player, const char* args) {
    if (!player || !args) return;

    char answer[16], guest_nick[32];
    int  game_id = -1;

    if (sscanf(args, "%15s %d %31s", answer, &game_id, guest_nick) != 3) {
        send_error(player->socket_fd, "Formato errato");
        return;
    }
    if (strcmp(answer, ANSWER_YES) != 0 && strcmp(answer, ANSWER_NO) != 0) {
        send_error(player->socket_fd, "Risposta valida: yes oppure no");
        return;
    }

    Game* game = game_find_by_id(game_id);
    if (!game || game->owner != player) {
        send_error(player->socket_fd, "Partita non valida");
        return;
    }

    Player* guest = player_find_by_nickname(guest_nick);
    if (!guest) {
        send_error(player->socket_fd, "Giocatore non trovato");
        return;
    }
    if (guest->current_game_id != game_id) {
        send_error(player->socket_fd, "Richiesta join non più valida");
        return;
    }

    if (strcmp(answer, ANSWER_YES) == 0) {
        if (player->is_playing && player->current_game_id != game_id) {
            send_error(player->socket_fd, "Sei già in un'altra partita attiva");
            return;
        }
        if (guest->is_playing) {
            send_error(player->socket_fd, "Giocatore già impegnato in un'altra partita");
            return;
        }
    }

    pthread_mutex_lock(&game->mutex);
    if (game->state != STATE_WAITING) {
        pthread_mutex_unlock(&game->mutex);
        send_error(player->socket_fd, "Partita già iniziata");
        return;
    }

    if (strcmp(answer, ANSWER_NO) == 0) {
        pthread_mutex_unlock(&game->mutex);
        guest->current_game_id = -1;
        send_error(guest->socket_fd, "Richiesta rifiutata");
        send_ok(player->socket_fd);
        return;
    }

    game->guest            = guest;
    game->state            = STATE_PLAYING;
    player->is_playing     = true;
    guest->is_playing      = true;
    player->current_game_id = game_id;
    guest->current_game_id  = game_id;
    pthread_mutex_unlock(&game->mutex);

    notify_pending_joiners_game_started(game_id, player, guest);

    send_start_game(player->socket_fd, 'X', game_id);
    send_start_game(guest->socket_fd,  'O', game_id);
    send_board_update(player->socket_fd, game);
    send_board_update(guest->socket_fd,  game);
    send_turn(player->socket_fd, 'X');
    send_turn(guest->socket_fd,  'X');
    broadcast_games_update();
}
