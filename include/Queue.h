//
// Created by vic on 07/05/2024.
//
#define _GNU_SOURCE
#ifndef SYSPRO1_QUEUE_H
#define SYSPRO1_QUEUE_H
#include <stdio.h>
#include <stdalign.h>
#include <sys/types.h>
#include <stddef.h>
typedef struct Queue Queue;
typedef struct QueueNode QueueNode;
typedef struct Job Job;
struct QueueNode {
    int idx;
    QueueNode* next;
};

struct Queue {
    QueueNode* head;
    QueueNode* tail;
    int size;
};
struct Job {
    int id;
    pid_t pid;
    char* description;
    int description_argc;
    int queuePosition;
    QueueNode node;
};
void init(Queue*);
void enqueue(Queue*, QueueNode*);
Job* dequeue(Queue*);
int isEmpty(Queue*);
QueueNode* getNode(Queue*, int);
void removeNode(Queue*, int);
Job* removeNodeId(Queue*, int, const char*);

#define getJob(ptr, type,member) ((type*)((char*)(ptr)-offsetof(type, member))) //container_of implementation


#endif //SYSPRO1_QUEUE_H
