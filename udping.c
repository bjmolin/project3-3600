/*
*	James Williams
*   Bryan Molina Cervantes
*	Assignment 3
*
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h> 
// #include "helper.c"

int main(int argc, char **argv) {
    char cflag[30] = "0x7fffffff";
    float iflag = 1.0;
    int pflag = 33333;
    int sflag = 12;
    bool nflag = false; 
    bool serverflag = false;
    char *cvalue = NULL;
    int index;
    int c;
    
    opterr = 0;
    // c:i:p:s:n contains the flags we will use but I don't recall 
    // what the colons mean (they're supposedly used to specify required flags)
    // Additionally, everything is sent to the stderr as specified in the doc
    while ((c = getopt(argc, argv, "c:i:p:s:nS")) != -1) { 
        switch (c) {
            case 'c': // ping-packet-count
                snprintf(cflag, sizeof(cflag), "%s", optarg);
                fprintf(stderr, "Count\t%10s\n", cflag);
                break;
            case 'i': // ping-interval
                iflag = atof(optarg);
                fprintf(stderr, "Interval%10.3f\n", iflag);
                break;
            case 'p': // port number
                pflag = atoi(optarg);
                fprintf(stderr, "Port\t%10d\n", pflag);
                break;
            case 's': // size in bytes
                sflag = atoi(optarg);
                fprintf(stderr, "Size\t%10d\n", sflag);
                break;
            case 'n': // no_print
                nflag = true;
                // fprintf(stderr, "nflag\t%10d\n");
                break;
            case 'S':
                serverflag = true;
                break;
            case '?':
                if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }
    }

    for (index = optind; index < argc; index++)
        printf("Non-option argument %s\n", argv[index]);

    fprintf(stderr, "Running in %s mode\n", serverflag ? "server" : "client");
    fprintf(stderr, "Port: %d\n", pflag);

    /******SERVER******/
    // Receive and Send pings
    /*
    if (serverflag) {
        for (i = 0; i < atoi(cflag); i++) {
            ReceivePing(sock); // Function prints out with stdout
            SendPing(sock, address); 
        }
    }
    */

    /******CLIENT******/
    // Send and Receive pings
    /*
    else {
        for (i = 0; i < atoi(cflag); i++) {
            SendPing(sock, address);
            ReceivePing(sock); // Function prints out with stdout
        }
    }
    */

   

    return 0;
}

    /*

    int seqNumber;
    
    time_t timestamp;
    time_t currTime;

    // Print ping details
    
    time(&timestamp);
    time(&currentTime)
    double roundTripTime = drifttime(currTime, timestamp)
    */