// SERVER

#include <unistd.h>

#include <sys/types.h>

#include <signal.h>
#include <bits/sigaction.h>
#include <asm-generic/signal-defs.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <string.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024 * 1024

// globals
ssize_t curr_connection_fd = -1; // new sockets_fds from OS, initialized with -1
bool on_air = true;
uint32_t pcc_total[95];

// print and exit
void print_execution() {
    for (int i = 0; i < 95; i++) 
        printf("char '%c' : %u times\n", (char) (i+32), pcc_total[i]);
    exit(0);
}

void SIGINT_handler(int signum) {
    if (curr_connection_fd == -1) // in case we are not right in the middle of proccessing a client
        print_execution();

    else
        on_air = false; // signaling to the main loop to finish processing current client and calling print
}



int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Invalid number of arguments to the program");
        exit(1);
    }

    // defining sigaction for the program
    struct sigaction SIGINT_act; 
    SIGINT_act.sa_handler = &SIGINT_handler;
    SIGINT_act.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &SIGINT_act, 0) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // init sock_addr
    struct sockaddr_in server_addr = {AF_INET, htons(atoi(argv[1]))};
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    // socketing
    int connection_fd; // gets new fd by accept() from here
    if ((connection_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // reuse addr
    if (setsockopt(connection_fd, SOL_SOCKET, SO_REUSEADDR, (bool*)&(bool){true}, sizeof(bool)) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // binding
    if (bind(connection_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    //listening
    if (listen(connection_fd, 10) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // ###
    //  MAIN LOOP 
    //         ###
    
    uint32_t N, N_net_byte_ord;
    uint32_t C, C_net_byte_ord; 

    char buffer[BUFFER_SIZE];
    uint32_t curr_client_pcc[95];
    
    uint32_t total_read_bytes = 0; 
    ssize_t curr_read_bytes = 0;
    uint32_t transferred_read_bytes = 0;

    uint32_t total_written_bytes = 0;
    ssize_t curr_written_bytes = 0;

    while (on_air) {
        memset(curr_client_pcc, '\0', sizeof(curr_client_pcc));
        C = 0;

        curr_connection_fd = accept(connection_fd, NULL, NULL);

        if (curr_connection_fd == -1) {
            fprintf(stderr, "%s", strerror(errno));
            exit(1);
        }

        // extracting N
        total_read_bytes = 0; 
        curr_read_bytes = 0;
        do {
            curr_read_bytes = read(curr_connection_fd, (char*)&N_net_byte_ord + total_read_bytes, 4 - total_read_bytes);
            total_read_bytes += curr_read_bytes;
        } while (curr_read_bytes > 0);
        
        if ( curr_read_bytes == -1 || total_read_bytes != 4 ) {

            if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE || total_read_bytes != 4) { 
                curr_read_bytes == 0 ? fprintf(stderr, "N could not be read") : fprintf(stderr, "%s", strerror(errno));
                close(curr_connection_fd);
                curr_connection_fd = -1;
                continue; // continue to another request
            }

            else { // error that relates to something else in the program
                fprintf(stderr, "%s", strerror(errno));
                exit(1);
            }
        }

        N = ntohl(N_net_byte_ord);

        // reading the content
        transferred_read_bytes = 0;
        while ((curr_read_bytes = read(curr_connection_fd, buffer, N - transferred_read_bytes)) > 0 ) {// read the content
            // write from the buffer to current pcc statistics  
            for (int i = 0; i < curr_read_bytes; i++) {
                if(32 <= buffer[i] && buffer[i] <= 126) {
                    curr_client_pcc[(int)(buffer[i]-32)]++;
                    C++;
                }
            }
            transferred_read_bytes += curr_read_bytes;
            memset(buffer, '\0', BUFFER_SIZE);
        }

        
        if ( curr_read_bytes == -1 || transferred_read_bytes != N ) {

            if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE || transferred_read_bytes != N) { 
                curr_read_bytes == 0 ? fprintf(stderr, "Could not get all the N bytes") : fprintf(stderr, "%s", strerror(errno));
                close(curr_connection_fd);
                curr_connection_fd = -1;
                continue; // continue to another request
            }

            else { // error that relates to something else in the program
                fprintf(stderr, "%s", strerror(errno));
                exit(1);
            }
        }

        // sending C to client
        C_net_byte_ord = htonl(C);

        total_written_bytes = 0;
        curr_written_bytes = 0;
        do {
            curr_written_bytes = write(curr_connection_fd, (char*)&C_net_byte_ord + total_written_bytes, 4 - total_written_bytes);
            total_written_bytes += curr_written_bytes;
        } while (curr_written_bytes > 0);
        
        if ( curr_written_bytes == -1 || total_written_bytes != 4 ) {

            if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE || total_written_bytes != 4) { 
                curr_written_bytes == 0 ? fprintf(stderr, "C could not be written to client") : fprintf(stderr, "%s", strerror(errno));
                close(curr_connection_fd);
                curr_connection_fd = -1;
                continue; // continue to another request
            }

            else { // error that relates to something else in the program
                fprintf(stderr, "%s", strerror(errno));
                exit(1);
            }
        }

        // updating pcc_table
        for(int i = 0; i < 95; i++)
            pcc_total[i] += curr_client_pcc[i];

        close(curr_connection_fd);
        curr_connection_fd = -1;

    }

    print_execution();
}