
#ifndef ESP_PLATFORM

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <sys/signal.h>


static int sock;


int16_t read_byte() {
    uint8_t b;

    if (read(sock, &b, 1) == 1) {
        return b;
    } else {
        return -1;
    }
}

void read_blob(void* buf, int32_t btr) {
    int32_t read_ = 0;
    uint8_t* p = buf;
    while (read_ != btr) {
        int32_t br = read(sock, p + read_, btr - read_);
        if (br == -1) {
            return;
        }
        read_ += br;
    }
}

void write_byte(uint8_t b) {
    write(sock, &b, 1);
}

void write_blob(void* buf, int32_t btw) {
    ssize_t wrote = 0;
    uint8_t* p = buf;
    while (wrote != btw) {
        ssize_t bw = write(sock, p + wrote, btw - wrote);
        if (bw == -1) {
            return;
        }
        wrote += bw;
    }
}

void init_socket_io() {

    short int port;
    struct    sockaddr_in servaddr;


    port = 8088;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating listening socket.\n");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);

    if (inet_aton("127.0.0.1", &servaddr.sin_addr) <= 0) {
        printf("Invalid remote IP address.\n");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *) &servaddr, sizeof(servaddr) ) < 0 ) {
        printf("Error calling connect()\n");
        exit(EXIT_FAILURE);
    }
}

#endif

