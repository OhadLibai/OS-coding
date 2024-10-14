#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h> 

#include <signal.h>
#include <sys/stat.h> // stat

#include <fcntl.h> // io_redirection

#include <string.h> //strcmp 
#include <wait.h>


/* redefining SIGCHLD,
   eliminating zombies */
void sigchld_handler(int signal) { // credit to sources: cboard blog + stackoverflow
    while (waitpid(-1, NULL, WNOHANG) > 0) {};
}


void SIGCHLD_handling() {
    struct sigaction sa;
    sa.sa_handler = &sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP; 
    if(sigaction(SIGCHLD, &sa, 0) == -1) { 
        perror("Defining SIGCHLD failure");
        exit(1); // error handling instructions
    }   
}


/* redefining SIGINT */
void SIGINT_handling() {  
    struct sigaction sa;
	sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, 0) == -1) {
		perror("Defining SIGINT failure");
        exit(1); // error handling instructions	
    }
}


/* reseting SIGINT with default,
   for foreground child */
void reset_SIGINT_SIGDFL() {
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0) == -1){
        perror("Reset SIGINT failure");
        exit(1); // error handling instructions
    }
}


int pipe_index(int count, char** arglist) {
	for (int i = 1; i < (count-1); i++) { // legal pipe commands are given.
		if (*arglist[i] == '|')
			return i;
    }        
	return -1;
}


int file_exists (char* filename) {
        struct stat buffer;   
        return (stat (filename, &buffer) == 0);
}


int background(int count, char** arglist) {
    arglist[count - 1] = NULL; // Behavior, 2.2

    int pid = fork();
    if (pid == -1) {
        perror("Failed forking while current background command was given");
        return 0;
    }

    if (pid == 0) {
        execvp(arglist[0], arglist);
        perror("Failed performing execvp"); // should not get here
        exit(1);
    }

    return 1; // successful background command
}


int pipe_command(char** arglist, int pipe_index) {
    arglist[pipe_index] = NULL;

    int fd[2];
    if (pipe(fd) == -1) {
        perror("Failed fd while piping");
        return 0;
    }

    int pid_1 = fork();
    if (pid_1 == -1) {
        perror("Pipe fork error");
        return 0;
    }

    if (pid_1 == 0) { // first child insert the pipe
        reset_SIGINT_SIGDFL();
        close(fd[0]);

        if (dup2(fd[1],1) == -1) {
            perror("Failed on dup2() trying to pipe through first born");
            exit(1);
        }
        close(fd[1]); // this is after dup2
        execvp(arglist[0], arglist);
        perror("Pipe command: failed execvp in first born"); // should not get here
        exit(1);
    }

    // setting for second child to take the input
    int pid_2 = fork();
    if (pid_2 == -1) {
        perror("Pipe fork error");
        return 0; 
    }

    if (pid_2 == 0) {
        reset_SIGINT_SIGDFL();
        close(fd[1]);

        if (dup2(fd[0], 0) == -1) {
            perror("Failed on dup2() trying piping through second born");
            exit(1);
        }
        close(fd[0]); // after dup2()
        execvp(arglist[pipe_index + 1], arglist + pipe_index + 1);
        perror("Pipe command: failed execvp in second born");
        exit(1);
    }

    // closing in parent
    close(fd[0]); close(fd[1]);

    // wait routine
    int wait_call = waitpid(pid_1, NULL, 0); // first born
    if ((wait_call==-1) && errno != ECHILD && errno != EINTR) {
        perror("error on wait to first born, pipe");
        return 0;
    }

    wait_call = waitpid(pid_2, NULL, 0); // second born
    if ((wait_call==-1) && errno != ECHILD && errno != EINTR) {
        perror("error on wait to second born, pipe");
        return 0;
    }  

    return 1;
}


int exec_regular_command(char** arglist) {
    int pid = fork();
    if (pid == -1) {
        perror("Failed fork() on exec_regular_command");
        return 0;
    }

    if (pid == 0) {
        reset_SIGINT_SIGDFL();
        execvp(arglist[0], arglist);
        perror("Execvp on regular command failed"); // should not be here
        exit(1);
    }

    int wait_call = waitpid(pid, NULL, 0);
    if ( (wait_call == -1) && errno!=EINTR && errno!=ECHILD) { 
        perror("wait failed, regular command.");
        return 0; // error handling (3)
    }

    return 1; // successful running.
}


int input_redirect(int count, char** arglist) { // a little credit: KRIS JORDAN YouTube channel.
    arglist[count - 2] = NULL; // slice

    int pid = fork();
    if (pid == -1) {
        perror("input redirect fork failed");
        return 0;
    }

    if (pid == 0) {
        reset_SIGINT_SIGDFL();

        int fd = open(arglist[count - 1], O_RDONLY, 0777); // setting new file with permissions and flags
                                                           // assuming file exists and should not be created
        if (fd == -1) {
            perror("open() sys call failed, input redirection");
            exit(1);
        }

        if (dup2(fd, 0) == -1) {
            perror("dup2() input redirection failure");
            exit(1);
        }

        close(fd); // ensure reference count won't scale up
        
        execvp(arglist[0], arglist); // going to redirect
        perror("execvp failed, redirect input");
        exit(1);
    }

    // waiting procedure
    int wait_call = waitpid(pid, NULL, 0);     
    if ((wait_call == -1) && errno != ECHILD && errno != EINTR) {
        perror("input redirection, wait call failed");
        return 0; 
    }

    return 1; // success in process_arglist
}



int append_output_redirect(int count, char** arglist) { // a little credit: KRIS JORDAN YouTube channel.
    arglist[count - 2] = NULL; // slice

    int pid = fork();
    if (pid == -1) {
        perror("failed forking on appending redirect output");
        return 0; 
    }

    if (pid == 0) {
        reset_SIGINT_SIGDFL();
        int fd;
        
        // using access: if (access(arglist[count-1], F_OK) == 0)
        // using stat
        if (file_exists(arglist[count-1])) // file exists
            fd = open(arglist[count - 1], O_WRONLY | O_APPEND);    

        else  // file need to be created
            fd = open(arglist[count - 1], O_CREAT | O_WRONLY, 0777); // flags and permissions    
            
        if (fd == -1) {
            perror("error opening the file, open() sys call failed");
            exit(1);
        }
    
        if (dup2(fd, 1) == -1) {
        perror("dup2 error while appending redirect output to existing file");
        exit(1);
        }
        
        close(fd); // ensure reference count won't scale up

        execvp(arglist[0], arglist); // going to redirect
        perror("execvp failed, redirect output");
        exit(1);
    }
    
    // waiting procedure
    int wait_call = waitpid(pid, NULL, 0);     
    if ((wait_call == -1) && errno != ECHILD && errno != EINTR) {
        perror("output redirection, wait call failed");
        return 0; 
    }

    return 1; // success in process_arglist
}


int prepare() { // initializing handlers
    SIGCHLD_handling();
    SIGINT_handling();
    return 0; 
}


int process_arglist(int count, char** arglist) {
    // background
    if (strcmp(arglist[count-1], "&") == 0)
        return background(count, arglist);

    if (count > 1) {
        int pipe_indx = pipe_index(count, arglist);
        if (!(pipe_indx == -1))
            return pipe_command(arglist, pipe_indx);
        else { 
            if ( strcmp(arglist[count-2], ">>") == 0 ) 
                return append_output_redirect(count, arglist);

            if  (*arglist[count-2] == '<')
                return input_redirect(count, arglist);     
        }
    }  
    
    return exec_regular_command(arglist); 
}


int finalize(void) { // bon voyage
    return 0;
}