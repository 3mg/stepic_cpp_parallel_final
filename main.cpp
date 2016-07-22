#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <iostream>
#include <sys/un.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>

#include <pthread.h>

#include <string>
#include <sstream>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>

char *dir;
pthread_mutex_t mymutex = PTHREAD_MUTEX_INITIALIZER;

static void * thread_start(void *socket_description) {
    int cs = *(int*)socket_description;

    pthread_mutex_unlock(&mymutex);

    printf("New request from %i\n", cs);

    char buf[BUFSIZ];

    int n = recv(cs, buf, BUFSIZ, MSG_NOSIGNAL);

    printf("%s\n", buf);

    char *part = strtok (buf, "\r\n");

    char *request = new char[strlen(part) + 1];
    strcpy(request, part);

    char *method = strtok (request, " ");
    char *path = strtok (NULL, " ");
    char *protocol = strtok (NULL, " ");

    strtok (buf, "\r\n"); // set to 2nd line

    printf("-----------------------\n");
    /*while (part != NULL && strlen(part) > 0) {
        part = strtok (NULL, "\r\n");
        int colon_idx = 0;
        while(part[colon_idx] != ':') {
            colon_idx++;
        }
        part[colon_idx] = '\0';
        char *header = &part[0], *value = &part[colon_idx + 2];

        printf("%s: %s\n", header, value);
    }*/
    printf("-----------------------\n");

    int code = 200;
    const char *content = "<h1>404</h1>";
    int content_length = strlen(content);
    std::stringstream ss;
    std::string s;

    char *filename = new char[strlen(dir) + strlen(path) + 1];
    sprintf(filename, "%s%s", dir, path);

    printf("fielname requested: %s \n", filename);

    struct stat file_stat;
    int res = stat(filename, &file_stat);

    if (res != -1 && file_stat.st_mode & S_IFREG) {
        FILE *fptr = fopen(filename, "r");

        code = 200;

        content_length = file_stat.st_size;

        ss<<"HTTP/1.1 "<<code<<" OK"<<"\r\n";
        ss<<"Content-Length: "<<content_length<<"\r\n";
        ss<<"Content-Type: text/html"<<"\r\n";
        ss<<"Connection: Closed"<<"\r\n";

        ss<<"\r\n";

        s = ss.str();
        write(cs, s.c_str(), s.length());

        char f_buf;
        while (fread(&f_buf, sizeof(char), 1, fptr) > 0) {
            write(cs, &f_buf, sizeof(char));
        }

        fclose(fptr);
    } else {
        code = 404;

        ss<<"HTTP/1.1 "<<code<<" Not found"<<"\r\n";
        ss<<"Content-Length: "<<content_length<<"\r\n";
        ss<<"Content-Type: text/html"<<"\r\n";
        ss<<"Connection: Closed"<<"\r\n";

        ss<<"\r\n"<<content;

        s = ss.str();
        write(cs, s.c_str(), s.length());
    }


    close(cs);


    delete filename;
    delete request;

    printf("Response sent to %i\n", cs);

    return 0;
}

struct sockaddr_in local;

void serve(char *host, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("socket: %d\n", s);

    inet_aton(host, &local.sin_addr);
    local.sin_port = htons(port);
    local.sin_family = AF_INET;

    int result = bind(s, (struct sockaddr *) &local, sizeof(local));
    if (result == -1) {
        perror("bind");

        local.sin_port = htons(port + 1);
        result = bind(s, (struct sockaddr *) &local, sizeof(local));
        if (result == -1) {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }

    listen(s, 5);

    char buf[BUFSIZ];

    int cs;

    pthread_mutex_lock(&mymutex);

    while (cs = accept(s, NULL, NULL)) {
        printf("Accepted request from %i\n", cs);

        int res, opt;
        pthread_t tnum;
        pthread_attr_t attr;

        res = pthread_attr_init(&attr);
        if (res != 0) {
            perror("pthread_attr_init");
            exit(1);
        }
        res = pthread_attr_setdetachstate(&attr, 1);
        if (res != 0) {
            perror("pthread_attr_setdetachstate");
            exit(1);
        }

        res = pthread_create(&tnum, &attr, &thread_start, (void*) &cs);
        if (res != 0) {
            perror("pthread_create");
            exit(1);
        }

        res = pthread_attr_destroy(&attr);
        if (res != 0) {
            perror("pthread_create");
            exit(1);
        }

        pthread_mutex_lock(&mymutex);
    }
}

int main(int argc, char **argv)
{
    char *host_s = NULL, *dir_s = NULL, opchar ;
    int port = 0;

    int has_host = 0, has_port = 0, has_dir = 0;
    int opindex = 0;
    int opcnt = 0;

    struct option opts[] = {
            {"host", required_argument, &has_host, 1},
            {"port", required_argument, &has_port, 1},
            {"dir", required_argument, &has_dir, 1},
            {0,0,0,0}
    };

    while ( -1 != (opchar = getopt_long(argc, argv, "h:p:d:", opts, &opindex))) {
        printf("option %c with value '%s'\n", opchar, optarg);
        switch( opchar ) {
            case 'h':
                host_s = new char[strlen(optarg)];
                strcpy(host_s, optarg);
                break;

            case 'p':
                port = atoi(optarg);
                break;

            case 'd':
                dir_s = new char[strlen(optarg)];
                strcpy(dir_s, optarg);
                break;

            default: /* '?' */
                fprintf(stderr, "Usage: %s -h 0.0.0.0 -p 9090 -d /home/user\n", argv[0]);
                exit(EXIT_FAILURE);
        }
        opcnt++;
    }

    std::cout<<host_s<<" "<<port<<" "<<dir_s<<std::endl;

    dir = dir_s;

    pid_t pid = getpid();

    int daemon_id = fork();

    if (daemon_id == 0) {
        pid = getpid();
        printf("%i\n", pid);

        chdir("/");
        setsid();
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);

        serve(host_s, port);
    } else {

    }

    return 0;
}
