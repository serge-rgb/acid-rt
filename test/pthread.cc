#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <unistd.h>

void* say_hello(void* _id) {
    long id = (long)_id;
    printf("Hello from thread %ld\n", id);
    pthread_exit(NULL);
}
int main(int argc, char *argv[]) {
    pthread_t threads[43];
    int err;
    long id = 42;
    for (long id = 0; id < 43; ++id) {
        err = pthread_create(&threads[id], NULL, say_hello, (void*)id);
        if (err) {
            puts("error creating thread.");
            exit(EXIT_FAILURE);
        }
    }
    pthread_exit(NULL);
}
