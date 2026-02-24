#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define NUM_EAST 10
#define NUM_WEST 50
#define MAX_PATIENCE 200
#define CROSS_TIME 1000
#define MAX_ON_ROPE 5
#define MAX_INTERVAL 500

typedef enum { WEST = 0, EAST = 1, NONE = -1 } Side;

const char * side_name[2] = { "\033[31mзападный\033[0m", "\033[34mвосточный\033[0m" };

typedef struct 
{
    int patience;
    Side side;
    int id;
} Baboon;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int west_on_rope;
    int east_on_rope;
    bool rope_shaking;
    Side shaking_side;
} SharedState;

SharedState* shared_state = NULL;

bool can_cross(Baboon* baboon)
{
    if (shared_state->west_on_rope >= MAX_ON_ROPE || shared_state->east_on_rope >= MAX_ON_ROPE) return false;
    if (baboon->side == WEST && shared_state->east_on_rope > 0) return false;
    if (baboon->side == EAST && shared_state->west_on_rope > 0) return false;
    
    if (shared_state->rope_shaking && baboon->side != shared_state->shaking_side)
        return false;
    
    return true;
}

void baboon_work(Baboon* baboon)
{
    int patience = baboon->patience;

    printf("[ %d ] %s бабуин подошел\n", baboon->id, side_name[baboon->side]);

    pthread_mutex_lock(&shared_state->mutex);
    
    while (!can_cross(baboon))
    {
        if (patience <= 0 && !shared_state->rope_shaking && 
           ((baboon->side == WEST && shared_state->east_on_rope > 0) || 
            (baboon->side == EAST && shared_state->west_on_rope > 0)))
        {
            printf("[ %d ] %s бабуин начал трясти канат!\n", baboon->id, side_name[baboon->side]);
            shared_state->rope_shaking = true;
            shared_state->shaking_side = baboon->side;
            pthread_cond_broadcast(&shared_state->cond);
        }
        
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100 * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        pthread_cond_timedwait(&shared_state->cond, &shared_state->mutex, &ts);
        patience -= 100;
    }

    if (baboon->side == WEST) shared_state->west_on_rope++;
    else shared_state->east_on_rope++;

    printf("[ %d ] %s бабуин лезет по канату ( В = %d; З = %d ) [ %d %d ]\n", 
            baboon->id, side_name[baboon->side], 
            shared_state->east_on_rope, shared_state->west_on_rope, 
            shared_state->shaking_side, shared_state->rope_shaking);

    pthread_mutex_unlock(&shared_state->mutex);

    usleep(CROSS_TIME);
    
    pthread_mutex_lock(&shared_state->mutex);
    
    if (baboon->side == WEST) shared_state->west_on_rope--;
    else shared_state->east_on_rope--;
    
    if (baboon->side == shared_state->shaking_side)
        shared_state->rope_shaking = false;

    printf("[ %d ] %s бабуин перелез\n", baboon->id, side_name[baboon->side]);
    
    pthread_cond_broadcast(&shared_state->cond);
    pthread_mutex_unlock(&shared_state->mutex);
}

void spawn_baboons(int count, Side side)
{
    for (int i = 0; i < count; i++)
    {
        pid_t pid = fork();
        
        if (pid == 0)
        {
            Baboon baboon;
            baboon.id = i + 1;
            baboon.side = side;
            baboon.patience = rand() % MAX_PATIENCE;
            
            srand(time(NULL) ^ getpid() ^ (i << 16));
            
            baboon_work(&baboon);
            
            exit(0);
        }
        usleep(rand() % MAX_INTERVAL);
    }
    
    for (int i = 0; i < count; i++) {
        wait(NULL);
    }
}

SharedState* init_shared_state()
{
    SharedState* state = mmap(NULL, sizeof(SharedState),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if (state == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&state->mutex, &mattr);
    pthread_mutexattr_destroy(&mattr);
    
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&state->cond, &cattr);
    pthread_condattr_destroy(&cattr);
    
    state->west_on_rope = 0;
    state->east_on_rope = 0;
    state->rope_shaking = false;
    state->shaking_side = NONE;
    
    return state;
}

void cleanup_shared_state(SharedState* state)
{
    pthread_mutex_destroy(&state->mutex);
    pthread_cond_destroy(&state->cond);
    munmap(state, sizeof(SharedState));
}

int main()
{
    setbuf(stdout, NULL);
    
    shared_state = init_shared_state();
    
    srand(time(NULL));
    
    pid_t west_pid = fork();
    if (west_pid == 0) {
        spawn_baboons(NUM_WEST, WEST);
        exit(0);
    }
    
    pid_t east_pid = fork();
    if (east_pid == 0) {
        spawn_baboons(NUM_EAST, EAST);
        exit(0);
    }
    
    waitpid(west_pid, NULL, 0);
    waitpid(east_pid, NULL, 0);
    
    printf("Концерт вещленче!\n");
    
    cleanup_shared_state(shared_state);
    return 0;
}
