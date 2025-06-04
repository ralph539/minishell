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

void traitement(int sig) {
    int status;
    pid_t pid_fork;
    while ((pid_fork = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0) {
        if (pid_fork == pid_global) pid_global = -1;
        if (WIFEXITED(status)) {
            printf("Le processus fils %d s’est terminé avec le code %i\n",
                   pid_fork, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Le processus fils %d s’est terminé par le signal %i\n",
                   pid_fork, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("Le processus fils %d est suspendu (signal %d)\n",
                   pid_fork, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("Le processus fils %d reprend\n", pid_fork);
        }
    }
}

void ignorer(int sig) {
    printf(" Control + C ignoré, ");
}

void commande_cd(char **cmd) {
    char *chemin;
    if (cmd[1] != NULL) {
        chemin = cmd[1];
    } else {
        chemin = getenv("HOME");
    }
    if (chdir(chemin) < 0) {
        perror("Erreur");
    }
}

void commande_dir(char **cmd) {
    char *chemin;
    if (cmd[1] != NULL) {
        chemin = cmd[1];
    } else {
        chemin = ".";
    }

    DIR *repertoire = opendir(chemin);
    if (repertoire == NULL) {
        perror("Erreur repertoire non valide");
    } else {
        struct dirent *in;
        while ((in = readdir(repertoire)) != NULL) {
            printf("%s\n", in->d_name);
        }
        closedir(repertoire);
    }
}

int main(void) {
    struct sigaction action;
    action.sa_handler = traitement;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &action, NULL);

    action.sa_handler = ignorer;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);

    sigset_t masque;
    sigemptyset(&masque);
    sigaddset(&masque, SIGINT);
    sigaddset(&masque, SIGTSTP);
    sigprocmask(SIG_BLOCK, &masque, NULL);

    bool fini = false;

    while (!fini) {
        printf("> ");
        struct cmdline *commande = readcmd();
        if (commande == NULL) {
            perror("lecture commande");
            exit(EXIT_FAILURE);
        }
        if (commande->err) {
            printf("erreur saisie : %s\n", commande->err);
            continue;
        }

        // Etape 17
        if (commande->seq[1] != NULL) {          
            int in_fd = STDIN_FILENO;            //entrée de la commande courante 
            int tube[2];

            for (int i = 0; commande->seq[i]; i++) {
                char **maillon = commande->seq[i];
                int   u_pipe = (commande->seq[i+1] != NULL);   

                if (u_pipe && pipe(tube) < 0) {
                    perror("pipe"); break;
                }

                pid_t pid = fork();
                if (pid == 0) {                                   // Fils
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    sigprocmask(SIG_UNBLOCK, &masque, NULL);      // déblocage

                    if (i == 0 && commande->in) {                 
                        int fd = open(commande->in, O_RDONLY);
                        if (fd < 0) { perror("open in"); _exit(1); }
                        dup2(fd, STDIN_FILENO); close(fd);
                    } else if (in_fd != STDIN_FILENO) {           
                        dup2(in_fd, STDIN_FILENO);
                    }

                    if (u_pipe) {                               
                        close(tube[0]);                           
                        dup2(tube[1], STDOUT_FILENO);
                        close(tube[1]);
                    } else if (!u_pipe && commande->out) {    
                        int fd = open(commande->out,
                                       O_WRONLY|O_CREAT|O_TRUNC, 0644);
                        if (fd < 0) { perror("open out"); _exit(1); }
                        dup2(fd, STDOUT_FILENO); close(fd);
                    }

                    if (in_fd != STDIN_FILENO) close(in_fd);
                    execvp(maillon[0], maillon);
                    perror(maillon[0]); _exit(1);
                }

                if (in_fd != STDIN_FILENO) close(in_fd);          // fin tube préc 

                if (u_pipe) {           
                    close(tube[1]);       
                    in_fd = tube[0];     
                } else {
                    in_fd = STDIN_FILENO; 
                }
            }

            // Attendre tous les fils du pipeline 
            while (wait(NULL) > 0);
            continue;
        }
   

        {
            int indexseq = 0;
            char **cmd;
            while ((cmd = commande->seq[indexseq])) {
                if (!cmd[0]) { indexseq++; continue; }

                if (strcmp(cmd[0], "exit") == 0) {
                    fini = true;
                    printf("Au revoir ...\n");
                }
                else if (strcmp(cmd[0], "cd") == 0) {
                    commande_cd(cmd);
                }
                else if (strcmp(cmd[0], "dir") == 0) {
                    commande_dir(cmd);
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
                        sigprocmask(SIG_UNBLOCK, &masque, NULL);  // déblocage
                        if (commande->backgrounded) setpgrp();
                        if (commande->in) {
                            int fd = open(commande->in, O_RDONLY);
                            if (fd < 0) { perror("open in"); exit(EXIT_FAILURE); }
                            dup2(fd, STDIN_FILENO); close(fd);
                        }
                        if (commande->out) {
                            int fd = open(commande->out,
                                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if (fd < 0) { perror("open out"); exit(EXIT_FAILURE); }
                            dup2(fd, STDOUT_FILENO); close(fd);
                        }
                        execvp(cmd[0], cmd);
                        perror("execvp");
                        exit(EXIT_FAILURE);
                    }
                    else {
                        if (!commande->backgrounded) {
                            pid_global = pid_fork;
                            while (pid_global != -1) pause();
                        } else {
                            printf("Lancement de commande en tache de fond (pid %d)\n", pid_fork);
                        }
                    }
                }
                indexseq++;
            }
        } 
    }
    return EXIT_SUCCESS;
}

