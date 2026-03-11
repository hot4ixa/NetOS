#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#define NUM_EAST 10
#define NUM_WEST 50
#define MAX_PATIENCE 20000
#define CROSS_TIME 100000
#define MAX_ON_ROPE 5
#define MAX_INTERVAL 50000
#define PORT 8888
#define BUFFER_SIZE 256

typedef enum { WEST = 0, EAST = 1, NONE = -1 } Side;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int west_on_rope = 0; // количество западных бабуинов на канате
int east_on_rope = 0; // количество восточных бабуинов на канате
bool rope_shaking = false;
Side shaking_side = NONE;

const char * side_name[2] = { "\033[31mзападный\033[0m", "\033[34mвосточный\033[0m" };

// Глобальный счётчик для уникальных идентификаторов бабуинов
int next_id = 0;
pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    int  patience;
    Side side;
    int  id;
    int  client_fd;
} Baboon;

bool can_cross( Baboon * baboon )
{
    if ( west_on_rope >= MAX_ON_ROPE || east_on_rope >= MAX_ON_ROPE ) return false;
    if ( baboon->side == WEST && east_on_rope > 0 ) return false;
    if ( baboon->side == EAST && west_on_rope > 0 ) return false;

    if ( rope_shaking && baboon->side != shaking_side )
        return false;

    return true;
}

void * baboon_thread( void * arg )
{
    Baboon * baboon = ( Baboon* )arg;
    int patience = baboon->patience;

    printf( "[ %d ] %s бабуин подошел\n", baboon->id, side_name[ baboon->side ] );
    
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    snprintf( buffer, BUFFER_SIZE, "[ %d ] %s бабуин подошел\n", baboon->id, side_name[ baboon->side ] );
    send(baboon->client_fd, buffer, strlen(buffer), 0);

    pthread_mutex_lock(&mutex);

    while ( !can_cross( baboon ) )
    {
        if ( patience <= 0 && !rope_shaking && ((baboon->side == WEST && east_on_rope > 0 ) || (baboon->side == EAST && west_on_rope > 0 )) )
        {
            printf("[ %d ] %s бабуин начал трясти канат!\n", baboon->id, side_name[ baboon->side ] );
            memset(buffer, 0, sizeof(buffer));
            
            snprintf( buffer, BUFFER_SIZE, "[ %d ] %s бабуин начал трясти канат!\n", baboon->id, side_name[ baboon->side ] );
            send(baboon->client_fd, buffer, strlen(buffer), 0);
            
            rope_shaking = true;
            shaking_side = baboon->side;
            pthread_cond_broadcast(&cond);
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100 * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        pthread_cond_timedwait(&cond, &mutex, &ts);
        patience -= 100;
    }

    if ( baboon->side == WEST ) west_on_rope++;
    else east_on_rope++;

    printf( "[ %d ] %s бабуин лезет по канату ( В = %d; З = %d ) [ %d %d ]\n",
            baboon->id, side_name[ baboon->side ], east_on_rope, west_on_rope, shaking_side, rope_shaking );
    memset(buffer, 0, sizeof(buffer));
    snprintf( buffer, BUFFER_SIZE, "[ %d ] %s бабуин лезет по канату ( В = %d; З = %d ) [ %d %d ]\n",
             baboon->id, side_name[ baboon->side ], east_on_rope, west_on_rope, shaking_side, rope_shaking );
    send(baboon->client_fd, buffer, strlen(buffer), 0);

    pthread_mutex_unlock(&mutex);

    usleep(CROSS_TIME);

    pthread_mutex_lock(&mutex);

    if ( baboon->side == WEST ) west_on_rope--;
    else east_on_rope--;

    if ( baboon->side == shaking_side )
        rope_shaking = false;

    printf( "[ %d ] %s бабуин перелез\n", baboon->id, side_name[ baboon->side ] );
    memset(buffer, 0, sizeof(buffer));
    snprintf( buffer, BUFFER_SIZE, "[ %d ] %s бабуин перелез\n", baboon->id, side_name[ baboon->side ] );
    send(baboon->client_fd, buffer, strlen(buffer), 0);


    pthread_cond_broadcast( &cond );
    pthread_mutex_unlock( &mutex );

    free( baboon );
    return NULL;
}

void * handle_client(void * arg)
{
    int client_fd = *(int*)arg;
    free(arg);

    while (1)
    {
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0)
        {
            close(client_fd);
            return NULL;
        }

        // Удаляем возможный символ новой строки
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';

        int count;
        char side_str[10];
        if (sscanf(buffer, "%d %s", &count, side_str) != 2 &&
            sscanf(buffer, "%s %d", side_str, &count) != 2)
        {
            fprintf(stderr, "Неверный формат запроса: %s\n", buffer);
            close(client_fd);
            return NULL;
        }

        Side side;
        if (strcmp(side_str, "west") == 0 || strcmp(side_str, "запад") == 0)
            side = WEST;
        else if (strcmp(side_str, "east") == 0 || strcmp(side_str, "восток") == 0)
            side = EAST;
        else
        {
            fprintf(stderr, "Неизвестная сторона: %s\n", side_str);
            close(client_fd);
            return NULL;
        }

        printf("Клиент запросил %d %s бабуинов\n", count, side_name[side]);
        

        // Создание бабуинов
        for (int i = 0; i < count; i++)
        {
            Baboon *baboon = malloc(sizeof(Baboon));
            if (!baboon)
            {
                perror("malloc");
                continue;
            }

            // Генерация уникального ID
            pthread_mutex_lock(&id_mutex);
            baboon->id = ++next_id;
            pthread_mutex_unlock(&id_mutex);

            baboon->side = side;
            baboon->patience = rand() % MAX_PATIENCE;

            baboon->client_fd = client_fd;

            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

            pthread_t thread;
            pthread_create(&thread, &attr, baboon_thread, baboon);
            pthread_attr_destroy(&attr);

            usleep(rand() % MAX_INTERVAL);
        }
    }

    close(client_fd);
    return NULL;
}

int main()
{
    setbuf(stdout, NULL);
    srand(time(NULL));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        exit(1);
    }

    printf("Сервер запущен на порту %d\n", PORT);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;
        pthread_t handler;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&handler, &attr, handle_client, fd_ptr);
        pthread_attr_destroy(&attr);
    }

    close(server_fd);
    return 0;
}
