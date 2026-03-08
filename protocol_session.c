#include "protocol_session.h"
#include "protocol_io.h"
#include "protocol_lobby.h"
#include "protocol_match.h"
#include "protocol.h"

#include <string.h>
#include <unistd.h>
#include <pthread.h>

/**
 * @brief Gestisce il comando NICK: imposta il nickname del giocatore.
 *        Ignora spazi iniziali e rifiuta nickname vuoti.
 * @param player Puntatore al giocatore.
 * @param args   Nuovo nickname da impostare.
 */
void handle_set_nickname(Player* player, const char* args) {
    if (!player || !args) return;

    while (*args == ' ') ++args;
    if (*args == '\0') {
        send_error(player->socket_fd, "Nickname vuoto non consentito");
        return;
    }

    strncpy(player->nickname, args, sizeof(player->nickname) - 1);
    player->nickname[sizeof(player->nickname) - 1] = '\0';
    send_ok(player->socket_fd);
}

/**
 * @brief Gestisce il comando QUIT: disconnette il giocatore dal server.
 *        Se in partita, notifica l'avversario, pulisce lo stato e chiude il socket.
 * @param player Puntatore al giocatore che vuole disconnettersi.
 */
void handle_quit(Player* player) {
    if (!player) return;

    Game* game = find_player_active_game(player);
    if (!game) {
        game = game_find_by_player(player);
    }

    if (game) {
        if (game->owner == player) {
            if (game->guest) {
                set_player_idle(game->guest);
                send_message(game->guest->socket_fd, "%s Owner disconnesso.", RSP_MESSAGE);
            }
            games_list_remove(game);
            game_destroy(game);
        } else {
            pthread_mutex_lock(&game->mutex);
            game->guest = NULL;
            game->state = STATE_WAITING;
            pthread_mutex_unlock(&game->mutex);
            if (game->owner) {
                set_player_idle(game->owner);
                send_message(game->owner->socket_fd, "%s Sfidante disconnesso.", RSP_MESSAGE);
            }
        }
        broadcast_games_update();
    }

    set_player_idle(player);
    close(player->socket_fd);
    players_list_remove(player);
    player_destroy(player);
}
