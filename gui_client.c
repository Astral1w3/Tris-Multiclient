#include "interface.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    AppUI ui;
    int socket_fd;
    GIOChannel *socket_channel;
    guint socket_watch_id;

    char my_nickname[32];
    char opponent_nickname[32];
    char selected_owner_nickname[32];
    char pending_request_nickname[32];
    int pending_request_game_id;

    char my_symbol;
    char current_turn;
    char board[9];
    int pending_move_index;
    gboolean game_finished;
} AppState;

static void app_handle_server_message(AppState *app, const char *line);

static gboolean app_is_connected(const AppState *app) {
    return app->socket_fd >= 0;
}

static gboolean app_is_my_turn(const AppState *app) {
    return (!app->game_finished &&
            app->my_symbol != '\0' &&
            app->current_turn == app->my_symbol);
}

static void app_refresh_match_title(AppState *app) {
    ui_set_match_title(&app->ui, app->my_nickname, app->opponent_nickname);
}

static void app_refresh_board(AppState *app) {
    ui_set_board(&app->ui, app->board, app_is_my_turn(app), app->game_finished);
}

static void app_refresh_turn_label(AppState *app) {
    if (app->game_finished) {
        ui_set_turn(&app->ui, "Partita terminata.");
        return;
    }

    if (app_is_my_turn(app)) {
        char text[64];
        snprintf(text, sizeof(text), "Tocca a te (%c)", app->my_symbol);
        ui_set_turn(&app->ui, text);
    } else if (app->current_turn == 'X' || app->current_turn == 'O') {
        char text[64];
        snprintf(text, sizeof(text), "Turno avversario (%c)", app->current_turn);
        ui_set_turn(&app->ui, text);
    } else {
        ui_set_turn(&app->ui, "In attesa...");
    }
}

static void app_reset_game_state(AppState *app) {
    memset(app->board, ' ', sizeof(app->board));
    app->my_symbol = '\0';
    app->current_turn = 'X';
    app->pending_move_index = -1;
    app->game_finished = FALSE;
    app->opponent_nickname[0] = '\0';
    app_refresh_match_title(app);
    app_refresh_board(app);
    app_refresh_turn_label(app);
}

static void app_send_command(AppState *app, const char *fmt, ...) {
    char buffer[512];
    va_list args;

    if (!app_is_connected(app)) {
        return;
    }

    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0 || len >= (int)sizeof(buffer) - 2) {
        return;
    }

    buffer[len++] = '\n';
    buffer[len] = '\0';
    (void)write(app->socket_fd, buffer, (size_t)len);
}

static void app_disconnect(AppState *app, gboolean update_ui) {
    if (app->socket_watch_id != 0) {
        g_source_remove(app->socket_watch_id);
        app->socket_watch_id = 0;
    }

    if (app->socket_channel) {
        g_io_channel_unref(app->socket_channel);
        app->socket_channel = NULL;
    }

    if (app->socket_fd >= 0) {
        close(app->socket_fd);
        app->socket_fd = -1;
    }

    if (update_ui) {
        ui_set_connection_state(&app->ui, FALSE);
        ui_set_status(&app->ui, "Disconnesso dal server.");
        ui_show_launcher(&app->ui);
    }
}

static gboolean on_socket_event(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    AppState *app = user_data;

    if ((condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) != 0) {
        app_disconnect(app, TRUE);
        return FALSE;
    }

    while (1) {
        gchar *line = NULL;
        gsize line_len = 0;
        GError *error = NULL;
        GIOStatus status = g_io_channel_read_line(source, &line, &line_len, NULL, &error);

        if (status == G_IO_STATUS_AGAIN) {
            g_clear_error(&error);
            g_free(line);
            break;
        }

        if (status == G_IO_STATUS_EOF) {
            g_clear_error(&error);
            g_free(line);
            app_disconnect(app, TRUE);
            return FALSE;
        }

        if (status == G_IO_STATUS_ERROR) {
            if (error && error->message) {
                ui_set_status(&app->ui, error->message);
            }
            g_clear_error(&error);
            g_free(line);
            app_disconnect(app, TRUE);
            return FALSE;
        }

        if (status == G_IO_STATUS_NORMAL && line) {
            g_strchomp(line);
            if (line[0] != '\0') {
                app_handle_server_message(app, line);
            }
            g_free(line);
            continue;
        }

        g_free(line);
        break;
    }

    return TRUE;
}

