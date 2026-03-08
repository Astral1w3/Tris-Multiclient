#include "interface.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int game_id;
    char owner_nickname[32];
} GameRowData;

static UiCallbacks g_callbacks;
static AppUI *g_ui = NULL;

static void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        "#list-title { font-size: 18px; font-weight: 700; }\n"
        "#match-title { font-size: 30px; font-weight: 700; }\n"
        "#turn-title { font-size: 18px; }\n"
        ".cell-button { font-size: 42px; font-weight: 700; }\n";

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GameRowData *game_row_data_new(int game_id, const char *owner) {
    GameRowData *data = g_new0(GameRowData, 1);
    data->game_id = game_id;
    g_strlcpy(data->owner_nickname, owner ? owner : "", sizeof(data->owner_nickname));
    return data;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (g_callbacks.on_exit) {
        g_callbacks.on_exit(g_callbacks.user_data);
    }
    gtk_main_quit();
}

static void on_connect_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (!g_callbacks.on_connect_server || !g_ui) {
        return;
    }
    g_callbacks.on_connect_server(
        gtk_entry_get_text(GTK_ENTRY(g_ui->ip_entry)),
        gtk_entry_get_text(GTK_ENTRY(g_ui->port_entry)),
        gtk_entry_get_text(GTK_ENTRY(g_ui->nickname_entry)),
        g_callbacks.user_data);
}

static void on_create_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (g_callbacks.on_create_game) {
        g_callbacks.on_create_game(g_callbacks.user_data);
    }
}

static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (g_callbacks.on_refresh_games) {
        g_callbacks.on_refresh_games(g_callbacks.user_data);
    }
}

static void on_join_button_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    GameRowData *data = g_object_get_data(G_OBJECT(button), "game_row_data");
    if (!data || !g_callbacks.on_join_game) {
        return;
    }
    g_callbacks.on_join_game(data->game_id, data->owner_nickname, g_callbacks.user_data);
}

static void on_games_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    (void)user_data;
    GameRowData *data = g_object_get_data(G_OBJECT(row), "game_row_data");
    if (!data || !g_callbacks.on_join_game) {
        return;
    }
    g_callbacks.on_join_game(data->game_id, data->owner_nickname, g_callbacks.user_data);
}

static void on_answer_yes_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (g_callbacks.on_answer_join) {
        g_callbacks.on_answer_join(TRUE, g_callbacks.user_data);
    }
}

static void on_answer_no_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (g_callbacks.on_answer_join) {
        g_callbacks.on_answer_join(FALSE, g_callbacks.user_data);
    }
}

static void on_cell_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    if (!g_callbacks.on_cell_clicked) {
        return;
    }

    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "cell_idx"));
    int row = idx / UI_BOARD_SIZE;
    int col = idx % UI_BOARD_SIZE;
    g_callbacks.on_cell_clicked(row, col, g_callbacks.user_data);
}

static void on_back_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    if (g_callbacks.on_back_to_lobby) {
        g_callbacks.on_back_to_lobby(g_callbacks.user_data);
    }
}

static void clear_games_list(AppUI *ui) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(ui->games_list));
    for (GList *it = children; it != NULL; it = it->next) {
        gtk_widget_destroy(GTK_WIDGET(it->data));
    }
    g_list_free(children);
}

static void add_empty_games_row(AppUI *ui) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new("Nessuna partita disponibile.");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_container_add(GTK_CONTAINER(row), label);
    gtk_container_add(GTK_CONTAINER(ui->games_list), row);
    gtk_widget_show_all(row);
}

