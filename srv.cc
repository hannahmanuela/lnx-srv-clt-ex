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

using namespace std;
using namespace std::chrono;

#define SRV_LISTEN_PORT 8080
#define WRK_SLC 5
#define SRV_SLC 50
#define NSEC_PER_MSEC 1000000

// can also include #include <linux/sched/types.h>, but then we get known include errors with sched_param being double defined
struct sched_attr {
    uint32_t size;

    uint32_t sched_policy;
    uint64_t sched_flags;

    /* SCHED_NORMAL, SCHED_BATCH */
    int32_t sched_nice;

    /* SCHED_FIFO, SCHED_RR */
    uint32_t sched_priority;

    /* SCHED_DEADLINE (nsec) */
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};




int time_since(high_resolution_clock::time_point start) {
    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start);
    return 1000 * time_span.count();  // 1000 => shows millisec; 1000000 => shows microsec
}

// 50 ms
int short_fac() {

    high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    long long sum = 0;
    for (long long i = 0; i < 10000000; i++) {
        sum = 3 * i + 1;
    }

    cout << "short run: " << time_since(start) << "ms" << endl;

    return sum;
}



void run_workload(vector<int> *pids) {

    int c_pid = fork();

    if (c_pid == -1) { 
        cout << "fork failed: " << strerror(errno) << endl;
        perror("fork"); 
        exit(EXIT_FAILURE); 
    } else if (c_pid > 0) {
        pids->push_back(c_pid);
        cout << "returning" << endl;
        return; 
    } else {

        high_resolution_clock::time_point spawn_time = high_resolution_clock::now();

        struct sched_attr attr;
        int ret = syscall(SYS_sched_getattr, getpid(), &attr, sizeof(attr), 0);;
        if (ret < 0) {
            perror("ERROR: sched_getattr");
        }
        attr.sched_runtime = WRK_SLC * NSEC_PER_MSEC;
        ret = syscall(SYS_sched_setattr, getpid(), &attr, 0);
        if (ret < 0) {
            perror("ERROR: sched_setattr");
        }

        // cout << "set sched_runtime to be " << rt_to_use << endl;
        // set child affinity - they will both run on cpu 1
        cpu_set_t  mask;
        CPU_ZERO(&mask);
        CPU_SET(1, &mask);
        if ( sched_setaffinity(0, sizeof(mask), &mask) > 0) {
            cout << "set affinity had an error" << endl;
        }

        high_resolution_clock::time_point work_start = high_resolution_clock::now();
        cout << getpid() << ": starting work after " << time_since(spawn_time) << endl;
        
        short_fac();

        cout << getpid() << ": done working after " << time_since(spawn_time) << ", having used "<< time_since(work_start) << "ms to do its work" << endl;

        exit(0);
    }
}





void run_server() {

    // set CPU mask
    cpu_set_t  mask;
    CPU_ZERO(&mask);
    CPU_SET(1, &mask);
    if ( sched_setaffinity(0, sizeof(mask), &mask) > 0) {
        cout << "set affinity had an error" << endl;
    }

    // set slice
    struct sched_attr attr;
    int ret = syscall(SYS_sched_getattr, getpid(), &attr, sizeof(attr), 0);;
    if (ret < 0) {
        perror("ERROR: sched_getattr");
    }
    attr.sched_runtime = WRK_SLC * NSEC_PER_MSEC;
    ret = syscall(SYS_sched_setattr, getpid(), &attr, 0);
    if (ret < 0) {
        perror("ERROR: sched_setattr");
    }

    // create socket
    int listen_fd, client_conn_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SRV_LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr*)&address,
             sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // open socket, listen
    if (listen(listen_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    cout << "listening for machines" << endl;
    // accept connections as they come in
    vector<int> worker_pids;
    while (1) {
        if ((client_conn_fd= accept(listen_fd, (struct sockaddr*)&address,
                  &addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        cout << "got conn in " << client_conn_fd << endl;
        
        // immediately fork the workload
        run_workload(&worker_pids);

    }
    close(listen_fd);

    for (int pid : worker_pids) {
        struct rusage usage_stats;
        int ret = wait4(pid, NULL, 0, &usage_stats);
        if (ret < 0) {
            perror("wait4 did bad");
        }
    }
}


int main() {

    run_server();

}
