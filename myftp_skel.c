#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 512

/**
 * function: receive and analize the answer from the server
 * sd: socket descriptor
 * code: three leter numerical code to check if received
 * text: normally NULL but if a pointer if received as parameter
 *       then a copy of the optional message from the response
 *       is copied
 * return: result of code checking
 **/
bool recv_msg(int sd, int code, char *text) {
    char buffer[BUFSIZE], message[BUFSIZE];
    int recv_s, recv_code;

    // receive the answer
    recv_s = recv(sd, buffer, BUFSIZE, 0);

    // error checking
    if (recv_s < 0) warn("Error receiving data.\m");
    if (recv_s == 0) errx(1, "Connection closed by host.\n");

    // parsing the code and message receive from the answer
    sscanf(buffer, "%d %[^\r\n]\r\n", &recv_code, message);
    printf("%d %s\n", recv_code, message);

    // optional copy of parameters
    if(text) strcpy(text, message);

    // boolean test for the code
    return (code == recv_code) ? true : false;
}

/**
 * function: send command formated to the server
 * sd: socket descriptor
 * operation: four letters command
 * param: command parameters
 **/
void send_msg(int sd, char *operation, char *param) {
    char buffer[BUFSIZE] = "";

    // command formating
    if (param != NULL)
        sprintf(buffer, "%s %s\r\n", operation, param);
    else
        sprintf(buffer, "%s\r\n", operation);

    // send command and check for errors
    if (send(sd, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending data.\n");
    }
}

/**
 * function: simple input from keyboard
 * return: input without ENTER key
 **/
char * read_input() {
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin)) {
        return strtok(input, "\n");
    }
    return NULL;
}

/**
 * function: login process from the client side
 * sd: socket descriptor
 **/
void authenticate(int sd) {
    char *input, desc[100];
    int code;

    // ask for user
    printf("username: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "USER", input);

    // relese memory
    free(input);

    // wait to receive password requirement and check for errors
    if (!recv_msg(sd, 331, desc)) {
        printf("Failed to receive password from server.\n");
        return;
    }

    // ask for password
    printf("passwd: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "PASS", input);

    // release memory
    free(input);

    // wait for answer and process it and check for errors
    if (!recv_msg(sd, 230, desc)) {
        printf("Auth failed. User could not log in.\n");
    }
}

/**
 * function: operation get
 * sd: socket descriptor
 * file_name: file name to get from the server
 **/
void get(int sd, char *file_name) {
    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    FILE *file;

    // send the RETR command to the server
    send_msg(sd, "RETR", file_name);

    // check for the response
    if (!recv_msg(sd, 550, desc)) {
        printf("Couldn't find file or directory.\n");
        return;
    }

    // parsing the file size from the answer received
    // "File %s size %ld bytes"
    sscanf(buffer, "File %*s size %d bytes", &f_size);

    // open the file to write
    file = fopen(file_name, "w");

    //receive the file
    while ((recv_s = recv(sd, buffer, r_size, 0)) > 0) {
        fwrite(buffer, sizeof(char), recv_s, file);
    }    

    // close the file
    fclose(file);

    // receive the OK from the server
    if (!recv_msg(sd, 226, desc)) {
        printf("Transfer did not finish.\n");
        return;
    }
}

/**
 * function: operation quit
 * sd: socket descriptor
 **/
void quit(int sd) {
    char desc[100];

    // send command QUIT to the client
    send_msg(sd, "QUIT", NULL);

    // receive the answer from the server
    if (!recv_msg(sd, 221, desc)) {
        printf("Failed to receive infromation from server.\n");
        return;
    }
}

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(int sd) {
    char *input, *op, *param;

    while (true) {
        printf("Operation: ");
        input = read_input();
        if (input == NULL)
            continue; // avoid empty input
        op = strtok(input, " ");
        // free(input);
        if (strcmp(op, "get") == 0) {
            param = strtok(NULL, " ");
            get(sd, param);
        }
        else if (strcmp(op, "quit") == 0) {
            quit(sd);
            break;
        }
        else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}

/**
 * Run with
 *         ./myftp <SERVER_IP> <SERVER_PORT>
 **/
int main (int argc, char *argv[]) {
    int sd;
    struct sockaddr_in addr;

    // arguments checking
    if (argc != 3) {
        printf("%s", argv[0]);
        return 1;
    }

    // create socket and check for errors
    sd = socket(PF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        printf("Socket creation failed.\n");
    }
    
    // set socket data
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = atoi(argv[2]);
    inet_pton(PF_INET, argv[1], &(addr.sin_addr));
    bind(sd, (struct sockaddr *)&addr, sizeof(addr));

    // connect and check for errors
    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Connection error.\n");
        close(sd);
        return 1;
    }

    // if receive hello proceed with authenticate and operate if not warning
    char desc[100];
    if (!recv_msg(sd, 220, desc)) {
        printf("Did not receive initial message from server.\n");
        close(sd);
        return 1;
    }

    authenticate(sd);
    operate(sd);

    // close socket
    close(sd);

    return 0;
}
