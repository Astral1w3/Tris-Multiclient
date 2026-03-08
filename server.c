#include "game.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 100
#define DEFAULT_PORT 8888
#define BUFFER_SIZE 1024
#define TIMEOUT_SECONDS 300

static int server_fd = -1;
static volatile sig_atomic_t running = 1;

/**
 * @brief Gestore dei segnali SIGINT/SIGTERM: arresta il loop principale e chiude il socket server.
 * @param sig Numero del segnale ricevuto.
 */
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        if (server_fd >= 0) {
            close(server_fd);
            server_fd = -1;
        }
    }
}

/**
 * @brief Legge la porta dalla riga di comando; se assente o invalida, usa DEFAULT_PORT.
 * @param argc Numero di argomenti.
 * @param argv Array degli argomenti (argv[1] = porta).
 * @return Numero di porta valido (1-65535) oppure DEFAULT_PORT.
 */
static int parse_port(int argc, char* argv[]) {
    if (argc < 2) {
        return DEFAULT_PORT;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        return DEFAULT_PORT;
    }
    return port;
}

/**
 * @brief Dispatcher dei comandi: effettua il parsing e smista al gestore appropriato.
 * @param player Puntatore al giocatore che ha inviato il comando.
 * @param line   Riga di testo contenente il comando ricevuto.
 * @return 0 per continuare, -1 se il client ha chiesto QUIT.
 */
static int process_client_command(Player* player, const char* line) {
    char cmd[64] = {0};
    char args[512] = {0};

    if (!player || !line) {
        return 0;
    }

    if (parse_command(line, cmd, args, sizeof(args)) != 0) {
        return 0;
    }

    if (strcmp(cmd, CMD_CREATE_GAME) == 0) {
        handle_create_game(player, args);
    } else if (strcmp(cmd, CMD_LIST_GAMES) == 0) {
        handle_list_games(player);
    } else if (strcmp(cmd, CMD_JOIN_GAME) == 0) {
        handle_join_game(player, args);
    } else if (strcmp(cmd, CMD_ANSWER_JOIN) == 0) {
        handle_answer_join(player, args);
    } else if (strcmp(cmd, CMD_MOVE) == 0) {
        handle_move(player, args);
    } else if (strcmp(cmd, CMD_LEAVE_GAME) == 0) {
        handle_leave_game(player);
    } else if (strcmp(cmd, CMD_SET_NICKNAME) == 0) {
        handle_set_nickname(player, args);
    } else if (strcmp(cmd, CMD_QUIT) == 0) {
        handle_quit(player);
        return -1;
    } else {
        send_error(player->socket_fd, "Comando sconosciuto");
    }
    return 0;
}

/**
 * @brief Processa lo stream di byte ricevuto dal socket, estraendo righe complete.
 *        Accumula caratteri nel buffer di riga e invoca process_client_command ad ogni newline.
 * @param player    Puntatore al giocatore mittente.
 * @param chunk     Buffer di byte appena letti dal socket.
 * @param chunk_len Numero di byte ricevuti.
 * @param line_buf  Buffer di accumulo per la riga corrente.
 * @param line_len  Puntatore alla lunghezza corrente della riga accumulata.
 * @return 0 per continuare, -1 se il client ha fatto QUIT.
 */
static int process_client_stream(Player* player, const char* chunk, size_t chunk_len, char* line_buf, size_t* line_len) {
    for (size_t i = 0; i < chunk_len; ++i) {
        char c = chunk[i];
        if (c == '\n' || c == '\r') {
            if (*line_len > 0) {
                line_buf[*line_len] = '\0';
                if (process_client_command(player, line_buf) < 0) {
                    return -1;
                }
                *line_len = 0;
            }
            continue;
        }

        if (*line_len < BUFFER_SIZE - 1) {
            line_buf[(*line_len)++] = c;
        }
    }

    return 0;
}

/**
 * @brief Controlla tutte le partite attive e termina quelle con timeout scaduto.
 *        Il giocatore che non ha mosso entro TIMEOUT_SECONDS perde la partita.
 */
static void check_game_timeouts(void) {
    while (running) {
        time_t now = time(NULL);
        Game* expired_game = NULL;
        GameResult owner_result = RESULT_LOSS;
        GameResult guest_result = RESULT_LOSS;
        char win_symbol = 'X';

        pthread_mutex_lock(&games_mutex);
        Game* current = games_list;
        while (current) {
            if (current->state == STATE_PLAYING && now - current->last_move_time > TIMEOUT_SECONDS) {
                current->state = STATE_TERMINATED;
                expired_game = current;
                break;
            }
            current = current->next;
        }
        pthread_mutex_unlock(&games_mutex);

        if (!expired_game) {
            break;
        }

        pthread_mutex_lock(&expired_game->mutex);
        win_symbol = (expired_game->current_turn == 'X') ? 'O' : 'X';
        pthread_mutex_unlock(&expired_game->mutex);

        owner_result = (win_symbol == 'X') ? RESULT_WIN : RESULT_LOSS;
        guest_result = (win_symbol == 'O') ? RESULT_WIN : RESULT_LOSS;
        terminate_game(expired_game, owner_result, guest_result);
    }
}

/**
 * @brief Thread handler per ogni client: legge comandi via select() con timeout di 1s.
 *        Crea il Player, invia il welcome e processa lo stream fino a disconnessione.
 * @param arg Puntatore a int contenente il file descriptor del client (allocato in heap).
 * @return NULL sempre.
 */
static void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char read_buffer[BUFFER_SIZE];
    char line_buffer[BUFFER_SIZE];
    size_t line_len = 0;
    char nickname[32];

    snprintf(nickname, sizeof(nickname), "Player_%d", client_fd);
    Player* player = player_create(client_fd, nickname);
    if (!player) {
        close(client_fd);
        return NULL;
    }

    players_list_add(player);
    send_welcome(client_fd);

    while (running && player->socket_fd >= 0) {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (activity > 0 && FD_ISSET(client_fd, &read_fds)) {
            ssize_t bytes_read = read(client_fd, read_buffer, sizeof(read_buffer));
            if (bytes_read <= 0) {
                break;
            }

            if (process_client_stream(
                    player,
                    read_buffer,
                    (size_t)bytes_read,
                    line_buffer,
                    &line_len) < 0) {
                return NULL;
            }
        }

        check_game_timeouts();
    }

    if (player->socket_fd >= 0) {
        handle_quit(player);
    }
    return NULL;
}

/**
 * @brief Entry point del server: configura socket TCP, accetta connessioni e avvia thread.
 *        Supporta porta da riga di comando (default 8888), gestisce SIGINT/SIGTERM.
 * @param argc Numero di argomenti.
 * @param argv Argomenti: argv[1] = porta (opzionale).
 * @return 0 se terminato correttamente, 1 in caso di errore.
 */
int main(int argc, char* argv[]) {
    int port = parse_port(argc, argv);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server Tris attivo su porta %d\n", port);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running) {
                perror("accept");
            }
            continue;
        }

        printf("Connessione da %s\n", inet_ntoa(client_addr.sin_addr));

        int* thread_arg = malloc(sizeof(int));
        if (!thread_arg) {
            close(client_fd);
            continue;
        }

        *thread_arg = client_fd;
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, thread_arg) != 0) {
            close(client_fd);
            free(thread_arg);
            continue;
        }
        pthread_detach(client_thread);
    }

    games_list_cleanup();
    players_list_cleanup();
    if (server_fd >= 0) {
        close(server_fd);
    }
    return 0;
}
