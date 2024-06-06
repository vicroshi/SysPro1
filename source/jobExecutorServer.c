//
// Created by vic on 07/05/2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include "Queue.h"
#include "protocol.h"
#include <sys/poll.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/wait.h>
//#define READ_FIFO "/tmp/sdi1900096_read"
#define CONCURRENCY_DEFAULT 1
#define PIPENAME_LEN 44 // /tmp/sdi1900096clientr_ + 20 digits + \0

char** tokenize(const char* , int );

int main(int argc, char* argv[]) {
    //IPC setup
    if (mkfifo(SERVER_FIFO,  0666)==-1 && errno != EEXIST){
        perror("mkfifo read error:");
        exit(EXIT_FAILURE);
    }
    int fd_r = open(SERVER_FIFO,O_RDWR|O_NONBLOCK);
    if(fd_r==-1){
        perror("open read fifo:");
        exit(EXIT_FAILURE);
    }
    int fd_server;
    if((fd_server = open("jobExecutorServer.txt",O_CREAT|O_WRONLY,0666))==-1){
        perror("open error:");
        exit(EXIT_FAILURE);
    }
    pid_t serv_chld = fork(); //daemonize
    if(serv_chld==-1){
        perror("fork:");
        exit(EXIT_FAILURE);
    }
    else if(serv_chld!=0){ //initial server pid
        write(fd_server,&serv_chld,sizeof(pid_t));
        exit(EXIT_SUCCESS);
    }
    //server worker runs as server would
    int concurrency_level = CONCURRENCY_DEFAULT;
    Job* job;
    int job_id = 1; // auto_increment ID
    Queue queue; //waiting queue
    Queue running_queue; //running queue, could also just be a list
    char client_rfifo[PIPENAME_LEN] = {0};
    char client_wfifo[PIPENAME_LEN]= {0};
    init(&queue);
    init(&running_queue);
    int nbytes = snprintf(NULL, 0, "%d", getpid());
    char pid_str[nbytes+1];
    snprintf(pid_str,nbytes+1,"%d",getpid());
    write(fd_server,pid_str, sizeof(pid_t )); //write pid to .txt
    //poll and signalfd initialization
    struct pollfd fds[2];
    fds[0].fd = fd_r;
    fds[0].events = POLLIN;
    sigset_t mask;
    int fd_s;
    struct signalfd_siginfo fdsi;
    ssize_t s;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    fd_s = signalfd(-1, &mask, 0);
    if (fd_s == -1) {
        perror("signalfd");
        exit(EXIT_FAILURE);
    }
    fds[1].fd = fd_s;
    fds[1].events = POLLIN;
    request_header h = {.0,.0,.0,.0};
    //event loop start
    while (1){
        while(poll(fds,2,-1)==-1){ //re-poll if EINTR occurs
            if(errno != EINTR){
                perror("poll:");
                exit(EXIT_FAILURE);
            }
        }
        if (fds[1].revents & POLLIN){ //SIGCHLD happened
            s = read(fd_s, &fdsi, sizeof(struct signalfd_siginfo));
            if (s != sizeof(struct signalfd_siginfo)) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (fdsi.ssi_signo == SIGCHLD) {
                pid_t pid;
                while ((pid = waitpid(-1,NULL,WNOHANG))>0){
                    if(fdsi.ssi_pid==h.message){ //the last h.message might have the last terminated child, check if  it is the case and skip it because it already has been removed
                        continue;
                    }
                    Job* done_job= removeNodeId(&running_queue,pid,"pid"); //might be redundant but ok
                    if(done_job){
                        free(done_job->description);
                        free(done_job);
                    }
                }
                while(running_queue.size < concurrency_level && queue.size){ //start jobs from queue
                    job = dequeue(&queue);
                    enqueue(&running_queue,&job->node);
                    char** args = tokenize(job->description,job->description_argc); //tokenize job description
                    job->pid = fork();
                    if(job->pid==-1){
                        perror("fork:");
                        exit(EXIT_FAILURE);
                    }
                    if(job->pid==0){
                        execvp(args[0],args);
                        char errbuf[500]={0}; //this wass for debugging purposes
                        sprintf(errbuf,"execvp1 failed %s\n",args[0]);
                        perror(errbuf);
                        exit(EXIT_FAILURE);
                    }
                    //cleanup mem
                    for (int i = 0; i < job->description_argc; ++i) {
                        free(args[i]);
                    }
                    free(args);
                }
            }
        }
        if(fds[0].revents & POLLIN){ //satisfy requests
            nbytes = 0;
            do { /*this loop saves extra poll(2) syscalls that would return immediately,
                  *because there would be unread data on the pipe,
                  * it will read all available client requests*/
                int bytes_to_read = sizeof(request_header)-nbytes;
                int total_bytes_read = nbytes;
                while(bytes_to_read>0){
                    nbytes = read(fd_r,&h+total_bytes_read,bytes_to_read);
                    bytes_to_read-=nbytes;
                    total_bytes_read+=nbytes;
                }
                sprintf(client_rfifo,"/tmp/sdi1900096clientr_%d",h.pid);
                int fd_w = open(client_rfifo,O_WRONLY);
                if(fd_w==-1){
                    perror("open client read fifo:");
                    exit(EXIT_FAILURE);
                }
                response_header rh;
                char buf[40]={0}; //only for stop messages
                switch (h.command_num) {
                    case 1: //issue job
                        sprintf(client_wfifo,"/tmp/sdi1900096clientw_%d",h.pid);
                        int fd_rc;
                        fd_rc = open(client_wfifo,O_RDONLY); //open the pipe where clients write their job
                        // descriptions , call it fd where server reads from client
                        if(fd_rc==-1){
                            perror("open client write fifo:");
                            exit(EXIT_FAILURE);
                        }
                        char* message_buffer = malloc(h.message+1);
                        message_buffer[h.message] = '\0';
                        bytes_to_read = h.message;
                        total_bytes_read = 0;
                        while(bytes_to_read>0){ //read job description
                            nbytes = read(fd_rc,message_buffer+total_bytes_read,bytes_to_read);
                            if(nbytes==-1){
                                break;
                            }
                            bytes_to_read-=nbytes;
                            total_bytes_read+=nbytes;
                        }
                        close(fd_rc);
                        //create the job
                        job = malloc(sizeof(Job));
                        job->id = job_id++;
                        job->description = strdup(message_buffer);
                        job->description_argc = h.arg_count; //this is useful for tokenizing
                        if (running_queue.size < concurrency_level && !queue.size) { //if job can run, then run it
                            enqueue(&running_queue, &job->node);
                            char** args;
                            args = tokenize(job->description,job->description_argc);
                            job->pid = fork();
                            //execute job
                            if(job->pid==-1){
                                perror("fork:");
                                exit(EXIT_FAILURE);
                            }
                            if(job->pid==0){
                                execvp(args[0],args);
                                char errbuf[500]={0};
                                sprintf(errbuf,"execvp2 failed %s\n",args[0]);
                                perror(errbuf);
                                exit(EXIT_FAILURE);
                            }
                            //mem cleanup
                            for (int i = 0; i < job->description_argc; ++i) {
                                free(args[i]);
                            }
                            free(args);
                            rh.int1 = running_queue.size; // this is the queue position
                        }
                        else { //if not then just queue it
                            enqueue(&queue, &job->node);
                            rh.int1=queue.size; //queue position
                        }
                        //write response
                        free(message_buffer);
                        rh.id = job->id;
                        rh.len = strlen(job->description)+1;
                        write(fd_w,&rh,sizeof(response_header)); //response header first
                        write(fd_w,job->description,rh.len); //then the job description
                        break;
                    case 2: //just set the concurrency level
                        concurrency_level = h.message;
                        //if it increases and new jobs can run, then run them
                        while (queue.size && running_queue.size < concurrency_level) {
                            job = dequeue(&queue);
                            enqueue(&running_queue, &job->node);
                            char** args = tokenize(job->description,job->description_argc);
                            job->pid = fork();
                            if(job->pid==-1){
                                perror("fork:");
                                exit(EXIT_FAILURE);
                            }
                            if(job->pid==0){
                                execvp(args[0],args);
                                char errbuf[500]={0};
                                sprintf(errbuf,"execvp3 failed %s\n",args[0]);
                                perror(errbuf);
                                exit(EXIT_FAILURE);
                            }
                            for (int i = 0; i < job->description_argc; ++i) {
                                free(args[i]);
                            }
                            free(args);
                        }
                        //no printing so just set everything to negative
                        rh.id = -1;
                        rh.int1 = -1;
                        rh.len = -1;
                        write(fd_w,&rh,sizeof(response_header)); //respond
                        break;
                    case 3: //stopping jobs
                        job = removeNodeId(&running_queue,h.message,"jobid"); //remove job from running queue
                        if(job==NULL){ //if it wasn't running then it must be queued so remove it
                            job = removeNodeId(&queue,h.message,"jobid");
                            if(job!=NULL){
                                sprintf(buf,"job_%d removed",job->id);
                                free(job->description);
                                free(job);
                            }
                        }
                        else{ //if it was then terminate it
                            kill(job->pid,SIGKILL);
                            sprintf(buf,"job_%d terminated",job->id);
                            free(job->description);
                            free(job);
                        }
                        //response
                        rh.id = -1;
                        rh.int1 = -1;
                        rh.len = strlen(buf)+1;
                        write(fd_w,&rh,sizeof(response_header)); //header
                        write(fd_w,buf,rh.len); //message
                        buf[0] = '\0'; //idk why but it's here
                        break;
                    case 4:
                        { // compound statement gia na einai ok me ta variable declarations
                            QueueNode *node = NULL;
                            int queue_size = 0;
                            if (h.message == 1) { //runnning
                                node = running_queue.head;
                                queue_size = running_queue.size;
                            } else if (h.message == 2) { //queued
                                node = queue.head;
                                queue_size = queue.size;
                            }
                            struct iovec* vec = malloc((queue_size+2)*sizeof(struct iovec)); //gia tin writev
                            jobTwople* jt = malloc(queue_size*sizeof(jobTwople)); //holds id and queuepos
                            rh.id = -1; //so client know its poll
                            rh.int1 = queue_size; //number of jobs
                            rh.len = 0; // length of all descriptions
                            vec[0].iov_base = &rh; //buffer start
                            vec[0].iov_len = sizeof(response_header); //buffer length
                            vec[1].iov_base = jt; //buffer start
                            vec[1].iov_len = queue_size*sizeof(jobTwople); //buffer length
                            int vec_idx = 2;
                            int queue_pos = 1;
                            while (node != NULL) { //fill the vector with job descriptions buffers
                                job = getJob(node, Job, node); //container of magic
                                vec[vec_idx].iov_base = job->description; //buffer start
                                vec[vec_idx].iov_len = strlen(job->description)+1; //length + 1 to include the '\0' byte
                                rh.len += vec[vec_idx].iov_len; // sum of all descriptions
                                jt[queue_pos-1].job_id = job->id;
                                jt[queue_pos-1].queue_pos = queue_pos;
                                queue_pos++; //this is the index in the queue
                                vec_idx++; //next buffer
                                node = node->next; //next job
                            }
                            writev(fd_w,vec,queue_size+2); //write response header and message with one syscall
                            //mem cleanup
                            free(vec);
                            free(jt);
                        }
                        break;
                    case 5: //exit gracefully
                        rh.id = -1;
                        rh.int1 = -1;
                        rh.len = strlen("jobExecutorServer terminated.")+1;
                        QueueNode* node = queue.head;
                        //i decided to terminate all jobs when exiting
                        while (node!=NULL){
                            job = getJob(node,Job,node);
                            node = node->next;
                            free(job->description);
                            free(job);
                        }
                        node = running_queue.head;
                        while (node!=NULL){
                            job = getJob(node,Job,node);
                            node = node->next;
                            kill(job->pid,SIGKILL);
                            free(job->description);
                            free(job);
                        }
                        write(fd_w,&rh,sizeof(response_header));
                        write(fd_w,"jobExecutorServer terminated.",rh.len);
                        close(fd_r);
                        close(fd_s);
                        unlink(SERVER_FIFO);
                        unlink("jobExecutorServer.txt");
                        exit(EXIT_SUCCESS);
                    default:
                        break;
                }
                close(fd_w);
            } while ((nbytes = read(fd_r,&h,sizeof(request_header)))!=-1 || errno!=EAGAIN); //read until no more requests
            //this works because the pipe is opened in non-blocking mode, so EAGAIN means it would block
        }
    }
}


char** tokenize(const char* input, int num_tokens) { //tokenize with space delimeter and keep quotes intact
    char* str = strdup(input);
    char** result = malloc((num_tokens + 1) * sizeof(char*)); // allocate memory for the known number of tokens
    int count = 0;
    char* token_start = str; //start of 1st token is the start of stirng
    int in_quotes = 0; //state for quotes
    //parse string
    for (char* p = str; *p; ++p) {
        if (*p == '\"' || *p == '\'') { //quotes beginning
            in_quotes = !in_quotes;
        }
        else if (*p == ' ' && !in_quotes) { //found a token, if we are not in quotes
            *p = '\0'; //the current token ends here
            result[count++] = strdup(token_start); //add the token to the list
            token_start = p + 1; //start new token
        }
    }

    // add the last token
    if (token_start != str + strlen(str)) {
        result[count++] = strdup(token_start);
    }
    result[num_tokens] = NULL;  // Null terminate the list of tokens
//    printf("count: %d num_tokens: %d\n",count,num_tokens);
//    for (int i = 0; i < count; i++) {
//        printf("%s ",result[i]);
//    }
    free(str);
    return result;
}