static const char *skip_command_name(const char *line, const char *name) {
    const char *ptr = line + strlen(name);
    while (*ptr == ' ') {
        ++ptr;
    }
    return ptr;
}

static void app_handle_games_update(AppState *app, const char *line) {
    const char *payload = skip_command_name(line, "GAMES_UPDATE");
    ui_show_games_from_payload(&app->ui, payload);
}

static void app_handle_join_request(AppState *app, const char *line) {
    int game_id = -1;
    char nickname[32] = {0};

    if (sscanf(line, "JOIN_REQ %d %31s", &game_id, nickname) != 2) {
        return;
    }

    app->pending_request_game_id = game_id;
    g_strlcpy(app->pending_request_nickname, nickname, sizeof(app->pending_request_nickname));
    ui_show_join_request(&app->ui, nickname);
    ui_set_status(&app->ui, "Richiesta di join ricevuta.");
}

static void app_handle_start_game(AppState *app, const char *line) {
    char symbol = '\0';
    int game_id = -1;

    if (sscanf(line, "START_GAME %c %d", &symbol, &game_id) != 2) {
        return;
    }
    (void)game_id;

    memset(app->board, ' ', sizeof(app->board));
    app->my_symbol = symbol;
    app->current_turn = 'X';
    app->pending_move_index = -1;
    app->game_finished = FALSE;

    if (symbol == 'X' && app->pending_request_nickname[0] != '\0') {
        g_strlcpy(app->opponent_nickname, app->pending_request_nickname, sizeof(app->opponent_nickname));
    } else if (symbol == 'O' && app->selected_owner_nickname[0] != '\0') {
        g_strlcpy(app->opponent_nickname, app->selected_owner_nickname, sizeof(app->opponent_nickname));
    }

    app_refresh_match_title(app);
    app_refresh_board(app);
    app_refresh_turn_label(app);

    ui_hide_join_request(&app->ui);
    ui_hide_result(&app->ui);
    ui_show_game(&app->ui);
    ui_set_status(&app->ui, "Partita avviata.");
}

static void app_handle_board_update(AppState *app, const char *line) {
    char board_data[10] = {0};
    char turn = '\0';

    if (sscanf(line, "BOARD_UPD %9s %c", board_data, &turn) != 2) {
        return;
    }

    for (int i = 0; i < 9; ++i) {
        app->board[i] = (board_data[i] == '-') ? ' ' : board_data[i];
    }
    app->current_turn = turn;
    app->pending_move_index = -1;

    app_refresh_board(app);
    app_refresh_turn_label(app);
}

static void app_handle_turn_update(AppState *app, const char *line) {
    char turn = '\0';
    if (sscanf(line, "TURN %c", &turn) != 1) {
        return;
    }

    app->current_turn = turn;
    app_refresh_board(app);
    app_refresh_turn_label(app);
}

static void app_handle_game_over(AppState *app, const char *line) {
    char result[16] = {0};
    const char *result_text = "Partita conclusa.";

    if (sscanf(line, "GAME_OVER %15s", result) != 1) {
        return;
    }

    app->game_finished = TRUE;
    app->pending_move_index = -1;
    app_refresh_board(app);
    app_refresh_turn_label(app);

    if (strcmp(result, "WIN") == 0) {
        result_text = "Hai vinto!";
    } else if (strcmp(result, "LOSS") == 0) {
        result_text = "Hai perso.";
    } else if (strcmp(result, "DRAW") == 0) {
        result_text = "Pareggio.";
    }

    ui_show_result(&app->ui, result_text);
    ui_set_status(&app->ui, "Partita terminata.");
}

static void app_handle_error(AppState *app, const char *line) {
    const char *error_text = skip_command_name(line, "ERROR");

    if (app->pending_move_index >= 0) {
        app->board[app->pending_move_index] = ' ';
        app->pending_move_index = -1;
        app->current_turn = app->my_symbol;
        app_refresh_board(app);
        app_refresh_turn_label(app);
    }

    ui_set_status(&app->ui, error_text);
}

