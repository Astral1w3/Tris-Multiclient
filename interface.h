#ifndef INTERFACE_H
#define INTERFACE_H

#include <gtk/gtk.h>

#define UI_BOARD_SIZE 3

typedef struct {
    GtkWidget *launcher_window;
    GtkWidget *game_window;

    GtkWidget *ip_entry;
    GtkWidget *port_entry;
    GtkWidget *nickname_entry;
    GtkWidget *connect_button;
    GtkWidget *create_button;
    GtkWidget *refresh_button;
    GtkWidget *status_label;

    GtkWidget *games_list;
    GtkWidget *join_box;
    GtkWidget *join_label;

    GtkWidget *match_label;
    GtkWidget *turn_label;
    GtkWidget *board_buttons[UI_BOARD_SIZE][UI_BOARD_SIZE];
    GtkWidget *result_box;
    GtkWidget *result_label;
    GtkWidget *back_button;
} AppUI;

typedef struct {
    void (*on_connect_server)(const char *ip, const char *port, const char *nickname, gpointer user_data);
    void (*on_create_game)(gpointer user_data);
    void (*on_refresh_games)(gpointer user_data);
    void (*on_join_game)(int game_id, const char *owner_nickname, gpointer user_data);
    void (*on_answer_join)(gboolean accepted, gpointer user_data);
    void (*on_cell_clicked)(int row, int col, gpointer user_data);
    void (*on_back_to_lobby)(gpointer user_data);
    void (*on_exit)(gpointer user_data);
    gpointer user_data;
} UiCallbacks;

void ui_build(AppUI *ui, const UiCallbacks *callbacks);

void ui_set_connection_state(AppUI *ui, gboolean connected);
void ui_set_status(AppUI *ui, const char *text);
void ui_show_games_from_payload(AppUI *ui, const char *payload);

void ui_show_join_request(AppUI *ui, const char *nickname);
void ui_hide_join_request(AppUI *ui);

void ui_show_game(AppUI *ui);
void ui_show_launcher(AppUI *ui);

void ui_set_match_title(AppUI *ui, const char *my_nick, const char *other_nick);
void ui_set_turn(AppUI *ui, const char *text);
void ui_set_board(AppUI *ui, const char board[9], gboolean my_turn, gboolean game_finished);

void ui_show_result(AppUI *ui, const char *text);
void ui_hide_result(AppUI *ui);

#endif
