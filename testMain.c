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

// Main function to test implementation of threads.
int main(int argc, char *argv[]) {
    if (argc != 2) // Ensure the correct number of command-line arguments
        DieWithUserMessage("Parameter(s)", "<Server Port/Service>");

    char *service = argv[1]; // Service/port number provided as a command-line argument
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

    pthread_mutex_destroy(&lock); // Clean up the mutex
    close(sock); // Close the server socket
    return 0;
}