static void app_handle_server_message(AppState *app, const char *line) {
    if (g_str_has_prefix(line, "WELCOME")) {
        ui_set_status(&app->ui, "Connessione al server riuscita.");
        return;
    }

    if (g_str_has_prefix(line, "GAMES_UPDATE")) {
        app_handle_games_update(app, line);
        return;
    }

    if (g_str_has_prefix(line, "JOIN_REQ ")) {
        app_handle_join_request(app, line);
        return;
    }

    if (g_str_has_prefix(line, "START_GAME ")) {
        app_handle_start_game(app, line);
        return;
    }

    if (g_str_has_prefix(line, "BOARD_UPD ")) {
        app_handle_board_update(app, line);
        return;
    }

    if (g_str_has_prefix(line, "TURN ")) {
        app_handle_turn_update(app, line);
        return;
    }

    if (g_str_has_prefix(line, "GAME_OVER ")) {
        app_handle_game_over(app, line);
        return;
    }

    if (g_str_has_prefix(line, "ERROR ")) {
        app_handle_error(app, line);
        return;
    }

    if (g_str_has_prefix(line, "MESSAGE ")) {
        ui_set_status(&app->ui, skip_command_name(line, "MESSAGE"));
        return;
    }

    if (g_str_has_prefix(line, "WAITING ")) {
        ui_set_status(&app->ui, skip_command_name(line, "WAITING"));
        return;
    }

    if (strcmp(line, "OK") == 0) {
        return;
    }
}

static void on_connect_server(const char *ip, const char *port, const char *nickname, gpointer user_data) {
    AppState *app = user_data;
    int parsed_port = atoi(port);

    if (app_is_connected(app)) {
        return;
    }

    if (!nickname || nickname[0] == '\0') {
        ui_set_status(&app->ui, "Inserisci un nickname valido.");
        return;
    }

    if (parsed_port <= 0 || parsed_port > 65535) {
        ui_set_status(&app->ui, "Porta non valida.");
        return;
    }

    // TODO: INSERIRE QUI LA TUA FUNZIONE DI CONNESSIONE RETE.
    // Esempio attuale: socket()/connect() per avere un flusso completo e semplice.
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ui_set_status(&app->ui, "Errore creazione socket.");
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)parsed_port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        ui_set_status(&app->ui, "Indirizzo IP non valido.");
        close(fd);
        return;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ui_set_status(&app->ui, "Connessione fallita.");
        close(fd);
        return;
    }

    app->socket_fd = fd;
    g_strlcpy(app->my_nickname, nickname, sizeof(app->my_nickname));
    app->selected_owner_nickname[0] = '\0';
    app->pending_request_nickname[0] = '\0';
    app->pending_request_game_id = -1;

    app->socket_channel = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(app->socket_channel, FALSE);
    g_io_channel_set_flags(
        app->socket_channel,
        g_io_channel_get_flags(app->socket_channel) | G_IO_FLAG_NONBLOCK,
        NULL);
    app->socket_watch_id = g_io_add_watch(
        app->socket_channel,
        G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
        on_socket_event,
        app);

    ui_set_connection_state(&app->ui, TRUE);
    ui_set_status(&app->ui, "Connessione effettuata.");
    app_refresh_match_title(app);

    // TODO: INSERIRE QUI LA TUA FUNZIONE per inviare nickname/lista lobby.
    app_send_command(app, "SET_NICKNAME %s", nickname);
    app_send_command(app, "LIST_GAMES");
}

static void on_create_game(gpointer user_data) {
    AppState *app = user_data;
    if (!app_is_connected(app)) {
        ui_set_status(&app->ui, "Connettiti prima al server.");
        return;
    }

    // TODO: INSERIRE QUI LA TUA FUNZIONE di creazione partita.
    app_send_command(app, "CREATE_GAME");
    ui_set_status(&app->ui, "Richiesta creazione partita inviata.");
}

