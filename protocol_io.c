#include "protocol_io.h"
#include "protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Scrive tutti i byte del buffer sul socket, gestendo EINTR.
 * @param socket_fd File descriptor del socket.
 * @param buffer    Dati da inviare.
 * @param len       Numero di byte da scrivere.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
static int write_all(int socket_fd, const char* buffer, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t written = write(socket_fd, buffer + sent, len - sent);
        if (written < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (written == 0) return -1;
        sent += (size_t)written;
    }
    return 0;
}

/**
 * @brief Invia un messaggio formattato sul socket, con newline finale (thread-safe).
 * @param socket_fd File descriptor del socket destinatario.
 * @param format    Stringa di formato printf-like.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_message(int socket_fd, const char* format, ...) {
    char buffer[1024];
    va_list args;

    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0 || len >= (int)sizeof(buffer) - 2) return -1;

    buffer[len++] = '\n';
    buffer[len]   = '\0';

    pthread_mutex_lock(&send_mutex);
    int result = write_all(socket_fd, buffer, (size_t)len);
    pthread_mutex_unlock(&send_mutex);
    return result;
}

/**
 * @brief Invia un messaggio di errore al client con prefisso RSP_ERROR.
 * @param socket_fd File descriptor del socket destinatario.
 * @param error_msg Descrizione dell'errore da inviare.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_error(int socket_fd, const char* error_msg) {
    return send_message(socket_fd, "%s %s", RSP_ERROR, error_msg);
}

/**
 * @brief Invia una risposta di conferma (RSP_OK) al client.
 * @param socket_fd File descriptor del socket destinatario.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_ok(int socket_fd) {
    return send_message(socket_fd, "%s", RSP_OK);
}

/**
 * @brief Invia il messaggio di benvenuto al client appena connesso.
 * @param socket_fd File descriptor del socket destinatario.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_welcome(int socket_fd) {
    return send_message(socket_fd, "%s Benvenuto nel server Tris!", RSP_WELCOME);
}

/**
 * @brief Invia la lista delle partite in attesa al client (formato "id:nick;...").
 *        Scorre la lista globale delle partite con stato STATE_WAITING.
 * @param socket_fd File descriptor del socket destinatario.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_games_update(int socket_fd) {
    char buffer[4096];
    size_t used = 0;
    int waiting_games = 0;

    used = (size_t)snprintf(buffer, sizeof(buffer), "%s ", RSP_GAMES_UPDATE);
    if (used >= sizeof(buffer)) return -1;

    pthread_mutex_lock(&games_mutex);
    Game* current = games_list;
    while (current) {
        if (current->state == STATE_WAITING && current->owner) {
            int written = snprintf(
                buffer + used, sizeof(buffer) - used,
                "%d:%s;", current->game_id, current->owner->nickname);
            if (written < 0 || (size_t)written >= sizeof(buffer) - used) break;
            used += (size_t)written;
            waiting_games++;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&games_mutex);

    if (waiting_games == 0) {
        snprintf(buffer + used, sizeof(buffer) - used, "NONE");
    }
    return send_message(socket_fd, "%s", buffer);
}

/**
 * @brief Invia una richiesta di join all'owner della partita.
 * @param socket_fd          Socket dell'owner.
 * @param game_id            ID della partita.
 * @param requester_nickname Nickname del giocatore richiedente.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_join_request(int socket_fd, int game_id, const char* requester_nickname) {
    return send_message(socket_fd, "%s %d %s", RSP_JOIN_REQ, game_id, requester_nickname);
}

/**
 * @brief Notifica al client l'inizio della partita con il simbolo assegnato.
 * @param socket_fd Socket del client.
 * @param symbol    Simbolo assegnato ('X' o 'O').
 * @param game_id   ID della partita avviata.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_start_game(int socket_fd, char symbol, int game_id) {
    return send_message(socket_fd, "%s %c %d", RSP_START_GAME, symbol, game_id);
}

/**
 * @brief Invia lo stato corrente della board come stringa di 9 caratteri e il turno.
 * @param socket_fd Socket del client destinatario.
 * @param game      Puntatore alla partita di cui inviare la board.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_board_update(int socket_fd, Game* game) {
    if (!game) return -1;

    char board_str[10];
    int  idx  = 0;
    char turn = 'X';

    pthread_mutex_lock(&game->mutex);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) {
            char cell = game->board[row][col];
            board_str[idx++] = (cell == ' ') ? '-' : cell;
        }
    board_str[idx] = '\0';
    turn = game->current_turn;
    pthread_mutex_unlock(&game->mutex);

    return send_message(socket_fd, "%s %s %c", RSP_BOARD_UPD, board_str, turn);
}

/**
 * @brief Invia il messaggio di fine partita con il risultato (WIN/LOSS/DRAW).
 * @param socket_fd Socket del client destinatario.
 * @param result    Risultato della partita per il giocatore.
 * @param game      Puntatore alla partita terminata.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_game_over(int socket_fd, GameResult result, Game* game) {
    if (!game) return -1;
    return send_message(socket_fd, "%s %s", RSP_GAME_OVER, game_result_to_string(result));
}

/**
 * @brief Invia al client di chi è il turno corrente.
 * @param socket_fd Socket del client destinatario.
 * @param symbol    Simbolo del giocatore di turno ('X' o 'O').
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_turn(int socket_fd, char symbol) {
    return send_message(socket_fd, "%s %c", RSP_TURN, symbol);
}

/**
 * @brief Invia al client un messaggio di attesa (es. richiesta join in corso).
 * @param socket_fd Socket del client destinatario.
 * @param message   Testo descrittivo dello stato di attesa.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int send_waiting(int socket_fd, const char* message) {
    return send_message(socket_fd, "%s %s", RSP_WAITING, message);
}
