/*
*	James Williams
*   Bryan Molina Cervantes
*	Assignment 3
*
*/

#include "helper.h"

void PrintLocalIP() {
    struct ifaddrs *addrs, *tmp;
    getifaddrs(&addrs); // Get a list of all network interfaces
    tmp = addrs;

    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) { // Check for IPv4 address
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            // Exclude loopback address
            if (strcmp(inet_ntoa(pAddr->sin_addr), "127.0.0.1") != 0) {
                printf("My IP address is: %s\n", inet_ntoa(pAddr->sin_addr));
                break; // Break after printing the first non-loopback IPv4 address
            }
        }
        // Move to the next interface
        tmp = tmp->ifa_next;
    }
    
    freeifaddrs(addrs); // Free the linked list
}

// Function to set up the server: creates socket, binds it, and returns the socket descriptor
//SERVER
int setupServer(char* service) {
    struct addrinfo addrCriteria;                   // Criteria for address match
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out the structure
    addrCriteria.ai_family = AF_UNSPEC;             // Accept any address family
    addrCriteria.ai_flags = AI_PASSIVE;             // For wildcard IP address
    addrCriteria.ai_socktype = SOCK_DGRAM;          // Datagram socket
    addrCriteria.ai_protocol = IPPROTO_UDP;         // UDP protocol

    struct addrinfo *servAddr; // Pointer to the resulting list of server addresses
    int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
    if (rtnVal != 0)
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

    int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
    if (sock < 0)
        DieWithSystemMessage("socket() failed");

    if (bind(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
        DieWithSystemMessage("bind() failed");


    freeaddrinfo(servAddr); // Free the linked list allocated by getaddrinfo()
    return sock;
}

// Thread function to listen for incoming messages
//SERVER
void* listenForConnections(void* arg) {
    int sock = *((int*)arg); // Extract the socket descriptor from argument

    while (1) {
        ssize_t numBytesRcvd = recvfrom(sock, buffer, MAXSTRINGLENGTH, 0,
                                        (struct sockaddr *)&clntAddr, &clntAddrLen);
        if (numBytesRcvd < 0) {
            DieWithSystemMessage("recvfrom() failed");
        } else {
            pthread_mutex_lock(&lock); // Lock the mutex before updating the shared buffer
            bufferLength = numBytesRcvd; // Set the global buffer length to the number of received bytes
            pthread_mutex_unlock(&lock); // Unlock the mutex
        }
    }
    return NULL; // This will never reach here due to the infinite loop
}

// Thread function to send responses
//SERVER
void* sendEcho(void* arg) {
    int sock = *((int*)arg); // Extract the socket descriptor from argument

    while (1) {
        pthread_mutex_lock(&lock); // Ensure exclusive access to the buffer
        if (bufferLength > 0) {
            ssize_t numBytesSent = sendto(sock, buffer, bufferLength, 0,
                                          (struct sockaddr *)&clntAddr, clntAddrLen);
            if (numBytesSent < 0)
                DieWithSystemMessage("sendto() failed");
            else if (numBytesSent != bufferLength)
                DieWithUserMessage("sendto()", "sent unexpected number of bytes");

            bufferLength = 0; // Reset the buffer length after sending
        }
        pthread_mutex_unlock(&lock); // Release the lock
    }
    return NULL; // This will never reach here due to the infinite loop
}

// Function to set up the client: creates socket, gets server address, and returns the socket descriptor
//CLIENT
int setupClient(const char *server, const char *servPort, struct addrinfo **servAddr) {
    struct addrinfo addrCriteria;
    memset(&addrCriteria, 0, sizeof(addrCriteria));
    addrCriteria.ai_family = AF_UNSPEC;
    addrCriteria.ai_socktype = SOCK_DGRAM;
    addrCriteria.ai_protocol = IPPROTO_UDP;

    int rtnVal = getaddrinfo(server, servPort, &addrCriteria, servAddr);

    if (rtnVal != 0)
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

    int sock = socket((*servAddr)->ai_family, (*servAddr)->ai_socktype, (*servAddr)->ai_protocol);
    if (sock < 0)
        DieWithSystemMessage("socket() failed");

    return sock;
}


// Thread function to send pings to the server
//CLIENT
//Refactored does not currently work
void* sendPing(void* arg) {
    ThreadArgs *args = (ThreadArgs *) arg;
    int sock = args->sock;
    struct addrinfo *servAddr = args->addr;
	int headerSize = 30;
    int packetSize = args->size + headerSize; // Size of the packet

    char *pingBuffer = malloc(packetSize);
    if (pingBuffer == NULL) {
    	DieWithSystemMessage("Failed to allocate memory for ping buffer");
    }

    int count = 0;
    while (count < packetCount) {
        pthread_mutex_lock(&lock);

        // Build the packet header and data
        buildHeader(pingBuffer, packetSize, count);

        // Send the prepared packet
        ssize_t numBytes = sendto(sock, pingBuffer, strlen(pingBuffer), 0,
                                  servAddr->ai_addr, servAddr->ai_addrlen);
        if (numBytes < 0) {
			free(pingBuffer);
			pthread_mutex_unlock(&lock);
            DieWithSystemMessage("sendto() failed");
        }

		count++;
		//Store the number of packets sent in the global variable
		packetsSent = count;
        pthread_mutex_unlock(&lock);

		//Uncomment this line to see the packets being sent
        //printf("Packet %d sent, size: %d\n", count, packetSize);

        usleep(500000); // Sleep for 0.5 seconds
    }

    free(pingBuffer);
    return NULL;
}

// Thread function to receive responses from the server
//CLIENT
//Currently works, with parsePacket function, 
//but parsePacket function is not testable because buildHeader function is not working
void* receiveResponse(void* arg) {
	ThreadArgs *args = (ThreadArgs *) arg;
    int sock = args->sock;
    struct addrinfo *servAddr = args->addr;
	int count = 0;
	int numPackets = 0; // Counter for received packets
	struct timespec recvTime; // Time at which the packet was received
	
	// Set the receive timeout to 2 seconds
	setRecvTimeout(sock, 2000); 

    while (true) {
		//Create a 
        struct sockaddr_storage fromAddr;
		//Get the length of the address
        socklen_t fromAddrLen = sizeof(fromAddr);
		//Create a PacketInfo structure to store the received packet information
		PacketInfo info;
		//Allocate memory for the temporary string
		char *tempString = (char*) malloc(MAXSTRINGLENGTH);
		if (tempString == NULL) {
			DieWithSystemMessage("Failed to allocate memory for temporary string");
		}

        // Receive a response
        ssize_t numBytes = recvfrom(sock, tempString, MAXSTRINGLENGTH, 0,
                                    (struct sockaddr *) &fromAddr, &fromAddrLen);

		//Handle the case where the recvfrom() function times out
        if (numBytes < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
            	printf("recvfrom() timed out.\n");
			} else
            	DieWithSystemMessage("recvfrom() failed");
		//Handle the case where the packet is received from an unknown source
        } else if (!SockAddrsEqual(servAddr->ai_addr, (struct sockaddr *) &fromAddr)) {
            DieWithUserMessage("recvfrom()", "received a packet from unknown source");
        }

		//Handle the case where the packet is received successfully
		else if (numBytes > 0){
			numPackets++;
			//printf("Received: %s Size: %ld\n", tempString, numBytes);
		}

		// Parse the received packet
		parsePacket(tempString, &info, sizeof(tempString));

		//Lock the mutex
		pthread_mutex_lock(&lock);
		//Store the number of packets received in the global variable
		packetsReceived = numPackets;
		//copy the received packet to the global buffer
		//not currently used, but nice to have
		strcpy(buffer, tempString);
		free(tempString);
        buffer[numBytes] = '\0';  // Null-terminate received data
        bufferLength = numBytes;
		//Unlock the mutex
        pthread_mutex_unlock(&lock);
		count++;
		if(count >= packetCount)
			break;
		
    }
	
    return NULL;
}

// Function to set the receive timeout for a socket
int setRecvTimeout(int sockfd, long millisec) {
    struct timeval timeout;
    timeout.tv_sec = millisec / 1000;           // Seconds
    timeout.tv_usec = (millisec % 1000) * 1000; // Microseconds

    // Set the receive timeout
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        DieWithSystemMessage("setsockopt(SO_RCVTIMEO) failed");
        return -1;
    }
    return 0;
}

