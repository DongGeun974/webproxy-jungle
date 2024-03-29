#include <stdio.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0(X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char*path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
int connect_endServer(char *hostname, int port, char *http_header);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  socklen_t clientlen;
  char hostname[MAXLINE], port[MAXLINE];
  
  struct sockaddr_storage clientaddr;

  if(argc != 2)
  {
    fprintf(stderr, "usage : %s <port>\n", argv[0]);
  }

  listenfd = Open_listenfd(argv[1]);

  while(1)
  {
    clientlen = sizeof(clientaddr);

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s).\n", hostname, port);

    doit(connfd);
    Close(connfd);
  }
  return 0;
}

void doit(int connfd)
{
  int end_serverfd;

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char endserver_http_header[MAXLINE];
  char hostname[MAXLINE], path[MAXLINE];
  int port;

  rio_t rio, server_rio;

  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);

  if (strcasecmp(method, "GET"))
  {
    printf("Proxy does not implement the method\n");
    return;
  }

  // uri를 통해서 hostname, path, port 설정
  parse_uri(uri, hostname, path, &port);

  // end server에 보낼 http 헤더 설정
  build_http_header(endserver_http_header, hostname, path, port, &rio);

  // endserver(server)에 연결
  end_serverfd = connect_endServer(hostname, port, endserver_http_header);
  if(end_serverfd < 0)
  {
    printf("Connection failed\n");
    return;
  }

  // endserver(server)에 수정된 헤더 전송
  rio_readinitb(&server_rio, end_serverfd);
  Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));

  size_t n;
  // receive message from endserver(server)
  while ((n=Rio_readlineb(&server_rio, buf, MAXLINE)) != 0)
  {
    printf("proxy received %d bytes, then send\n", n);
    // send to client
    Rio_writen(connfd, buf, n);
  }
  Close(end_serverfd);
}

void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio)
{
  char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  // 경로(path)를 format에 맞추어 request_hdr에 저장
  sprintf(request_hdr, requestlint_hdr_format, path);

  // client에서 보낸 요청(http header)를 라인마다 읽으면서 처리
  while(Rio_readlineb(client_rio, buf, MAXLINE) > 0)
  {
    // endof_hdr = "\r\n"
    if(strcmp(buf, endof_hdr) == 0)
    {
      break;
    }

    // host_key = "Host"
    if (!strncasecmp(buf, host_key, strlen(host_key)))
    {
      // buf를 host_key에 저장
      strcpy(host_hdr, buf);
      continue;
    }

    /*
      connection_key = "Connection";
      user_agent_key = "User-Agent";
      proxy_connection_key = "Proxy-Connection";
      strncasecmp() : 같으면 return 0
      if에서 (조건1) || (조건2) 일 때, 조건1이 참이여도 뒤에 조건 확인
    */
    if(!strncasecmp(buf, connection_key, strlen(connection_key)) 
        || !strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
        || !strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
    {
      strcat(other_hdr, buf);
    }
  }

  if(strlen(host_hdr) == 0)
  {
    sprintf(host_hdr, host_hdr_format, hostname);
  }

  // set http_header that will send to endserver
  sprintf(http_header,"%s%s%s%s%s%s%s",
          request_hdr, host_hdr, conn_hdr, prox_hdr,
          user_agent_hdr, other_hdr, endof_hdr);
    
  return;
}

// endserver(server)에 연결하는데, 서버 입장에서는 프록시는 클라이언트
inline int connect_endServer(char *hostname, int port, char *build_http_header)
{
  char portStr[100];
  sprintf(portStr, "%d", port);
  return Open_clientfd(hostname, portStr);
}

void parse_uri(char *uri, char *hostname, char *path, int *port)
{
  *port = 80;
  char *pos = strstr(uri,"//");

  pos = (pos!=NULL) ? (pos+2) : (uri);

  char *pos2 = strstr(pos, ":");

  if(pos2 != NULL)
  {
    *pos2 = '\0';
    sscanf(pos, "%s", hostname);
    sscanf(pos2+1, "%d%s", port, path);
  }
  else
  {
    pos2 = strstr(pos, "/");
    if (pos2 != NULL)
    {
      *pos2 = '\0';
      sscanf(pos,"%s",hostname);
      *pos2 = '/';
      sscanf(pos2,"%s",path);
    }
    else
    {
      sscanf(pos,"%s",hostname);
    }
  }
  return;
}