static void add_game_row(AppUI *ui, int game_id, const char *owner) {
    char text[128];
    GameRowData *row_data = game_row_data_new(game_id, owner);
    GameRowData *btn_data = game_row_data_new(game_id, owner);

    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *label = gtk_label_new(NULL);
    GtkWidget *button = gtk_button_new_with_label("Collega");

    snprintf(text, sizeof(text), "Partita #%d    Owner: %s", game_id, owner);
    gtk_label_set_text(GTK_LABEL(label), text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    g_object_set_data_full(G_OBJECT(row), "game_row_data", row_data, g_free);
    g_object_set_data_full(G_OBJECT(button), "game_row_data", btn_data, g_free);
    g_signal_connect(button, "clicked", G_CALLBACK(on_join_button_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(line), label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(line), button, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(row), line);
    gtk_container_add(GTK_CONTAINER(ui->games_list), row);
    gtk_widget_show_all(row);
}

static void build_launcher_window(AppUI *ui) {
    /* Finestra 1: launcher con connessione e lista partite */
    ui->launcher_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui->launcher_window), "Tris - Launcher");
    gtk_window_set_default_size(GTK_WINDOW(ui->launcher_window), 560, 600);
    g_signal_connect(ui->launcher_window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    gtk_container_add(GTK_CONTAINER(ui->launcher_window), main_box);

    /* Header: campi IP, porta, nickname e bottone di connessione */
    GtkWidget *header_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(header_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(header_grid), 8);

    GtkWidget *ip_label = gtk_label_new("Indirizzo IP");
    GtkWidget *port_label = gtk_label_new("Porta");
    GtkWidget *nick_label = gtk_label_new("Nickname");

    ui->ip_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ui->ip_entry), "127.0.0.1");
    ui->port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ui->port_entry), "8888");
    ui->nickname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ui->nickname_entry), "es. Mario");

    ui->connect_button = gtk_button_new_with_label("Connetti");
    g_signal_connect(ui->connect_button, "clicked", G_CALLBACK(on_connect_clicked), NULL);

    gtk_grid_attach(GTK_GRID(header_grid), ip_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header_grid), ui->ip_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(header_grid), port_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header_grid), ui->port_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(header_grid), nick_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(header_grid), ui->nickname_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(header_grid), ui->connect_button, 0, 3, 2, 1);

    gtk_box_pack_start(GTK_BOX(main_box), header_grid, FALSE, FALSE, 0);

    /* Azioni rapide per creare partita e aggiornare la lobby */
    GtkWidget *actions_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ui->create_button = gtk_button_new_with_label("Crea partita");
    ui->refresh_button = gtk_button_new_with_label("Aggiorna lista");
    g_signal_connect(ui->create_button, "clicked", G_CALLBACK(on_create_clicked), NULL);
    g_signal_connect(ui->refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(actions_box), ui->create_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions_box), ui->refresh_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), actions_box, FALSE, FALSE, 0);

    /* Corpo centrale: lista partite */
    GtkWidget *list_title = gtk_label_new("LISTA PARTITE");
    gtk_widget_set_name(list_title, "list-title");
    gtk_widget_set_halign(list_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), list_title, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    ui->games_list = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(ui->games_list), FALSE);
    g_signal_connect(ui->games_list, "row-activated", G_CALLBACK(on_games_row_activated), NULL);
    gtk_container_add(GTK_CONTAINER(scroll), ui->games_list);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 0);

    /* Box semplice per accettare/rifiutare richieste di join */
    ui->join_box = gtk_frame_new("Richiesta di join");
    GtkWidget *join_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(join_content), 8);
    ui->join_label = gtk_label_new("");
    gtk_widget_set_halign(ui->join_label, GTK_ALIGN_START);
    GtkWidget *join_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *yes_btn = gtk_button_new_with_label("Accetta");
    GtkWidget *no_btn = gtk_button_new_with_label("Rifiuta");
    g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_answer_yes_clicked), NULL);
    g_signal_connect(no_btn, "clicked", G_CALLBACK(on_answer_no_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(join_buttons), yes_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(join_buttons), no_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(join_content), ui->join_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(join_content), join_buttons, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(ui->join_box), join_content);
    gtk_box_pack_start(GTK_BOX(main_box), ui->join_box, FALSE, FALSE, 0);

    /* Riga di stato in basso */
    ui->status_label = gtk_label_new("Inserisci i dati e premi Connetti.");
    gtk_widget_set_halign(ui->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), ui->status_label, FALSE, FALSE, 0);
}

static void build_game_window(AppUI *ui) {
    /* Finestra 2: schermata partita con top bar e griglia 3x3 */
    ui->game_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui->game_window), "Tris - Partita");
    gtk_window_set_default_size(GTK_WINDOW(ui->game_window), 520, 620);
    g_signal_connect(ui->game_window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 16);
    gtk_container_add(GTK_CONTAINER(ui->game_window), main_box);

    ui->match_label = gtk_label_new("MIO NICK vs AVVERSARIO");
    gtk_widget_set_name(ui->match_label, "match-title");
    gtk_box_pack_start(GTK_BOX(main_box), ui->match_label, FALSE, FALSE, 0);

    ui->turn_label = gtk_label_new("In attesa del turno...");
    gtk_widget_set_name(ui->turn_label, "turn-title");
    gtk_box_pack_start(GTK_BOX(main_box), ui->turn_label, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(grid, TRUE);

    for (int row = 0; row < UI_BOARD_SIZE; ++row) {
        for (int col = 0; col < UI_BOARD_SIZE; ++col) {
            int idx = row * UI_BOARD_SIZE + col;
            GtkWidget *button = gtk_button_new_with_label("");
            gtk_widget_set_size_request(button, 120, 120);
            gtk_style_context_add_class(gtk_widget_get_style_context(button), "cell-button");
            g_object_set_data(G_OBJECT(button), "cell_idx", GINT_TO_POINTER(idx));
            g_signal_connect(button, "clicked", G_CALLBACK(on_cell_clicked), NULL);
            gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
            ui->board_buttons[row][col] = button;
        }
    }
    gtk_box_pack_start(GTK_BOX(main_box), grid, TRUE, TRUE, 0);

    /* Box finale mostrato solo a partita conclusa */
    ui->result_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    ui->result_label = gtk_label_new("");
    ui->back_button = gtk_button_new_with_label("Torna al launcher");
    g_signal_connect(ui->back_button, "clicked", G_CALLBACK(on_back_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(ui->result_box), ui->result_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ui->result_box), ui->back_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), ui->result_box, FALSE, FALSE, 0);
}

