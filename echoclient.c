#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3)
    {
        fprintf(stderr, "usage : %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    // associate a descriptor with a read buffer and reset buffer
    // 읽기 버퍼 초기화 후, 식별자와 연결
    Rio_readinitb(&rio, clientfd);

    // fgets()는 첫번째 줄 바꾸기 문자('\n')까지 또는 읽은 문자수가 n-1까지 읽음
    while(Fgets(buf, MAXLINE, stdin) != NULL)
    {
        Rio_writen(clientfd, buf, strlen(buf));
        Rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
    }

    Close(clientfd);
    exit(0);
}