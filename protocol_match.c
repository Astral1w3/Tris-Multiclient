#include "protocol_match.h"
#include "protocol_io.h"
#include "protocol_lobby.h"
#include "board.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/**
 * @brief Trova la partita attiva in cui il giocatore è coinvolto come owner o guest.
 *        Verifica che la partita sia in stato STATE_PLAYING.
 * @param player Puntatore al giocatore da cercare.
 * @return Puntatore alla Game attiva, oppure NULL se non trovata.
 */
Game* find_player_active_game(Player* player) {
    if (!player || player->current_game_id <= 0) return NULL;

    Game* game = game_find_by_id(player->current_game_id);
    if (!game) return NULL;

    pthread_mutex_lock(&game->mutex);
    bool is_valid = (game->state == STATE_PLAYING) &&
                    (game->owner == player || game->guest == player);
    pthread_mutex_unlock(&game->mutex);

    return is_valid ? game : NULL;
}

/**
 * @brief Resetta lo stato del giocatore a idle (non in partita, game_id = -1).
 * @param player Puntatore al giocatore da resettare.
 */
void set_player_idle(Player* player) {
    if (!player) return;
    player->is_playing      = false;
    player->current_game_id = -1;
}

/**
 * @brief Termina una partita: invia game_over a entrambi, resetta i giocatori e dealloca.
 * @param game         Puntatore alla partita da terminare.
 * @param owner_result Risultato per l'owner (WIN/LOSS/DRAW).
 * @param guest_result Risultato per il guest (WIN/LOSS/DRAW).
 */
void terminate_game(Game* game, GameResult owner_result, GameResult guest_result) {
    if (!game) return;

    if (game->owner) send_game_over(game->owner->socket_fd, owner_result, game);
    if (game->guest) send_game_over(game->guest->socket_fd, guest_result, game);

    set_player_idle(game->owner);
    set_player_idle(game->guest);

    games_list_remove(game);
    game_destroy(game);
    broadcast_games_update();
}

/**
 * @brief Gestisce il comando MOVE: esegue una mossa sulla board e verifica il vincitore.
 *        Valida coordinate, turno, cella libera, e aggiorna lo stato della partita.
 * @param player Puntatore al giocatore che effettua la mossa.
 * @param args   Stringa con formato "riga colonna" (valori 0..2).
 */
void handle_move(Player* player, const char* args) {
    if (!player || !args || !player->is_playing) return;

    int row = -1, col = -1;
    if (sscanf(args, "%d %d", &row, &col) != 2) {
        send_error(player->socket_fd, "Formato mossa: MOVE <riga> <colonna>");
        return;
    }
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        send_error(player->socket_fd, "Coordinate non valide (usa valori 0..2)");
        return;
    }

    Game* game = find_player_active_game(player);
    if (!game || !game->owner || !game->guest) {
        send_error(player->socket_fd, "Partita non attiva");
        return;
    }

    char symbol = (game->owner == player) ? 'X' : 'O';

    pthread_mutex_lock(&game->mutex);
    if (game->state != STATE_PLAYING) {
        pthread_mutex_unlock(&game->mutex);
        send_error(player->socket_fd, "Partita non attiva");
        return;
    }
    if (game->current_turn != symbol) {
        pthread_mutex_unlock(&game->mutex);
        send_error(player->socket_fd, "Non è il tuo turno");
        return;
    }
    if (game->board[row][col] != ' ') {
        pthread_mutex_unlock(&game->mutex);
        send_error(player->socket_fd, "Cella occupata");
        return;
    }

    game->board[row][col]  = symbol;
    game->current_turn     = (symbol == 'X') ? 'O' : 'X';
    game->last_move_time   = time(NULL);
    pthread_mutex_unlock(&game->mutex);

    send_board_update(game->owner->socket_fd, game);
    send_board_update(game->guest->socket_fd, game);

    char winner = game_check_winner(game);
    if (winner == ' ') {
        send_turn(game->owner->socket_fd, game->current_turn);
        send_turn(game->guest->socket_fd, game->current_turn);
        return;
    }

    game->state = STATE_TERMINATED;
    GameResult owner_result = RESULT_DRAW;
    GameResult guest_result = RESULT_DRAW;
    if (winner == 'X') {
        owner_result = RESULT_WIN;
        guest_result = RESULT_LOSS;
    } else if (winner == 'O') {
        owner_result = RESULT_LOSS;
        guest_result = RESULT_WIN;
    }

    terminate_game(game, owner_result, guest_result);
}

/**
 * @brief Gestisce il comando LEAVE: il giocatore abbandona la partita in corso.
 *        Chi abbandona perde, l'avversario vince. Termina e dealloca la partita.
 * @param player Puntatore al giocatore che vuole uscire.
 */
void handle_leave_game(Player* player) {
    if (!player) return;

    Game* game = find_player_active_game(player);
    if (!game) {
        set_player_idle(player);
        send_ok(player->socket_fd);
        return;
    }

    GameResult owner_result = RESULT_DRAW;
    GameResult guest_result = RESULT_DRAW;
    if (game->owner == player) {
        owner_result = RESULT_LOSS;
        guest_result = RESULT_WIN;
    } else {
        owner_result = RESULT_WIN;
        guest_result = RESULT_LOSS;
    }

    game->state = STATE_TERMINATED;
    terminate_game(game, owner_result, guest_result);
}
