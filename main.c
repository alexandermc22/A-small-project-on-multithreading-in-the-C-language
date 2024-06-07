#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "main.h"

typedef struct {
    int counter;
    int peopleInBus;
    sem_t mutex;
    sem_t sem_bus_is_open;
    sem_t sem_final;
    FILE *fp;
} SharedData;

SharedData *sharedData;

int main(int argc, char *argv[]) {

    // Argument parse
    if (argc != 6) {
        fprintf(stderr, "Usage: L Z K TL TB\n");
        return 1;
    }
    char *endptr;
    long L = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || L <= 0 || L >= 20000) {
        fprintf(stderr, "Invalid value for L\n");
        return 1;
    }

    long Z = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || Z <= 0 || Z > 10) {
        fprintf(stderr, "Invalid value for Z\n");
        return 1;
    }

    long K = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0' || K < 10 || K > 100) {
        fprintf(stderr, "Invalid value for K\n");
        return 1;
    }

    long TL = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0' || TL < 0 || TL > 10000) {
        fprintf(stderr, "Invalid value for TL\n");
        return 1;
    }

    long TB = strtol(argv[5], &endptr, 10);
    if (*endptr != '\0' || TB < 0 || TB > 1000) {
        fprintf(stderr, "Invalid value for TB\n");
        return 1;
    }

    int shm_fd; // Shared memory segment descriptor

    // Create shared memory segment
    shm_fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    ftruncate(shm_fd, sizeof(SharedData)); // Set segment size to SharedData struct size

    // Map shared memory segment into process memory
    sharedData = (SharedData *)mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Initialize semaphores
    sem_init(&sharedData->mutex, 1, 1); // 1 - shared between threads of the process
    sem_init(&sharedData->sem_bus_is_open, 1, 0);
    sem_init(&sharedData->sem_final, 1, 0);

    // Open file for writing
    sharedData->fp = fopen("proj2.out", "w");
    if (sharedData->fp == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Initialize counter
    sharedData->counter = 1;
    sharedData->peopleInBus = 0;

    // Array of integers for people on each station
    int *peopleOnStation[Z];
    const char *shm_name1 = "/peopleOnStation"; // Name for shared memory

    // Create and map shared memory
    int shm_fd1 = shm_open(shm_name1, O_CREAT | O_RDWR, 0666);
    if (shm_fd1 == -1) {
        perror("shm_open");
        return 1;
    }
    ftruncate(shm_fd1, Z * sizeof(int)); // Set memory size

    // Map memory into process address space
    int *SharedPeopleOnStation = mmap(NULL, Z * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd1, 0);
    if (SharedPeopleOnStation == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    for (int i = 0; i < Z; i++) {
        SharedPeopleOnStation[i] = 0;
        peopleOnStation[i] = &SharedPeopleOnStation[i];
    }

    // Array of semaphores
    sem_t *semaphores[Z];
    const char *shm_name = "/my_shared_memory"; // Name for shared memory

    // Create and map shared memory for semaphores
    int shm_fd2 = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd2 == -1) {
        perror("shm_open");
        return 1;
    }
    ftruncate(shm_fd2, Z * sizeof(sem_t)); // Set memory size

    // Map memory into process address space
    sem_t *shared_semaphores = mmap(NULL, Z * sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0);
    if (shared_semaphores == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Initialize semaphores in shared memory
    for (int i = 0; i < Z; i++) {
        semaphores[i] = &(shared_semaphores[i]);
        sem_init(semaphores[i], 1, 0); // Initialize semaphore
    }

    int status;
    pid_t pid;
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork failed\n");
        return 1;
    } else if (pid == 0) {
        // Child process
        busHandle(L,semaphores,Z,TB,peopleOnStation,K);
        exit(0);
    } else {
        // Parent process
        for (int i = 1; i <= L; i++) {
            pid_t child_pid = fork();
            if (child_pid < 0) {
                fprintf(stderr, "Fork failed\n");
                return 1;
            } else if (child_pid == 0) {
                srand(time(NULL) ^ getpid());
                // Child process (skier)
                int station = rand() % Z + 1;

                skierHandle(i,station,TL,semaphores,peopleOnStation[station-1],K);
                exit(0);  // Important to exit child process so it doesn't continue the loop
            }
        }

        // Wait for all child processes to finish
        for (int i = 0; i < L+1; i++) {
            waitpid(-1, &status, 0);
        }
    }

    // clean memory
    sem_destroy(&sharedData->mutex);
    sem_destroy(&sharedData->sem_bus_is_open);
    sem_destroy(&sharedData->sem_final); 
    fclose(sharedData->fp);
    munmap(sharedData, sizeof(SharedData));
    close(shm_fd);
    shm_unlink("/myshm");

    for (int i = 0; i < Z; i++) {
        sem_destroy(semaphores[i]); 
    }
    munmap(semaphores, sizeof(semaphores));
    close(shm_fd2);

    munmap(peopleOnStation, sizeof(peopleOnStation));
    close(shm_fd1);

    return 0;
}

