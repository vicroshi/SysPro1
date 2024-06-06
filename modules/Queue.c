//
// Created by vic on 07/05/2024.
//
#include <string.h>
#include "Queue.h"
void init(Queue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

//didnt use it
int isEmpty(Queue* queue) {
    return (queue->size == 0);
}
//adds a job to the queue at the end
void enqueue(Queue* queue, QueueNode* node) {
    node->next = NULL;
    if (queue->head == NULL) {
        queue->head = node;
    } else {
        queue->tail->next = node;
    }
    queue->tail = node;
    node->idx = queue->size;
    queue->size++;
}
//unused function to return nth node
QueueNode* getNode(Queue* queue, int idx) {
    QueueNode* node = queue->head;
    while (idx>0 && node != NULL) {
        node = node->next;
        idx--;
    }
    return node;
}
//unused function
void removeNode(Queue* queue, int idx) {
    QueueNode* node = queue->head;
    QueueNode* prev = NULL;
    while (idx>0 && node != NULL) {
        prev = node;
        node = node->next;
        idx--;
    }
    if (node == NULL) {
        return;
    }
    if (prev == NULL) {
        queue->head = node->next;
    } else {
        prev->next = node->next;
    }
    if (node == queue->tail) {
        queue->tail = prev;
    }
    QueueNode* temp = node->next;
    while (temp != NULL) {
        temp->idx--;
        temp = temp->next;
    }
    queue->size--;
}
//this removes a job from the queue based on an integer key, whether it is the job id or the pid
Job* removeNodeId(Queue* queue, int id, const char* key) {
    QueueNode* node = queue->head;
    QueueNode* prev = NULL;
    Job* job;
    while (node != NULL) {
        job = getJob(node, Job, node);
        if (job->id == id && strcmp("jobid", key) == 0) {
            break;
        }
        else if(job->pid == id && strcmp("pid", key) == 0){
            break;
        }
        prev = node;
        node = node->next;
    }
    if (node == NULL) {
        return NULL;
    }
    if (prev == NULL) {
        queue->head = node->next;
    } else {
        prev->next = node->next;
    }
    if (node == queue->tail) {
        queue->tail = prev;
    }
    QueueNode* temp = node->next;
    while (temp != NULL) {
        temp->idx--;
        temp = temp->next;
    }
    queue->size--;
    return job;
}
//removes first node from queue
Job* dequeue(Queue* queue) {
    if (queue->head == NULL) {
        return NULL;
    }
    int old_idx = queue->head->idx;
    Job* job = getJob(queue->head, Job, node);
    queue->head = queue->head->next;
    if (queue->head != NULL){
        queue->head->idx = old_idx;
    }
    queue->size--;
    return job;
}