static void on_refresh_games(gpointer user_data) {
    AppState *app = user_data;
    if (!app_is_connected(app)) {
        ui_set_status(&app->ui, "Connettiti prima al server.");
        return;
    }

    // TODO: INSERIRE QUI LA TUA FUNZIONE di refresh lobby.
    app_send_command(app, "LIST_GAMES");
}

static void on_join_game(int game_id, const char *owner_nickname, gpointer user_data) {
    AppState *app = user_data;
    if (!app_is_connected(app)) {
        ui_set_status(&app->ui, "Connettiti prima al server.");
        return;
    }

    g_strlcpy(app->selected_owner_nickname, owner_nickname ? owner_nickname : "", sizeof(app->selected_owner_nickname));

    // TODO: INSERIRE QUI LA TUA FUNZIONE di richiesta join.
    app_send_command(app, "JOIN_GAME %d", game_id);
    ui_set_status(&app->ui, "Richiesta di join inviata.");
}

static void on_answer_join(gboolean accepted, gpointer user_data) {
    AppState *app = user_data;
    if (!app_is_connected(app) || app->pending_request_game_id < 0) {
        return;
    }

    // TODO: INSERIRE QUI LA TUA FUNZIONE di risposta al join (yes/no).
    app_send_command(
        app,
        "ANSWER_JOIN %s %d %s",
        accepted ? "yes" : "no",
        app->pending_request_game_id,
        app->pending_request_nickname);

    if (accepted) {
        g_strlcpy(app->opponent_nickname, app->pending_request_nickname, sizeof(app->opponent_nickname));
    }

    app->pending_request_game_id = -1;
    app->pending_request_nickname[0] = '\0';
    ui_hide_join_request(&app->ui);
}

static void on_cell_clicked(int row, int col, gpointer user_data) {
    AppState *app = user_data;
    int idx = row * UI_BOARD_SIZE + col;

    if (!app_is_connected(app) || app->game_finished || !app_is_my_turn(app)) {
        return;
    }

    if (app->board[idx] != ' ') {
        return;
    }

    /* Aggiornamento locale immediato: la cella si disabilita e mostra X/O subito. */
    app->board[idx] = app->my_symbol;
    app->pending_move_index = idx;
    app->current_turn = (app->my_symbol == 'X') ? 'O' : 'X';
    app_refresh_board(app);
    app_refresh_turn_label(app);

    // TODO: INSERIRE QUI LA TUA FUNZIONE di invio mossa.
    app_send_command(app, "MOVE %d %d", row, col);
}

static void on_back_to_lobby(gpointer user_data) {
    AppState *app = user_data;

    if (app_is_connected(app) && app->my_symbol != '\0' && !app->game_finished) {
        app_send_command(app, "LEAVE_GAME");
    }

    ui_hide_result(&app->ui);
    ui_show_launcher(&app->ui);
    app_reset_game_state(app);
    ui_hide_join_request(&app->ui);

    if (app_is_connected(app)) {
        app_send_command(app, "LIST_GAMES");
        ui_set_status(&app->ui, "Rientrato nel launcher.");
    }
}

static void on_exit(gpointer user_data) {
    AppState *app = user_data;
    if (app_is_connected(app)) {
        app_send_command(app, "QUIT");
    }
    app_disconnect(app, FALSE);
}

int main(int argc, char *argv[]) {
    AppState app;
    UiCallbacks callbacks;

    gtk_init(&argc, &argv);

    memset(&app, 0, sizeof(app));
    app.socket_fd = -1;
    app.pending_request_game_id = -1;
    app.pending_move_index = -1;
    memset(app.board, ' ', sizeof(app.board));
    app.current_turn = 'X';

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_connect_server = on_connect_server;
    callbacks.on_create_game = on_create_game;
    callbacks.on_refresh_games = on_refresh_games;
    callbacks.on_join_game = on_join_game;
    callbacks.on_answer_join = on_answer_join;
    callbacks.on_cell_clicked = on_cell_clicked;
    callbacks.on_back_to_lobby = on_back_to_lobby;
    callbacks.on_exit = on_exit;
    callbacks.user_data = &app;

    ui_build(&app.ui, &callbacks);
    app_refresh_match_title(&app);
    app_refresh_board(&app);
    app_refresh_turn_label(&app);

    gtk_main();
    return 0;
}