void formatDateTime(double timeInSeconds, char *buffer, int bufferSize) {
    // Convert timeInSeconds to time_t, truncating to remove fractional seconds if any
    time_t rawTime = (time_t)timeInSeconds;

    // Obtain the local time as a struct
    struct tm *timeInfo;
    timeInfo = localtime(&rawTime);

    // Format the time into a string according to the desired format
    // Here using the format YYYY-MM-DD HH:MM:SS, you can customize it as needed
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", timeInfo);
}

// Function to build the packet header and fill the data
//CLIENT
//Currently not working
void buildHeader(char *buffer, int size, int count) {
    struct timespec sendTime;

	
	clockid_t clk_id = CLOCK_REALTIME;
	//if (clock_gettime(clk_id, &sendTime) != 0)//segfaults for some reason

    // Get current time
    if (clock_gettime(clk_id, &sendTime) != 0) {
        DieWithSystemMessage("clock_gettime() failed");
    }

    // Convert time to microseconds
    long timeInMicroseconds = sendTime.tv_sec * 1000000 + sendTime.tv_nsec / 1000;

    // Format the buffer to include the count and the time with specified tags
    int headerLength = snprintf(buffer, size, "s:%d:t:%ld:d:", count+1, timeInMicroseconds);
    if (headerLength < 0) {
        DieWithSystemMessage("snprintf() failed");
    }

    // Fill the rest of the buffer with 'A'
    memset(buffer + headerLength, 'A', size - headerLength);
}

