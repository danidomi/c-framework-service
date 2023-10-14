#include "server.h"

/* This function accepts a socket FD and a ptr to the null terminated
 * string to send.  The function will make sure all the bytes of the
 * string are sent.  Returns 1 on success and 0 on failure.
 */
int send_string(int sockfd, unsigned char *buffer) {
    int sent_bytes, bytes_to_send;
    bytes_to_send = strlen(buffer);
    while (bytes_to_send > 0) {
        sent_bytes = send(sockfd, buffer, bytes_to_send, 0);
        if (sent_bytes == -1)
            return 0; // return 0 on send error
        bytes_to_send -= sent_bytes;
        buffer += sent_bytes;
    }
    return 1; // return 1 on success
}

int recv_line(int sockfd, unsigned char *dest_buffer) {
    unsigned char *ptr;
    int eol_matched = 0;

    ptr = dest_buffer;
    while (recv(sockfd, ptr, 1, 0) == 1) { // read a single byte
        if (*ptr == EOL[eol_matched]) { // does this byte match terminator
            eol_matched++;
            if (eol_matched == EOL_SIZE) { // if all bytes match terminator,
                *(ptr + 1 - EOL_SIZE) = '\0'; // terminate the string
                return strlen(dest_buffer); // return bytes recevied
            }
        } else {
            eol_matched = 0;
        }
        ptr++; // increment the pointer to the next byter;
    }
    return 0; // didn't find the end of line characters
}

/***************************************************
This funtion handles the connection on the passed socket
from the passed client address. The connection is processed
as a web request, and this function replies over the
connected socket. Finally, the passed socket is closed
at the end of the function.
****************************************************/
void handle_connection(void *arg) {
    struct ThreadArgs *args = (struct ThreadArgs *) arg;
    int sockfd = args->sockfd;
    struct sockaddr_in *client_addr_ptr = args->client_addr_ptr;
    unsigned char *ptr, *resource;
    Request *request;
    char *requestPlain;
    int fd, length;

    resource = malloc(50);
    requestPlain = malloc(4000);
    length = recv_line(sockfd, requestPlain);
    log_message(DEBUG,"%s", requestPlain);
    request = parse_request(requestPlain);
    if (request != NULL) {
        log_message(DEBUG,"Method: %d", request->method);
        log_message(DEBUG,"Path: %s", request->path);
        log_message(DEBUG,"Query Parameters:");
        for (size_t i = 0; i < request->queryParamsCount; i++) {
            log_message(DEBUG,"\tKey: %s, Value: %s\n", request->queryParams[i].key, request->queryParams[i].value);
        }
    } else {
        log_message(WARNING, "Failed to parse request.");
    }
    log_message(DEBUG, "Got request from %s:%d \"%s\"\n", inet_ntoa(client_addr_ptr->sin_addr),
           ntohs(client_addr_ptr->sin_port), requestPlain);
    ptr = strstr(requestPlain, "HTTP/");     //Search for valid looking request
    if (ptr == NULL) {       //The this isnt a valid HTTP
        log_message(WARNING,"NOT HTTP!\n");
    } else if (strcmp(request->path, "/favicon.ico") == 0) {
        handle_404(sockfd);
        shutdown(sockfd, SHUT_RDWR);
        return;
    } else {
        log_message(DEBUG,"request->path %s %s\n", request->path, get_path());
        if (strcmp(request->path, get_path()) == 0) {
            log_message(DEBUG, "Handling %s", request->path);
            Response *response = handle_api(request);
            if (response == NULL) {
                log_message(WARNING, "Response is NULL");
                handle_404(sockfd);
                shutdown(sockfd, SHUT_RDWR);
                return;
            }
            char *http = malloc(10 + strlen(response->status_code) * sizeof(char));
            char *header = malloc(strlen(response->headers[0]) * response->headersCount * sizeof(char));
            char data[1000];
            //char *data = malloc(strlen(response->data) * sizeof(char)); // TODO would be better to do something like this!
            sprintf(http, "HTTP/1.0 %s\r\n", response->status_code);
            if (response->headersCount > 0) {
                for (int i = 0; i < response->headersCount; i++) {
                    log_message(DEBUG,"%d\n", i);
                    if (i != (response->headersCount - 1)) {
                        sprintf(header, "%s\r\n", response->headers[i]);
                    } else {
                        sprintf(header, "%s\r\n\r\n", response->headers[i]);
                    }
                }
            }
            log_message(DEBUG,"data: %s\n", response->data);
            sprintf(data, "%s\r\n", response->data);
            send_string(sockfd, http);
            send_string(sockfd, header);
            send_string(sockfd, data);
            free(http);
            free(header);
            //free(data);
        } else {
            handle_404(sockfd);
        } //Close else printf 200 block
    }//End if block for valid HTTP
    shutdown(sockfd, SHUT_RDWR);    //close socket ever so gingerly
}//Close void handle_connection


void handle_404(int sockfd) {
    send_string(sockfd, "HTTP/1.0 404 Not Found\r\n");
    send_string(sockfd, "Server: Tiny webserver \r\n\r\n");
    send_string(sockfd, "<html><head><title>404 not found</title></head>");
    send_string(sockfd, "<body><h1>URL not found</h1></body></html>\r\n");
}

/*******************************************************
This function accepts an open file descriptor and returns
the size of the associated file. returns -1 on failure
********************************************************/
int get_file_size(int fd) {
    struct stat stat_struct;

    if (fstat(fd, &stat_struct) == -1)
        return -1;
    return (int) stat_struct.st_size;
}// close get_file_size

// Definition of function fatal().
void fatal(char *a) {
    log_message(ERROR, "Error: %s\n", a);
    exit(-1);
}

// Default Implement this methods
__attribute__((weak)) Response *handle_api(Request *request) {
    Response *response = malloc(sizeof(Response));
    response->status_code = "200 OK";
    response->headers[0] = "Content-Type: application/json; charset=utf-8";
    response->headersCount = 1;
    response->data = "world";
    return response;
}

__attribute__((weak)) char *get_path() {
    return "/hello";
}
