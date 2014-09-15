#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <unistd.h>


//==============================================================================
//  Hello world thread.
//==============================================================================
void* say_hello(void* _id) {
    long id = (long)_id;
    printf("Hello from thread %ld\n", id);
    pthread_exit(NULL);
}

//==============================================================================
//  Dining philosophers problem.
//==============================================================================

static const unsigned long kNumPhilosophers = 5;

pthread_mutex_t chopsticks[kNumPhilosophers];

enum PhilosopherState {
    PhilosopherState_hungry,
    PhilosopherState_thinking,
    PhilosopherState_count,
};

struct PhilosopherArg {
    PhilosopherState state;
    unsigned long id;
};

void philosophers_init() {
    for (int i = 0; i < kNumPhilosophers; ++i) {
        pthread_mutex_init(&chopsticks[i], NULL);
    }
}

void* philosopher(void* _arg) {
    PhilosopherArg arg = *(PhilosopherArg*) _arg;
    PhilosopherState state = arg.state;
    unsigned long i = arg.id;
    static int max = 3;
    for(int c = 0; c < max;++c) {
        switch(state) {
        case PhilosopherState_hungry:
            unsigned long a,b,n, m;
            n = i;
            m = (i + 1) % kNumPhilosophers;
            {
                a = fmin(n,m);
                b = fmax(n,m);
                printf("Philosopher %ld taking %u, %u\n", i, a, b);
                pthread_mutex_lock(&chopsticks[a]);
                pthread_mutex_lock(&chopsticks[b]);
                usleep(100 * 1000); // eat time
                pthread_mutex_unlock(&chopsticks[a]);
                pthread_mutex_unlock(&chopsticks[b]);
                printf("Philosopher %ld is very satisfied!\n", i);
                state = PhilosopherState_thinking;
            }

            break;
        case PhilosopherState_thinking:
            usleep(100 * 1000);  // sleep
            state = PhilosopherState_hungry;
            printf("Philosopher %ld is now hungry!\n", i);
            break;
        default:
            break;
        }
    }
    printf("Philosopher %ld is now done.\n", i);
    pthread_exit(NULL);
}

//==============================================================================
// Peterson's algorithm
//==============================================================================

struct Peterson {
    int turn;
    int flags[2];
};

static Peterson peterson;

const static int kPetersonMax = 12500;

pthread_mutex_t stupid_lock = PTHREAD_MUTEX_INITIALIZER;

int sum = 0;

void critical() {
    // Make a very non-atomic increment
    int tmp = sum;
    tmp = tmp + 1;
    sum = tmp;
}

#define IGNORE_PETERSON 1  // Sum will be incorrect if set to 0 (non atomic increments)

void* peterson_0(void*) {
    int id = 0;
    for (int i = 0; i < kPetersonMax; ++i) {
#if IGNORE_PETERSON
        pthread_mutex_lock(&stupid_lock);
        peterson.flags[id] = true;  // I want to go.
        peterson.turn = 1;          // ... but you can go first.
        pthread_mutex_unlock(&stupid_lock);

        while (peterson.flags[1] && peterson.turn == 0) {}  // Busy wait
#endif

        critical();

#if IGNORE_PETERSON
        pthread_mutex_lock(&stupid_lock);
        peterson.flags[id] = false;  // I'm done!
        pthread_mutex_unlock(&stupid_lock);
#endif
    }
    pthread_exit(NULL);
}

void* peterson_1(void*) {
    int id = 1;
    for (int i = 0; i < kPetersonMax; ++i) {
#if IGNORE_PETERSON
        pthread_mutex_lock(&stupid_lock);
        peterson.flags[id] = true;  // I want to go.
        peterson.turn = 0;          // ... but you can go first.
        pthread_mutex_unlock(&stupid_lock);

        while (peterson.flags[0] && peterson.turn == 1) {}  // Busy wait
#endif

        critical();

#if IGNORE_PETERSON
        pthread_mutex_lock(&stupid_lock);
        peterson.flags[id] = false;  // I'm done!
        pthread_mutex_unlock(&stupid_lock);
#endif
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    pthread_t threads[43];
    int err;

    // Run hello.
    for (long id = 0; id < 3; ++id) {
        err = pthread_create(&threads[id], NULL, say_hello, (void*)id);
        if (err) {
            puts("error creating thread.");
            exit(EXIT_FAILURE);
        }
    }

    // Philosophers
    philosophers_init();
    for (unsigned long i = 0; i < kNumPhilosophers; ++i) {
        PhilosopherArg* arg;
        arg = (PhilosopherArg*)malloc(sizeof(PhilosopherArg));
        arg->state = PhilosopherState_hungry;
        arg->id = i;
        err = pthread_create(&threads[i], NULL, philosopher, (void*)arg);
        if (err) {
            puts("error creating thread.");
            exit(EXIT_FAILURE);
        }
    }

    // Test peterson
    pthread_t peterson_threads[2];
    pthread_create(&peterson_threads[0], NULL, peterson_0, NULL);
    pthread_create(&peterson_threads[1], NULL, peterson_1, NULL);

    usleep (4 * 1000 * 1000);  // This is extra stupid.

    printf("Sum is %d and should be %d\n", sum, 2 * kPetersonMax);
    pthread_exit(NULL);
}
