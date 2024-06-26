/*
*	James Williams
*   Bryan Molina Cervantes
*	Assignment 3
*
*/

#include "helper.h"

// Global variables for shared use between threads
struct sockaddr_storage clntAddr; // Client address storage
socklen_t clntAddrLen = sizeof(clntAddr); // Length of the client address
char buffer[MAXSTRINGLENGTH]; // Buffer for incoming and outgoing data
int bufferLength = 0; // Actual length of data in buffer
pthread_mutex_t lock; // Mutex for synchronizing access to the buffer
pthread_cond_t cond;
bool nflag;//no_print flag =false
double *rtts;//array of round trip times
int packetsSent;//number of packets sent
int packetsReceived;//number of packets received
int packetCount;//number of packets to send

struct timespec startTime, endTime;//for total time calculation


int main(int argc, char **argv) {
    char cflag[30] = "0x7fffffff";
    float iflag = 1.0;
    int pflag = 33333;
    int sflag = 12;
    nflag = false;
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
                //Redundant with the fprintf below
                //fprintf(stderr, "Port\t%10d\n", pflag);
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

    if (serverflag) {
        char service[6];  // Buffer for port number as string
        snprintf(service, sizeof(service), "%d", pflag);  // Convert port number to string
        
        int sock = setupServer(service); // Set up the server and get the socket descriptor
        PrintLocalIP(); // Print the local IP address

        pthread_t threads[2]; // Array to hold thread IDs
        pthread_mutex_init(&lock, NULL); // Initialize the mutex for buffer access synchronization

        // Create two threads for listening and sending
        pthread_create(&threads[0], NULL, listenForConnections, &sock);
        pthread_create(&threads[1], NULL, sendEcho, &sock);

        // Wait for both threads to finish (they won't in this continuous server setup)
        pthread_join(threads[0], NULL);
        pthread_join(threads[1], NULL);
        // Clean up the mutex
        pthread_mutex_destroy(&lock); 
        // Close the server socket
        close(sock); 
        return 0;
    }

    /******CLIENT******/
    // Send and Receive pings
    else {

        //Register the signal handler
        if (signal(SIGINT, handle_sigint) == SIG_ERR) {
            printf("Can't catch SIGINT\n");
        }
        // Get the server address from the command line
        char *ipAddress = NULL;
        if (optind < argc) {
            ipAddress = argv[optind];
        } else {
            DieWithUserMessage("Usage: ", "<Server Address>");
        }
        char server[30];
        strcpy(server, ipAddress);

        // Convert the port number to a string
        char servPort[6];
        snprintf(servPort, sizeof(servPort), "%d", pflag);

        //struct for the server address
        struct addrinfo *servAddr;
        // Set up the client and get the socket descriptor
        int sock = setupClient(server, servPort, &servAddr);
        if (sock < 0) {
            DieWithSystemMessage("Client setup failed");
        }
        // Allocate memory for round trip times
        rtts = malloc(atoi(cflag) * sizeof(int));
        if (rtts == NULL) {
            DieWithSystemMessage("Failed to allocate memory for round trip times");
        }
        // Set up the thread arguments
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (args == NULL) {
            DieWithSystemMessage("Failed to allocate memory for thread arguments");
        }
        // Set the thread arguments
        args->sock = sock;
        args->addr = servAddr;
        args->interval = iflag;
        args->size = sflag;
        args->no_print = nflag;

        packetCount = atoi(cflag);

        // Threads for sending and receiving
        pthread_t threads[2];  
        // Initialize the mutex
        pthread_mutex_init(&lock, NULL);        

        // Start threads
        pthread_create(&threads[0], NULL, sendPing, args);
        pthread_create(&threads[1], NULL, receiveResponse, args);
        
        // Join threads
        pthread_join(threads[0], NULL);
        pthread_join(threads[1], NULL);

        printSummaryStats();

        // Clean up the mutex and free the memory
        pthread_mutex_destroy(&lock); 
        freeaddrinfo(servAddr);
        close(sock);
        exit(0);
    }

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