//Function to parse the received packet and extract the sequence number, timestamp, and data
//CLIENT
//Currently working but not testable because buildHeader function is not working
void parsePacket(const char *packet, PacketInfo *info, int packetSize) {
    int seqNum;
    long sendTimeInMicroseconds;
    char *data = (char *) malloc(packetSize);
	clockid_t clk_id;

    // Extract sequence number, timestamp, and data
    if (sscanf(packet, "s:%d:t:%ld:d:%s", &seqNum, &sendTimeInMicroseconds, data) == 3) {
        

		// Store the extracted values in the PacketInfo structure
		// Convert sendTimeInMicroseconds to timespec
        info->sendTime.tv_sec = sendTimeInMicroseconds / 1000000;
        info->sendTime.tv_nsec = (sendTimeInMicroseconds % 1000000) * 1000;

        // Assume we immediately process the packet upon reception for simplicity
        clock_gettime(clk_id, &info->receiveTime);

        // Calculate Round Trip Time in milliseconds
        info->rtt = (info->receiveTime.tv_sec - info->sendTime.tv_sec) * 1000.0
                    + (info->receiveTime.tv_nsec - info->sendTime.tv_nsec) / 1000000.0;

        info->dataSize = sizeof(data);

		if (!nflag){// Print the calculated values
			//printf("%d\t%d\t%.4f", seqNum, info->dataSize, info->rtt);
			printf("%-10d%-10d%-10.4f\n", seqNum, info->dataSize, info->rtt);

			//My perfereed way to print the data
			//printf("Sequence Number: %d\n", seqNum);
        	//printf("Timestamp (microseconds): %ld\n", sendTimeInMicroseconds);
        	//printf("Data: %s\n", data);
			//printf("Round Trip Time: %.3f ms\n", info->rtt);
			//printf("Data Size: %d\n", info->dataSize);
		}
		//Lock the mutex
		pthread_mutex_lock(&lock);
		//Store the RTT in the global array
		rtts[seqNum-1] = info->rtt;
		//Unlock the mutex
		pthread_mutex_unlock(&lock);

    } else {
        printf("Failed to parse the packet\n");
    }
}

float calculateAverageRTT(){
	float sum = 0;
	for(int i = 0; i < packetsReceived; i++){
		sum += rtts[i];
	}
	return sum/packetsReceived;
}

float calculateMaxRTT(){
	float max = 0;
	for(int i = 0; i < packetsReceived; i++){
		if(rtts[i] > max){
			max = rtts[i];
		}
	}
	return max;
}

float calculateMinRTT(){
	float min = rtts[0];
	for(int i = 0; i < packetsReceived; i++){
		if(rtts[i] < min){
			min = rtts[i];
		}
	}
	return min;
}

float calculateTotalRTT(){
	float sum = 0;
	for(int i = 0; i < packetsReceived; i++){
		sum += rtts[i];
	}
	return sum;
}

void printSummaryStats(){
	float avg = calculateAverageRTT();
	float max = calculateMaxRTT();
	float min = calculateMinRTT();
	float total = calculateTotalRTT();
	float packetLossPercent = 100- ((packetsSent/packetsReceived)*100);
	printf("%d packets transmitted, %d packets received, %.2f%% packet loss, time %.3f ms\n", 
		packetsSent, packetsReceived, packetLossPercent, total);
	printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", min, avg, max);
}

// Function to be called when SIGINT is received
void handle_sigint(int sig) {
    printf("Caught signal %d\n", sig);
    printf("Printing statistics before exiting...\n");
    
    // Print the summary statistics
	printSummaryStats();

    // Exit the program
    exit(0);
}



