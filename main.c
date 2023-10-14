#include "src/server/server.h"

int main(void) {
    int sockfd, *new_sockfd, yes = 1;
    struct sockaddr_in host_addr, client_addr;      //My address information
    socklen_t sin_size;

    log_message(INFO,"Accepting web requests on port %d\n", PORT);

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        fatal("in socket");
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        fatal("setting socket option SO_REUSEADDR\n");

    host_addr.sin_family = AF_INET;     //Host byte order
    host_addr.sin_port = htons(PORT);       //Short, newtork byte order
    host_addr.sin_addr.s_addr = INADDR_ANY; //Automatically fill with my IP
    memset(&(host_addr.sin_zero), '\0', 8); //Zero the rest of the struct

    if (bind(sockfd, (struct sockaddr *) &host_addr, sizeof(struct sockaddr)) == -1)
        fatal("binding to socket");

    if (listen(sockfd, 20) == -1)
        fatal("listening on socket");

    while (1) {  //accept loop
        sin_size = sizeof(struct sockaddr_in);
        new_sockfd = (int *) malloc(sizeof(int)); // Allocate memory for the new socket descriptor
        if (new_sockfd == NULL)
            fatal("malloc");

        *new_sockfd = accept(sockfd, (struct sockaddr *) &client_addr, &sin_size);
        if (*new_sockfd == -1) {
            free(new_sockfd);
            fatal("accepting connection");
        }

        // Create a new thread and pass sockfd and client_addr_ptr as arguments
        pthread_t thread;

        struct ThreadArgs *args = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs));
        if (args == NULL) {
            free(new_sockfd);
            fatal("malloc");
        }
        args->sockfd = *new_sockfd;
        args->client_addr_ptr = &client_addr;
        if (pthread_create(&thread, NULL, (void *(*)(void *)) handle_connection, args) != 0) {
            free(new_sockfd);
            fatal("pthread_create");
        }

        // Detach the thread to allow it to clean up automatically
        pthread_detach(thread);
    }//Close while
    return 0;
}//Close main