#include <vector>
#include <chrono>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <numeric>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/resource.h> 
#include <sys/wait.h> 
#include <fcntl.h>
#include <linux/sched.h>
#include <sched.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

#define SRV_LISTEN_PORT 8080
#define SRV_IP "127.0.0.1"
#define NUM_LOOPS 5

void run_clnt() {
    int status, srv_fd;
    struct sockaddr_in serv_addr;
    if ((srv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SRV_LISTEN_PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SRV_IP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
 
    if ((status = connect(srv_fd, (struct sockaddr*)&serv_addr,
                   sizeof(serv_addr))) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    cout << "connected to srv" << endl;
 
    // closing the connected socket
    close(srv_fd);
}


int main() {
    for(int i = 0; i < NUM_LOOPS; i++) {
        run_clnt();
    }
}