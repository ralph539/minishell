#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>

pid_t pid_global = -1;

void handler(int sig) {
    int status;
    pid_t pid_fork;
    while ((pid_fork = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0) {
        if (pid_fork == pid_global) pid_global = -1;
        if (WIFEXITED(status)) {
            printf("Child process %d exited with code %i\n",
                   pid_fork, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process %d was terminated by signal %i\n",
                   pid_fork, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("Child process %d was suspended (signal %d)\n",
                   pid_fork, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("Child process %d resumed\n", pid_fork);
        }
    }
}

void ignore(int sig) {
    printf(" Control + C ignored, ");
}

void builtin_cd(char **cmd) {
    char *path;
    if (cmd[1] != NULL) {
        path = cmd[1];
    } else {
        path = getenv("HOME");
    }
    if (chdir(path) < 0) {
        perror("Error");
    }
}

void builtin_dir(char **cmd) {
    char *path;
    if (cmd[1] != NULL) {
        path = cmd[1];
    } else {
        path = ".";
    }

    DIR *directory = opendir(path);
    if (directory == NULL) {
        perror("Invalid directory");
    } else {
        struct dirent *entry;
        while ((entry = readdir(directory)) != NULL) {
            printf("%s\n", entry->d_name);
        }
        closedir(directory);
    }
}

int main(void) {
    struct sigaction action;
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &action, NULL);

    action.sa_handler = ignore;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    bool done = false;

    while (!done) {
        printf("> ");
        struct cmdline *cmdline = readcmd();
        if (cmdline == NULL) {
            perror("command read error");
            exit(EXIT_FAILURE);
        }
        if (cmdline->err) {
            printf("input error: %s\n", cmdline->err);
            continue;
        }

        // Step 17
        if (cmdline->seq[1] != NULL) {          
            int in_fd = STDIN_FILENO;           // input for the current command
            int pipefd[2];

            for (int i = 0; cmdline->seq[i]; i++) {
                char **part = cmdline->seq[i];
                int   use_pipe = (cmdline->seq[i+1] != NULL);   

                if (use_pipe && pipe(pipefd) < 0) {
                    perror("pipe"); break;
                }

                pid_t pid = fork();
                if (pid == 0) {                                   // Child
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    sigprocmask(SIG_UNBLOCK, &mask, NULL);      // unblock signals

                    if (i == 0 && cmdline->in) {                 
                        int fd = open(cmdline->in, O_RDONLY);
                        if (fd < 0) { perror("open in"); _exit(1); }
                        dup2(fd, STDIN_FILENO); close(fd);
                    } else if (in_fd != STDIN_FILENO) {          
                        dup2(in_fd, STDIN_FILENO);
                    }

                    if (use_pipe) {                              
                        close(pipefd[0]);                        
                        dup2(pipefd[1], STDOUT_FILENO);
                        close(pipefd[1]);
                    } else if (!use_pipe && cmdline->out) {    
                        int fd = open(cmdline->out,
                                       O_WRONLY|O_CREAT|O_TRUNC, 0644);
                        if (fd < 0) { perror("open out"); _exit(1); }
                        dup2(fd, STDOUT_FILENO); close(fd);
                    }

                    if (in_fd != STDIN_FILENO) close(in_fd);
                    execvp(part[0], part);
                    perror(part[0]); _exit(1);
                }

                if (in_fd != STDIN_FILENO) close(in_fd);         // close previous

                if (use_pipe) {           
                    close(pipefd[1]);       
                    in_fd = pipefd[0];     
                } else {
                    in_fd = STDIN_FILENO; 
                }
            }

            // Wait for all pipeline children
            while (wait(NULL) > 0);
            continue;
        }

        {
            int indexseq = 0;
            char **cmd;
            while ((cmd = cmdline->seq[indexseq])) {
                if (!cmd[0]) { indexseq++; continue; }

                if (strcmp(cmd[0], "exit") == 0) {
                    done = true;
                    printf("Goodbye ...\n");
                }
                else if (strcmp(cmd[0], "cd") == 0) {
                    builtin_cd(cmd);
                }
                else if (strcmp(cmd[0], "dir") == 0) {
                    builtin_dir(cmd);
                }
                else {
                    pid_t pid_fork = fork();
                    if (pid_fork < 0) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    }
                    if (pid_fork == 0) {
                        signal(SIGINT, SIG_DFL);
                        signal(SIGTSTP, SIG_DFL);
                        sigprocmask(SIG_UNBLOCK, &mask, NULL);  // unblock signals
                        if (cmdline->backgrounded) setpgrp();
                        if (cmdline->in) {
                            int fd = open(cmdline->in, O_RDONLY);
                            if (fd < 0) { perror("open in"); exit(EXIT_FAILURE); }
                            dup2(fd, STDIN_FILENO); close(fd);
                        }
                        if (cmdline->out) {
                            int fd = open(cmdline->out,
                                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if (fd < 0) { perror("open out"); exit(EXIT_FAILURE); }
                            dup2(fd, STDOUT_FILENO); close(fd);
                        }
                        execvp(cmd[0], cmd);
                        perror("execvp");
                        exit(EXIT_FAILURE);
                    }
                    else {
                        if (!cmdline->backgrounded) {
                            pid_global = pid_fork;
                            while (pid_global != -1) pause();
                        } else {
                            printf("Launching background command (pid %d)\n", pid_fork);
                        }
                    }
                }
                indexseq++;
            }
        } 
    }
    return EXIT_SUCCESS;
}

