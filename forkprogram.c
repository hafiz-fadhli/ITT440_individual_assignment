#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
 
#define NUM_CHILDREN 3
#define BUF_SIZE     256
 

static volatile sig_atomic_t interrupted = 0;
 

static void sigint_handler(int sig)
{
    (void)sig;
    const char msg[] =
        "\n[PARENT] *** Interrupt detected (Ctrl+C) *** "
        "Shutting down children and exiting...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    interrupted = 1;
}
 

static void child_process(int id, int read_fd)
{
    char buf[BUF_SIZE];
    ssize_t n;
 
    printf("[Child %d | PID %d] Started, waiting for messages...\n",
           id, (int)getpid());
    fflush(stdout);
 
    while ((n = read(read_fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("[Child %d | PID %d] Received message: %s",
               id, (int)getpid(), buf);
        if (buf[n - 1] != '\n')
            printf("\n");
        fflush(stdout);
    }
 
    printf("[Child %d | PID %d] Pipe closed, exiting.\n", id, (int)getpid());
    fflush(stdout);
    close(read_fd);
    _exit(0);
}
 
int main(void)
{
    int   pipes[NUM_CHILDREN][2];
    pid_t pids[NUM_CHILDREN];
    char  input[BUF_SIZE];
    int   i, j;

    signal(SIGINT, SIG_IGN);
 
    for (i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
 
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
 
        if (pids[i] == 0) {
            close(pipes[i][1]);
            for (j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            child_process(i + 1, pipes[i][0]);
        }
 
    }
 
    for (i = 0; i < NUM_CHILDREN; i++)
        close(pipes[i][0]);
 
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
 
    printf("[PARENT | PID %d] Created %d child processes.\n",
           (int)getpid(), NUM_CHILDREN);
    printf("[PARENT] Type a message and press Enter to send it to all children.\n");
    printf("[PARENT] Type 'exit' to quit, or press Ctrl+C to interrupt.\n\n");
 
    while (!interrupted) {
        printf("Enter message: ");
        fflush(stdout);
 
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (errno == EINTR || interrupted)
                break;
            break;
        }
 
        if (strcmp(input, "exit\n") == 0 || strcmp(input, "exit") == 0)
            break;
 
        for (i = 0; i < NUM_CHILDREN; i++) {
            if (write(pipes[i][1], input, strlen(input)) == -1)
                perror("write");
        }
 
        usleep(200000);
    }
 
    printf("\n[PARENT] Closing pipes and waiting for children...\n");
    for (i = 0; i < NUM_CHILDREN; i++)
        close(pipes[i][1]);
 
    for (i = 0; i < NUM_CHILDREN; i++)
        waitpid(pids[i], NULL, 0);
 
    if (interrupted)
        printf("[PARENT] Program terminated by user interrupt (SIGINT).\n");
    else
        printf("[PARENT] All children finished. Program ended normally.\n");
 
    return 0;
}
