#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 8888

static void print_help(void) {
    printf("\n=== COMANDI ===\n");
    printf("SET_NICKNAME <nome>\n");
    printf("CREATE_GAME\n");
    printf("LIST_GAMES\n");
    printf("JOIN_GAME <id>\n");
    printf("ANSWER_JOIN <yes/no> <id> <nick>\n");
    printf("MOVE <riga> <col>\n");
    printf("LEAVE_GAME\n");
    printf("QUIT\n\n");
}

static int connect_to_server(const char* host, int port) {
    struct sockaddr_in addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

static void trim_newline(char* text) {
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

int main(int argc, char* argv[]) {
    const char* host = (argc > 1) ? argv[1] : DEFAULT_HOST;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) {
        port = DEFAULT_PORT;
    }

    int sock = connect_to_server(host, port);
    if (sock < 0) {
        perror("Connessione fallita");
        return 1;
    }

    printf("Connesso a %s:%d. Digita HELP per i comandi.\n", host, port);

    fd_set fds;
    char buffer[BUFFER_SIZE];

    while (1) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(sock, &fds);

        if (select(sock + 1, &fds, NULL, NULL, NULL) < 0) {
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            if (!fgets(buffer, sizeof(buffer), stdin)) {
                break;
            }

            char command[BUFFER_SIZE];
            strncpy(command, buffer, sizeof(command) - 1);
            command[sizeof(command) - 1] = '\0';
            trim_newline(command);

            if (strcmp(command, "HELP") == 0) {
                print_help();
                continue;
            }

            if (strcmp(command, "QUIT") == 0) {
                write(sock, "QUIT\n", 5);
                break;
            }

            write(sock, buffer, strlen(buffer));
        }

        if (FD_ISSET(sock, &fds)) {
            ssize_t n = read(sock, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                break;
            }
            buffer[n] = '\0';
            printf("%s", buffer);
        }
    }

    close(sock);
    return 0;
}
