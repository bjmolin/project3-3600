/*
*	James Williams
*   Bryan Molina Cervantes
*	Assignment 3
*
*/

#ifndef HELPER_H_
#define HELPER_H_

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "Practical.h"

#define MAXSTRINGLENGTH 1024
#define SET_CLK_ID(clk_id) clk_id = CLOCK_REALTIME;

// Global variables for shared use between threads
extern struct sockaddr_storage clntAddr; // Client address storage
extern socklen_t clntAddrLen; // Length of the client address
extern char buffer[MAXSTRINGLENGTH]; // Buffer for incoming and outgoing data
extern int bufferLength; // Actual length of data in buffer
extern pthread_mutex_t lock; // Mutex for synchronizing access to the buffer
extern bool nflag; //no_print flag =false
extern double *rtts; //array of round trip times
extern int packetsSent; //number of packets sent
extern int packetsReceived; //number of packets received
extern int packetCount; //number of packets to send
extern pthread_cond_t cond;

typedef struct {
    int sock;
	int interval;
	int size;
	bool no_print;
    struct addrinfo *addr;
	
} ThreadArgs;

typedef struct{
	struct timespec sendTime;
	struct timespec receiveTime;
	float rtt;
	int seq;
	int dataSize;
} PacketInfo;

//Server Functions
int setupServer(char* service);
void* listenForConnections(void* arg);
void* sendEcho(void* arg);

//Client Functions
int setupClient(const char *server, const char *servPort, struct addrinfo **servAddr);
void* sendPing(void* arg);
void* receiveResponse(void* arg);
int setRecvTimeout(int sockfd, long millisec);
void parsePacket(const char *packet, PacketInfo *info, int packetSize);
void buildHeader(char *buffer, int size, int count);

//Helper Functions
void formatDateTime(double timeInSeconds, char *buffer, int bufferSize);
void PrintLocalIP();

//Statistics Functions
float calculateAverageRTT();
float calculateMaxRTT();
float calculateMinRTT();
void printSummaryStats();

void handle_sigint(int sig);

void timespec_add_us(struct timespec *t, long us);

#endif // HELPER_H_