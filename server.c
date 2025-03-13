#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_NAME 50
#define MAX_DATA 256
#define MAX_CLIENTS 20
#define MAX_USERS 20
#define MAX_PASSWORD 50

// Message type definitions
#define MSG_LOGIN       1
#define MSG_LO_ACK      2
#define MSG_LO_NAK      3
#define MSG_EXIT        4
#define MSG_NEW_SESS    5
#define MSG_NS_ACK      6
#define MSG_JOIN        7
#define MSG_JN_ACK      8
#define MSG_JN_NAK      9
#define MSG_LEAVE       10
#define MSG_QUERY       11
#define MSG_QU_ACK      12
#define MSG_MESSAGE     13
#define MSG_LO_NACK    14

// The message structure used between client and server.
struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

// Structure to hold user credentials
typedef struct {
    char username[MAX_NAME];
    char password[MAX_PASSWORD];
} User;

// Global array of users
User users[MAX_USERS];

// Function to initialize users
void initialize_users() {
    strcpy(users[0].username, "alice");     strcpy(users[0].password, "password1");
    strcpy(users[1].username, "bob");       strcpy(users[1].password, "secure123");
    strcpy(users[2].username, "charlie");   strcpy(users[2].password, "qwerty");
    strcpy(users[3].username, "david");     strcpy(users[3].password, "letmein");
    strcpy(users[4].username, "emma");      strcpy(users[4].password, "hello123");
    strcpy(users[5].username, "frank");     strcpy(users[5].password, "hunter2");
    strcpy(users[6].username, "grace");     strcpy(users[6].password, "iloveyou");
    strcpy(users[7].username, "harry");     strcpy(users[7].password, "abc123");
    strcpy(users[8].username, "isabel");    strcpy(users[8].password, "password");
    strcpy(users[9].username, "jack");      strcpy(users[9].password, "welcome");
}

// Structure to hold client information.
typedef struct {
    int sockfd;
    char clientID[MAX_NAME];
    char sessionID[MAX_NAME];  // empty string if not in a session
} client_t;

client_t clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper function to send a message to a client.
void send_to_client(int sockfd, unsigned int type, const char *clientID, const char *data) {
    struct message msg;
    msg.type = type;
    msg.size = data ? strlen(data) : 0;
    memset(msg.source, 0, MAX_NAME);
    strncpy((char *)msg.source, clientID, MAX_NAME - 1);
    memset(msg.data, 0, MAX_DATA);
    if (data)
        strncpy((char *)msg.data, data, MAX_DATA - 1);
    send(sockfd, &msg, sizeof(msg), 0);
}

