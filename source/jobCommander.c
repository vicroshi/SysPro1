//
// Created by vic on 07/05/2024.
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "protocol.h"
#define SERVER_FIFO "/tmp/sdi1900096_server" //server's request fifo
#define jobServer "./jobExecutorServer" //server executable
#define PIPE_PREFIX_LEN 23 //strlen("sdi1900096clientr_")


char* join_strings(char**, const char*, int);

int main(int argc, char* argv[]) {
    request_header h;
    h.pid = getpid();
    h.arg_count = 0;
    char* command;
    //arg check
    if(strcmp(argv[1],"issueJob")==0){
        h.command_num = 1;
        command = join_strings(argv+2," ",argc-2);
        h.message = strlen(command);
        h.arg_count = argc-2;
    }
    else if(strcmp(argv[1],"setConcurrency")==0){
        h.command_num = 2;
        h.message = atoi(argv[2]);
    }
    else if(strcmp(argv[1],"stop")==0){
        h.command_num = 3;
        sscanf(argv[2],"job_%d",&h.message); //extract numeric value
    }
    else if(strcmp(argv[1],"poll")==0){
        h.command_num = 4;
        if(strcmp(argv[2],"running")==0){
            h.message = 1;
        }
        else if(strcmp(argv[2],"queued")==0){
            h.message = 2;
        }
        else{
            exit(EXIT_FAILURE);
        }
    }
    else if(strcmp(argv[1],"exit")==0){
        h.command_num = 5;
        h.message = 0;
    }
    else{
        exit(EXIT_FAILURE);
    }
    //check if server is activated
    if (access("jobExecutorServer.txt", F_OK) == -1) {
        pid_t pid = fork();
        if(pid==-1){
            perror("fork:");
            exit(EXIT_FAILURE);
        }
        if(pid==0){
            execl(jobServer,jobServer,NULL);
            perror("execl failed to start the server:");
            exit(EXIT_FAILURE);
        }
        //wait for IPC to be initialized
        waitpid(-1,NULL,0);
    }
    int fd_sw; //fd for writing to server
    if((fd_sw = open(SERVER_FIFO,O_WRONLY))==-1){
        perror("open server fifo:");
        exit(EXIT_FAILURE);
    }
    int nbytes_pid = snprintf(NULL, 0, "%d", h.pid);
    char client_rfifo[PIPE_PREFIX_LEN+nbytes_pid+1];
    snprintf(client_rfifo,PIPE_PREFIX_LEN+nbytes_pid+1,"/tmp/sdi1900096clientr_%d",h.pid); //construct the name for clients reading fifo
    mkfifo(client_rfifo,0666);
    char* client_wfifo=NULL;
    if(h.command_num==1){ //if the command is issueJob, we need to write it to a different pipe, so create it
        client_wfifo = calloc(strlen(client_rfifo)+1,sizeof(char));
        strcpy(client_wfifo,client_rfifo);
        client_wfifo[21] = 'w'; //hardcoded :P
        mkfifo(client_wfifo,0666);
    }
    int nbytes = write(fd_sw,&h,sizeof(request_header));
    if(nbytes==-1){
        perror("write header:"); //no handling at all
    }
    int fd_r = open(client_rfifo,O_RDONLY); //open reading pipe
    if(fd_r==-1){
        perror("open read fifo:");
        exit(EXIT_FAILURE);
    }
    if(h.command_num==1){
        int fd_w = open(client_wfifo,O_WRONLY);
        if(fd_w==-1){
            perror("open write fifo:");
            exit(EXIT_FAILURE);
        }
        write(fd_w,command,strlen(command)+1);
        close(fd_w);
        free(command);
        unlink(client_wfifo);
        free(client_wfifo);
    }

    int len = 0;
    response_header rh;
    read(fd_r,&rh,sizeof(response_header));
    char* buffer;
    //resolve the response
    if(rh.id==-1){ //no ID case, so we are not getting a response to issueJob
        if(rh.int1==-1 && rh.len==-1){ //no queuePos/integerParameter or message length, so we must not print anything, just exit
            close(fd_r);
            close(fd_sw);
            if(unlink(client_rfifo)==-1){
                perror("unlink:");
                exit(EXIT_FAILURE);
            }
            return 0;
        }
        else if(rh.int1==-1){ //no queuePos or integerParameter, just read the message and print it
            len = rh.len;
            buffer = calloc(len,sizeof(char));
            read(fd_r,buffer,len);
            if (len == 1){
                printf("%s not found\n",argv[2]);
            }
            else{
                printf("%s\n",buffer);
            }
        }
        else{ // we must resolve the response to poll and print it
            len = rh.len;
            int njobs = rh.int1;
            jobTwople jt[njobs]; //this holds the ids and queuePos for all jobs
            buffer = malloc(len);
            read(fd_r,jt,njobs*sizeof(jobTwople)); //read them first
            read(fd_r,buffer,len); //read a continuous buffer of job descriptions separated by '\0'
            char* desc = buffer;
            if(h.message==1){ //this is for better reading
                printf("Running jobs:\n");
            }
            else{
                printf("Queued jobs:\n");
            }
            for(int i=0;i<njobs;i++){ //print correctly
                printf("<job_%d, %s, %d>\n",jt[i].job_id,desc,jt[i].queue_pos);
                desc = strchr(desc,'\0')+1; // or desc += strlen(desc)+1;
            }
        }
    }
    else{ //everything is in the request so just print the correct format
        buffer = malloc(rh.len); //job description allocation
        read(fd_r,buffer,rh.len); //read job description in buffer
        printf("<job_%d, %s, %d>\n",rh.id,buffer,rh.int1);
    }
    free(buffer);
    close(fd_r);
    close(fd_sw);
    if(unlink(client_rfifo)==-1){
        perror("unlink:");
        exit(EXIT_FAILURE);
    }
    return 0;
}


//literally made some last minute changes hoe it doesn't break anything :DDDDDDDDDD
char* join_strings(char** strings, const char* separator, int argc) { //used to join argv
    // calculate the total length of the resulting string
    size_t total_length = 0;
    for (int i = 0; i < argc; i++) {
        if (strchr(strings[i], ' ') != NULL) {
            total_length += 2; //for quotes
        }
        total_length += strlen(strings[i]) + 1;
    }
    char* result = malloc(total_length + 1);
    if (result == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    // copy the strings into the resulting string
    char* current_position = result;
    for (int i = 0; i < argc; i++) {
        if (strchr(strings[i], ' ') != NULL) { //if there is space in the string, add quotes
            strcpy(current_position, "\"");
            current_position++;
            strcpy(current_position, strings[i]);
            current_position += strlen(strings[i]);
            strcpy(current_position, "\"");
            current_position++;
        }
        else{ //else just copy
            strcpy(current_position, strings[i]);
            current_position += strlen(strings[i]);
        }
        //join them with separator
        strcpy(current_position, separator);
        current_position++;
    }
    // replace the last separator with a null terminator
    current_position--;
    *current_position = '\0';
    return result;
}