// CLIENT

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define BUFFER_SIZE 1024 * 1024


int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Invalid number of arguments to the program");
        exit(1);
    }

    // init sock_addr
    struct sockaddr_in server_addr = {AF_INET, htons(atoi(argv[2]))}; // initialized with sin_family and sin_port   
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr); // we are allowed to assume that valid IP addresses are given

    // socketing
    int socket_fd;                                                        
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // connecting to the server
    if (connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // FILE
    int file_fd;
    uint32_t N; 

    file_fd = open(argv[3], O_RDONLY);
    if (file_fd == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }
    
    if ((N = lseek(file_fd, 0, SEEK_END)) == -1) {
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }
    
    if (lseek(file_fd, 0, SEEK_SET) < 0) { // repositioning to the begining of the file
        fprintf(stderr, "%s", strerror(errno));
        exit(1);
    }

    // sending N, the number of bytes that will be transferred
    uint32_t N_net_byte_ord = htonl(N); // network byte order
    uint32_t total_written_bytes = 0;
    ssize_t written_bytes;

    while (total_written_bytes < 4) { // 4-BYTE sized 
        written_bytes = write(socket_fd, ((char*)&N_net_byte_ord) + total_written_bytes, 4 - total_written_bytes);
        if (written_bytes == -1) {
            fprintf(stderr, "%s", strerror(errno));
            exit(1);
        }
        total_written_bytes += written_bytes;
    }

    // sending the file content
    // fill the buffer then emptying it to the socket
    char buffer[BUFFER_SIZE];
    size_t curr_buf_read_cnt; // how many bytes we read to the buffer
    ssize_t read_bytes;
    uint32_t total_read_bytes = 0; // counter up to N

    uint32_t transferred_bytes = 0;

    while (transferred_bytes < N) {
        // nullifying for current loop
        memset(buffer, '\0', BUFFER_SIZE);     
        curr_buf_read_cnt = 0;
        total_written_bytes = 0;

        while (curr_buf_read_cnt < BUFFER_SIZE && (total_read_bytes < N)) { // fulling the buffer before writting to the socket
            read_bytes = read(file_fd, buffer + curr_buf_read_cnt, BUFFER_SIZE - curr_buf_read_cnt);
            if (read_bytes == -1) {
                fprintf(stderr, "%s", strerror(errno));
                exit(1);
            }
            curr_buf_read_cnt += read_bytes;

            total_read_bytes += read_bytes; // EOF like counter
        }
        
        while (total_written_bytes < curr_buf_read_cnt) { // pour from the buffer to the socket
            written_bytes = write(socket_fd, buffer + total_written_bytes, curr_buf_read_cnt - total_written_bytes);
            if (written_bytes == -1) {
                fprintf(stderr, "%s", strerror(errno));
                exit(1);
            }
            total_written_bytes += written_bytes;
        }
        transferred_bytes += total_written_bytes;
    }
    close(file_fd);


    // taking care of C, the number of printable characters
    uint32_t C, C_net_byte_ord;
    read_bytes = 0;
    total_read_bytes = 0;

    while (total_read_bytes < 4) { // 4-BYTE sized 
        read_bytes = read(socket_fd, ((char*)&C_net_byte_ord) + total_read_bytes, 4 - total_read_bytes);
        if (read_bytes == -1) {
            fprintf(stderr, "%s", strerror(errno));
            exit(1);
        }
        total_read_bytes += read_bytes;
    }
    close(socket_fd);

    C = ntohl(C_net_byte_ord); // converting
    printf("# of printable characters: %u\n", C);

    exit(0); // finished
}