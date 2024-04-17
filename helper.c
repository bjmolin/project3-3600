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
    while (count < args->packet_count) {
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
        pthread_mutex_unlock(&lock);
        printf("Packet %d sent, size: %d\n", count, packetSize);

        //usleep(500000); // Sleep for 0.5 seconds
    }

    printf("Total packets sent: %d\n", count);
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
        struct sockaddr_storage fromAddr;
        socklen_t fromAddrLen = sizeof(fromAddr);
		PacketInfo info;
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
			printf("Received: %s Size: %ld\n", tempString, numBytes);
		}

		// Parse the received packet
		parsePacket(tempString, &info, strlen(tempString));

		pthread_mutex_lock(&lock);

		strcpy(buffer, tempString);
		free(tempString);
        buffer[numBytes] = '\0';  // Null-terminate received data
        bufferLength = numBytes;
		
        pthread_mutex_unlock(&lock);
		count++;
		if(count == args->packet_count)
			break;
    }
	printf("Packets received: %d\n", numPackets);
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
    int headerLength = snprintf(buffer, size, "s:%d:t:%ld:d:", count, timeInMicroseconds);
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
        printf("Sequence Number: %d\n", seqNum);
        printf("Timestamp (microseconds): %ld\n", sendTimeInMicroseconds);
        printf("Data: %s\n", data);

		// Store the extracted values in the PacketInfo structure
		// Convert sendTimeInMicroseconds to timespec
        info->sendTime.tv_sec = sendTimeInMicroseconds / 1000000;
        info->sendTime.tv_nsec = (sendTimeInMicroseconds % 1000000) * 1000;

        // Assume we immediately process the packet upon reception for simplicity
        clock_gettime(clk_id, &info->receiveTime);

        // Calculate Round Trip Time in milliseconds
        info->rtt = (info->receiveTime.tv_sec - info->sendTime.tv_sec) * 1000.0
                    + (info->receiveTime.tv_nsec - info->sendTime.tv_nsec) / 1000000.0;

        info->dataSize = strlen(data);

		// Print the calculated values
		printf("Round Trip Time: %.3f ms\n", info->rtt);
		printf("Data Size: %d\n", info->dataSize);

    } else {
        printf("Failed to parse the packet\n");
    }
}



