#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 256

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Использование: %s <количество> <сторона (west/east)>\n", argv[0]);
        return 1;
    }

    int count = atoi(argv[1]);
    char *side = argv[2];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = inet_addr("127.0.0.1")
    };

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%d %s\n", count, side);

    if (send(sock, buffer, strlen(buffer), 0) < 0)
    {
        perror("send");
        close(sock);
        return 1;
    }

    close(sock);
    printf("Запрос отправлен: %s", buffer);
    return 0;
}
