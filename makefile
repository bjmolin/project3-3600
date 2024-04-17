# Makefile for client and server programs

# Compiler settings
CC = gcc
CFLAGS = -std=gnu99 -pthread -g
RM = rm -f

# Source files
COMMON_SOURCES = helper.c AddressUtility.c DieWithMessage.c
CLIENT_SOURCES = udping.c $(COMMON_SOURCES)

# Object files
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

# Executable names
CLIENT_EXECUTABLE = udping.exe

all: $(CLIENT_EXECUTABLE) $(SERVER_EXECUTABLE) clean

$(CLIENT_EXECUTABLE): $(CLIENT_OBJECTS)
	$(CC) $(CFLAGS) -o $(CLIENT_EXECUTABLE) $(CLIENT_OBJECTS)

clean:
	$(RM) *.o

# Dependency rules for the object files
udping.o: udping.c
helper.o: helper.c
AddressUtility.o: AddressUtility.c
DieWithMessage.o: DieWithMessage.c

