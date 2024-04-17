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
	
	struct addrinfo *result;
	for (result = *servAddr; result != NULL; result = result->ai_next) {
		struct sockaddr_in *addr = (struct sockaddr_in *) result->ai_addr;
		printf("setupClient(Family: %d - IP: %s - Port: %d)\n",
			addr->sin_family,
			inet_ntoa(addr->sin_addr),
			ntohs(addr->sin_port));
	}

    if (rtnVal != 0)
        DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

    int sock = socket((*servAddr)->ai_family, (*servAddr)->ai_socktype, (*servAddr)->ai_protocol);
    if (sock < 0)
        DieWithSystemMessage("socket() failed");

    return sock;
}

// Thread function to send pings to the server
//CLIENT
void* sendPing(void* arg) {
    ThreadArgs *args = (ThreadArgs *) arg;
    int sock = args->sock;
    struct addrinfo *servAddr = args->addr;

	struct addrinfo *result;
	for (result = servAddr; result != NULL; result = result->ai_next) {
		struct sockaddr_in *addr = (struct sockaddr_in *) result->ai_addr;
		printf("sendPing( Family: %d - IP: %s - Port: %d)\n",
			addr->sin_family,
			inet_ntoa(addr->sin_addr),
			ntohs(addr->sin_port));
	}

    while (true) {  // Consider using a condition for more control
        pthread_mutex_lock(&lock);
        if (bufferLength > 0) {
            ssize_t numBytes = sendto(sock, buffer, bufferLength, 0,
                                      servAddr->ai_addr, servAddr->ai_addrlen);
            if (numBytes < 0) {
                DieWithSystemMessage("sendto() failed");
            } else if (numBytes != bufferLength) {
                DieWithUserMessage("sendto() error", "sent unexpected number of bytes");
            }
            bufferLength = 0;  // Reset buffer length after sending
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// Thread function to receive responses from the server
//CLIENT
void* receiveResponse(void* arg) {
    int sock = *((int*) arg);
    struct addrinfo *servAddr = (struct addrinfo *) arg;

    while (true) {
        struct sockaddr_storage fromAddr;
        socklen_t fromAddrLen = sizeof(fromAddr);
        pthread_mutex_lock(&lock);
        ssize_t numBytes = recvfrom(sock, buffer, MAXSTRINGLENGTH, 0,
                                    (struct sockaddr *) &fromAddr, &fromAddrLen);
        if (numBytes < 0) {
            DieWithSystemMessage("recvfrom() failed");
        } else if (!SockAddrsEqual(servAddr->ai_addr, (struct sockaddr *) &fromAddr)) {
            DieWithUserMessage("recvfrom()", "received a packet from unknown source");
        }
        buffer[numBytes] = '\0';  // Null-terminate received data
        bufferLength = numBytes;
        pthread_mutex_unlock(&lock);
        printf("Received: %s\n", buffer);
    }
    return NULL;
}



