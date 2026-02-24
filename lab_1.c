#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <semaphore.h>

#define NUM_EAST 10
#define NUM_WEST 50
#define MAX_PATIENCE 200
#define CROSS_TIME 1000
#define MAX_ON_ROPE 5
#define MAX_INTERVAL 500

typedef enum { WEST = 0, EAST = 1, NONE = -1 } Side;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int west_on_rope = 0; // количество западных бабуинов на канате
int east_on_rope = 0; // количество восточных бабуинов на канате
bool rope_shaking = false;
Side shaking_side = NONE;

const char * side_name[2] = { "\033[31mзападный\033[0m", "\033[34mвосточный\033[0m" };

typedef struct 
{
    int patience;
    Side         side;
    int          id;
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

    pthread_mutex_lock(&mutex);
    
    while ( !can_cross( baboon ) )
    {
        if ( patience <= 0 && !rope_shaking && ((baboon->side == WEST && east_on_rope > 0 ) || (baboon->side == EAST && west_on_rope > 0 )) )
        {
            printf("[ %d ] %s бабуин начал трясти канат!\n", baboon->id, side_name[ baboon->side ] );
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

    pthread_mutex_unlock(&mutex);

    usleep(CROSS_TIME);
    
    pthread_mutex_lock(&mutex);
    
    if ( baboon->side == WEST ) west_on_rope--;
    else east_on_rope--;
    
    if ( baboon->side == shaking_side )
        rope_shaking = false;

    printf("[ %d ] %s бабуин перелез\n", baboon->id, side_name[ baboon->side ] );
    
    pthread_cond_broadcast( &cond );
    pthread_mutex_unlock( &mutex );
    
    free( baboon );
}

void * spawnWest()
{
    pthread_t threads[ NUM_WEST ];
    for ( int i = 0; i < NUM_WEST; i++ )
    {
        Baboon * baboon = malloc( sizeof( Baboon ) );

        baboon->id = i + 1;
        baboon->side = WEST;
        baboon->patience = rand() % MAX_PATIENCE;

        pthread_create( &threads[ i ], NULL, baboon_thread, ( void* )baboon );
        usleep( rand() % MAX_INTERVAL );
    }

    for ( int i = 0; i < NUM_WEST; i++ )
        pthread_join( threads[ i ], NULL );
    
    return NULL;
}

void * spawnEast()
{
    pthread_t threads[ NUM_EAST ];
    for ( int i = 0; i < NUM_EAST; i++ )
    {
        Baboon * baboon = malloc( sizeof( Baboon ) );
        
        baboon->id = i + 1;
        baboon->side = EAST;
        baboon->patience = rand() % MAX_PATIENCE;
        
        pthread_create( &threads[ i ], NULL, baboon_thread, ( void* )baboon );
        usleep( rand() % MAX_INTERVAL );
    }
    
    for ( int i = 0; i < NUM_EAST; i++ )
        pthread_join( threads[ i ], NULL );
    
    return NULL;
}

int main()
{
    setbuf(stdout, NULL);
    srand( time( NULL ) );
    
    pthread_t west;
    pthread_t east;
    pthread_create( &west, NULL, spawnWest, NULL );
    pthread_create( &east, NULL, spawnEast, NULL );
    
    pthread_join( west, NULL );
    pthread_join( east, NULL );
    
    printf ("Концерт вещленче!\n");

    return 0;
}