void ui_build(AppUI *ui, const UiCallbacks *callbacks) {
    memset(ui, 0, sizeof(*ui));
    memset(&g_callbacks, 0, sizeof(g_callbacks));
    if (callbacks) {
        g_callbacks = *callbacks;
    }
    g_ui = ui;

    apply_css();
    build_launcher_window(ui);
    build_game_window(ui);

    ui_set_connection_state(ui, FALSE);
    ui_show_games_from_payload(ui, "NONE");
    ui_hide_join_request(ui);
    ui_hide_result(ui);

    gtk_widget_show_all(ui->launcher_window);
    gtk_widget_hide(ui->join_box);
    gtk_widget_hide(ui->game_window);
}

void ui_set_connection_state(AppUI *ui, gboolean connected) {
    gtk_widget_set_sensitive(ui->ip_entry, !connected);
    gtk_widget_set_sensitive(ui->port_entry, !connected);
    gtk_widget_set_sensitive(ui->nickname_entry, !connected);
    gtk_widget_set_sensitive(ui->connect_button, !connected);
    gtk_widget_set_sensitive(ui->create_button, connected);
    gtk_widget_set_sensitive(ui->refresh_button, connected);
    gtk_widget_set_sensitive(ui->games_list, connected);
}

void ui_set_status(AppUI *ui, const char *text) {
    gtk_label_set_text(GTK_LABEL(ui->status_label), text ? text : "");
}

void ui_show_games_from_payload(AppUI *ui, const char *payload) {
    gboolean has_games = FALSE;
    clear_games_list(ui);

    if (!payload || payload[0] == '\0' || strcmp(payload, "NONE") == 0) {
        add_empty_games_row(ui);
        return;
    }

    char *copy = g_strdup(payload);
    char *token = strtok(copy, ";");
    while (token) {
        int game_id = 0;
        char owner[32] = {0};
        if (sscanf(token, "%d:%31s", &game_id, owner) == 2) {
            add_game_row(ui, game_id, owner);
            has_games = TRUE;
        }
        token = strtok(NULL, ";");
    }
    g_free(copy);

    if (!has_games) {
        add_empty_games_row(ui);
    }
}

void ui_show_join_request(AppUI *ui, const char *nickname) {
    char text[128];
    snprintf(text, sizeof(text), "%s vuole unirsi alla tua partita.", nickname ? nickname : "Un utente");
    gtk_label_set_text(GTK_LABEL(ui->join_label), text);
    gtk_widget_show(ui->join_box);
}

void ui_hide_join_request(AppUI *ui) {
    gtk_widget_hide(ui->join_box);
}

void ui_show_game(AppUI *ui) {
    gtk_widget_hide(ui->launcher_window);
    gtk_widget_show_all(ui->game_window);
    gtk_window_present(GTK_WINDOW(ui->game_window));
}

void ui_show_launcher(AppUI *ui) {
    gtk_widget_hide(ui->game_window);
    gtk_widget_show_all(ui->launcher_window);
    gtk_window_present(GTK_WINDOW(ui->launcher_window));
}

void ui_set_match_title(AppUI *ui, const char *my_nick, const char *other_nick) {
    char text[128];
    snprintf(text, sizeof(text), "%s vs %s",
             (my_nick && my_nick[0]) ? my_nick : "MIO NICK",
             (other_nick && other_nick[0]) ? other_nick : "AVVERSARIO");
    gtk_label_set_text(GTK_LABEL(ui->match_label), text);
}

void ui_set_turn(AppUI *ui, const char *text) {
    gtk_label_set_text(GTK_LABEL(ui->turn_label), text ? text : "");
}

void ui_set_board(AppUI *ui, const char board[9], gboolean my_turn, gboolean game_finished) {
    for (int row = 0; row < UI_BOARD_SIZE; ++row) {
        for (int col = 0; col < UI_BOARD_SIZE; ++col) {
            int idx = row * UI_BOARD_SIZE + col;
            GtkWidget *button = ui->board_buttons[row][col];
            char cell = board[idx];

            if (cell == 'X' || cell == 'O') {
                char label[2] = {cell, '\0'};
                gtk_button_set_label(GTK_BUTTON(button), label);
            } else {
                gtk_button_set_label(GTK_BUTTON(button), "");
            }

            gtk_widget_set_sensitive(button, (!game_finished && my_turn && cell == ' '));
        }
    }
}

void ui_show_result(AppUI *ui, const char *text) {
    gtk_label_set_text(GTK_LABEL(ui->result_label), text ? text : "");
    gtk_widget_show(ui->result_box);
}

void ui_hide_result(AppUI *ui) {
    gtk_widget_hide(ui->result_box);
}
