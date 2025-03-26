#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_NAME 50
#define MAX_DATA 256
#define BUF_SIZE 512

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
#define MSG_REGISTER    14
#define MSG_REG_ACK     15
#define MSG_REG_NAK     16

// The message structure used between client and server.
struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

int sockfd = -1;             // Global socket descriptor
volatile bool logged_in = false;  // Client login state

// Helper function to send a message over the socket.
void send_message(unsigned int type, const char *clientID, const char *data) {
    struct message msg;
    msg.type = type;
    msg.size = data ? strlen(data) : 0;
    memset(msg.source, 0, MAX_NAME);
    strncpy((char *)msg.source, clientID, MAX_NAME - 1);
    memset(msg.data, 0, MAX_DATA);
    if (data)
        strncpy((char *)msg.data, data, MAX_DATA - 1);
    if (sockfd != -1) {
        send(sockfd, &msg, sizeof(msg), 0);
    }
}

// Thread function to continuously receive messages from the server.
void *receiver_thread(void *arg) {
    (void)arg;  // Suppress unused parameter warning
    struct message msg;
    while (1) {
        int n = recv(sockfd, &msg, sizeof(msg), 0);
        if (n <= 0) {
            printf("Disconnected from server or error occurred.\n");
            logged_in = false;
            break;
        }
        // For simplicity, we just print the received message type and data.
        switch (msg.type) {
            case MSG_LO_ACK:
                printf("[Server] Login successful.\n");
                logged_in = true;
                break;
            case MSG_REG_ACK:
                printf("[Server] User registration successful. Logged in.\n");
                logged_in = true;
                break;
            case MSG_NS_ACK:
                printf("[Server] New session created successfully: %s\n", msg.data);
                break;
            case MSG_JN_ACK:
                printf("[Server] Joined session: %s\n", msg.data);
                break;
            case MSG_JN_NAK:
                printf("[Server] Session does not exist: %s\n", msg.data);
                break;
            case MSG_QU_ACK:
                printf("[Server] List:\n%s\n", msg.data);
                break;
            case MSG_MESSAGE:
                // Display broadcast message with sender ID.
                printf("[%s] %s\n", msg.source, msg.data);
                break;
            case MSG_LO_NAK:
                printf("[Server] Login NOT successful.\n");
                logged_in = false;
                break;
            case MSG_REG_NAK:
                printf("[Server] User registration NOT successful. Username taken. Try again.\n");
                logged_in = false;
                break;
            default:
                printf("[Server] Type %u: %s\n", msg.type, msg.data);
        }
    }
    return NULL;
}

int main() {
    char input[BUF_SIZE];
    char clientID[MAX_NAME] = {0};
    pthread_t recv_tid;

    printf("Welcome to the Text Conferencing Client.\n");
    printf("Commands:\n");
    printf("  /register <clientID> <password> <server-IP> <server-port>\n");
    printf("  /login <clientID> <password> <server-IP> <server-port>\n");
    printf("  /logout\n");
    printf("  /createsession <sessionID>\n");
    printf("  /joinsession <sessionID>\n");
    printf("  /leavesession\n");
    printf("  /list\n");
    printf("  /quit\n");
    printf("  <message text> (any text not starting with '/')\n");

    while (1) {
        //printf(">>");
        if (!fgets(input, BUF_SIZE, stdin))
            break;
        // Remove trailing newline.
        input[strcspn(input, "\n")] = '\0';

        // Parse the input command.
        if (strncmp(input, "/login", 6) == 0) {
            if (logged_in) {
                printf("Already logged in.\n");
                continue;
            }
            // Expected format: /login <clientID> <password> <server-IP> <server-port>
            char password[50], serverIP[50];
            int port;
            if (sscanf(input, "/login %s %s %s %d", clientID, password, serverIP, &port) != 4) {
                printf("Invalid login command format.\n");
                continue;
            }
            // Create socket.
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("Socket creation failed");
                continue;
            }
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);
            serv_addr.sin_addr.s_addr = inet_addr(serverIP);
            if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection failed");
                close(sockfd);
                sockfd = -1;
                continue;
            }
            // Send LOGIN message (password is sent as data).
            send_message(MSG_LOGIN, clientID, password);
            //logged_in = true;
            // Start the receiver thread.
            pthread_create(&recv_tid, NULL, receiver_thread, NULL);
        }
        else if (strncmp(input, "/register", 9) == 0) {
            if (logged_in) {
                printf("Already logged in. Log out to register new user.\n");
                continue;
            }
            // Expected format: /register <clientID> <password> <server-IP> <server-port>
            char password[50], serverIP[50];
            int port;
            if (sscanf(input, "/register %s %s %s %d", clientID, password, serverIP, &port) != 4) {
                printf("Invalid register command format.\n");
                continue;
            }
            // Create socket.
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("Socket creation failed");
                continue;
            }
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);
            serv_addr.sin_addr.s_addr = inet_addr(serverIP);
            if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection failed");
                close(sockfd);
                sockfd = -1;
                continue;
            }
            // Send REGISTER message (password is sent as data).
            send_message(MSG_REGISTER, clientID, password);
            //logged_in = true;
            // Start the receiver thread.
            pthread_create(&recv_tid, NULL, receiver_thread, NULL);
        }
        else if (strncmp(input, "/logout", 7) == 0) {
            if (!logged_in) {
                printf("Not logged in.\n");
                continue;
            }
            send_message(MSG_EXIT, clientID, "");
            logged_in = false;
            close(sockfd);
            sockfd = -1;
            printf("Logged out.\n");
        }
        else if (strncmp(input, "/createsession", 14) == 0) {
            if (!logged_in) {
                printf("Please log in first.\n");
                continue;
            }
            // Command: /createsession <sessionID>
            char sessionID[MAX_NAME] = {0};
            if (sscanf(input, "/createsession %s", sessionID) != 1) {
                printf("Invalid /createsession format.\n");
                continue;
            }
            send_message(MSG_NEW_SESS, clientID, sessionID);
        }
        else if (strncmp(input, "/joinsession", 12) == 0) {
            if (!logged_in) {
                printf("Please log in first.\n");
                continue;
            }
            char sessionID[MAX_NAME] = {0};
            if (sscanf(input, "/joinsession %s", sessionID) != 1) {
                printf("Invalid /joinsession format.\n");
                continue;
            }
            send_message(MSG_JOIN, clientID, sessionID);
        }
        else if (strncmp(input, "/leavesession", 13) == 0) {
            if (!logged_in) {
                printf("Please log in first.\n");
                continue;
            }
            send_message(MSG_LEAVE, clientID, "");
        }
        else if (strncmp(input, "/list", 5) == 0) {
            if (!logged_in) {
                printf("Please log in first.\n");
                continue;
            }
            send_message(MSG_QUERY, clientID, "");
        }
        else if (strncmp(input, "/quit", 5) == 0) {
            if (logged_in) {
                send_message(MSG_EXIT, clientID, "");
                close(sockfd);
            }
            printf("Exiting client.\n");
            break;
        }
        else {
            // Any other text is considered a message to the current session.
            if (!logged_in) {
                printf("Please log in first.\n");
                continue;
            }
            send_message(MSG_MESSAGE, clientID, input);
        }
    }
    return 0;
}