// Broadcast a MESSAGE to all clients in the same session (except the sender).
void broadcast_message(const char *sessionID, const char *senderID, const struct message *msg) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].sessionID, sessionID) == 0 &&
            strcmp(clients[i].clientID, senderID) != 0) {
            send(clients[i].sockfd, msg, sizeof(*msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove a client from the global list.
void remove_client(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sockfd == sockfd) {
            // Shift the array down.
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Thread function to handle communication with a client.
void *handle_client(void *arg) {
    int sockfd = *(int *)arg;
    struct message msg;
    char currentSession[MAX_NAME] = "";
    char currentClientID[MAX_NAME] = "";

    while (1) {
        int n = recv(sockfd, &msg, sizeof(msg), 0);
        if (n <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        switch (msg.type) {
            case MSG_LOGIN:
                bool login = false;
                pthread_mutex_lock(&clients_mutex);
                {
                    client_t newClient;
                    newClient.sockfd = sockfd;
                    strncpy(newClient.clientID, (char *)msg.source, MAX_NAME - 1);
                    newClient.clientID[MAX_NAME - 1] = '\0';
                    char passwordClient[MAX_PASSWORD];
                    strncpy(passwordClient, (char*) msg.data, MAX_NAME - 1);
                    newClient.sessionID[0] = '\0'; // Not in any session initially.
                    strncpy(currentClientID, newClient.clientID, MAX_NAME - 1);
                    for (int i = 0; i < MAX_USERS; i++) {
                        if (strcmp(users[i].username, newClient.clientID) == 0 && 
                        strcmp(users[i].password, passwordClient) == 0) {
                            clients[client_count++] = newClient;
                            login = true;
                        }
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                if (login == true) {
                    printf("Client %s logged in.\n", msg.source);
                    send_to_client(sockfd, MSG_LO_ACK, (char *)msg.source, "");
                } else {
                    printf("Client %s login failed.\n", msg.source);
                    send_to_client(sockfd, MSG_LO_NACK, (char *)msg.source, "");
                }
                    
                break;
            case MSG_EXIT:
                printf("Client %s logged out.\n", msg.source);
                send_to_client(sockfd, MSG_EXIT, (char *)msg.source, "");
                goto cleanup;
                break;
            case MSG_NEW_SESS:
                // Create new session; set client's sessionID.
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    if (clients[i].sockfd == sockfd) {
                        strncpy(clients[i].sessionID, (char *)msg.data, MAX_NAME - 1);
                        strncpy(currentSession, clients[i].sessionID, MAX_NAME - 1);
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                printf("Client %s created session %s.\n", msg.source, msg.data);
                send_to_client(sockfd, MSG_NS_ACK, (char *)msg.source, (const char *)msg.data);
                break;
            case MSG_JOIN:
                // Client wants to join an existing session.
                pthread_mutex_lock(&clients_mutex);
                {
                    int i;
                    // check if session exists
                    for (i = 0; i < client_count; i++) {
                        if (strcmp(clients[i].sessionID, (char *)msg.data) == 0) {
                            // check if client is in a session
                            for (int i = 0; i < client_count; i++) {
                                if (clients[i].sockfd == sockfd) {
                                    if (strlen(clients[i].sessionID) > 0) {
                                        // Already in a session; reject join.
                                        send_to_client(sockfd, MSG_JN_NAK, (char *)msg.source, "Already in a session.");
                                        pthread_mutex_unlock(&clients_mutex);
                                        goto next_iteration;
                                    } else {
                                        // not in a session and session exists then join
                                        strncpy(clients[i].sessionID, (char *)msg.data, MAX_NAME - 1);
                                        strncpy(currentSession, clients[i].sessionID, MAX_NAME - 1);
                                        send_to_client(sockfd, MSG_JN_ACK, (char *)msg.source, clients[i].sessionID);
                                        pthread_mutex_unlock(&clients_mutex);
                                        goto next_iteration;
                                    }
                                }
                            }
                            break;
                        }
                    }
                    send_to_client(sockfd, MSG_JN_NAK, (char *)msg.source, "Session not available.");
                }
                pthread_mutex_unlock(&clients_mutex);
                break;
            case MSG_LEAVE:
                // Remove client from session.
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    if (clients[i].sockfd == sockfd) {
                        clients[i].sessionID[0] = '\0';
                        currentSession[0] = '\0';
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                printf("Client %s left the session.\n", msg.source);
                break;
            case MSG_QUERY:
                // Build list of online clients and their session IDs.
                {
                    char listBuf[1024] = "";
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < client_count; i++) {
                        char entry[120];
                        snprintf(entry, sizeof(entry), "Client: %s, Session: %s\n",
                                 clients[i].clientID,
                                 (strlen(clients[i].sessionID) > 0) ? clients[i].sessionID : "None");
                        strncat(listBuf, entry, sizeof(listBuf) - strlen(listBuf) - 1);
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    send_to_client(sockfd, MSG_QU_ACK, (char *)msg.source, listBuf);
                }
                break;
            case MSG_MESSAGE:
                // Broadcast the message to all clients in the same session.
                if (strlen(currentSession) == 0) {
                    // If the sender is not in a session, ignore the message.
                    send_to_client(sockfd, MSG_MESSAGE, (char *)msg.source, "Not in a session.");
                } else {
                    printf("Broadcasting message from %s in session %s: %s\n", msg.source, currentSession, msg.data);
                    broadcast_message(currentSession, (char *)msg.source, &msg);
                }
                break;
            default:
                printf("Unknown message type from %s.\n", msg.source);
        }
next_iteration:
        ;
    }
cleanup:
    close(sockfd);
    remove_client(sockfd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    initialize_users();
    int port = atoi(argv[1]);
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create the server socket.
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", port);

    // Accept clients.
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        pthread_t tid;
        // Create a thread for each new client.
        pthread_create(&tid, NULL, handle_client, (void *)&client_sock);
    }

    close(server_sock);
    return 0;
}