//
// Created by sasha on 17.04.2024.
//

#ifndef IOS_MAIN_H
#define IOS_MAIN_H

#endif //IOS_MAIN_H

void busHandle(int CountOfPeople, sem_t **sem_stations, int countOfStations, int delay,int** peopleOnStations,int capacity);
void skierHandle(int order,int station, long waitingTime,sem_t **sem_stations,int* peopleOnStation,int capacity);
void printToFileBus(char* text);
void printToFileSkier(char* text,int order);