void busHandle(int countOfPeople, sem_t **sem_stations, int countOfStations, int delay,int** peopleOnStations,int capacity) 
{
    printToFileBus("started");
    srand(time(NULL) ^ getpid());
    
    
    while(countOfPeople!=0)
    {
        for (int i = 0; i < countOfStations; i++)
        {
            usleep((rand() % delay));
            char text[50];
            snprintf(text, sizeof(text), "arrived to %d", i+1);
            printToFileBus(&text[0]);

            // open semaphores for skiers
            sem_post(sem_stations[i]);
            sem_post(&sharedData->sem_bus_is_open); 
            // waiting when all skiers are been boarding
            while(!( *(peopleOnStations[i])== 0 || capacity == sharedData->peopleInBus))
            {
                usleep(10);
            }
            // close semaphores
            sem_wait(&sharedData->sem_bus_is_open);
            sem_wait(sem_stations[i]);
            snprintf(text, sizeof(text), "leaving %d", i+1);
            printToFileBus(&text[0]);
        }
        // minimize count of rest people
        countOfPeople-=sharedData->peopleInBus;
        printToFileBus("arrived to final");
        //open exit semaphore
        sem_post(&sharedData->sem_final);
        //wait when all skiers exit bus
        while (sharedData->peopleInBus!=0)
        {
            usleep(10);
        }
        sem_wait(&sharedData->sem_final);
        printToFileBus("leaving final");
    }
    printToFileBus("finish");
    return;
}

void skierHandle(int order,int station, long waitingTime,sem_t **sem_stations, int* peopleOnStation,int capacity) {
    printToFileSkier("started",order);
    srand(time(NULL) ^ getpid());
 
    usleep((rand() % waitingTime));
    char text[50];
    snprintf(text, sizeof(text), "arrived to %d", station);
    printToFileSkier(&text[0],order);
    (*peopleOnStation)++;

    while(true)
    {
        // wait when bus arrive to stantion
        sem_wait(sem_stations[station-1]);
        sem_post(sem_stations[station-1]);

        // close semaphore to board on bus
        sem_wait(&sharedData->sem_bus_is_open);

        // if we have space in bus
        if(sharedData->peopleInBus < capacity)   
        {
        printToFileSkier("boarding",order);
        (*peopleOnStation)--;
        sharedData->peopleInBus++;

        sem_post(&sharedData->sem_bus_is_open);

        sem_wait(&sharedData->sem_final);
        sem_post(&sharedData->sem_final);
        printToFileSkier("going to ski",order);
        sharedData->peopleInBus--;
        return;
        }
        else
        {
            sem_post(&sharedData->sem_bus_is_open);
        }
    }
}

void printToFileBus(char* text) {
    sem_wait(&sharedData->mutex); // Wait for semaphore access
    fprintf(sharedData->fp, "%d: BUS: %s\n", sharedData->counter, text);
    fflush(sharedData->fp);
    sharedData->counter++;
    sem_post(&sharedData->mutex); // Release semaphore
}

void printToFileSkier(char* text,int order) {
    sem_wait(&sharedData->mutex); // Wait for semaphore access
    fprintf(sharedData->fp, "%d: L %d: %s\n", sharedData->counter,order, text);
    fflush(sharedData->fp);
    sharedData->counter++;
    sem_post(&sharedData->mutex); // Release semaphore
}
