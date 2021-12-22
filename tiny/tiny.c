/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

void head_serve_static(int fd, char *filename, int filesize);
void head_serve_dynamic(int fd, char *filename, char *cgiargs);

static void sigpipe_handle(int x) {
  printf("Caught SIGPIPE\n");
}

int main(int argc, char **argv) {
  // sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
  // signal(SIGPIPE, SIG_IGN);
  // sigset_t set;
  // sigemptyset(&set);
  // sigaddset(&set, SIGPIPE);
  // pthread_sigmask(SIG_BLOCK, &set, NULL);
  signal(SIGPIPE, sigpipe_handle);
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 서버는 open_listenfd함수 호출해서 연결 요청 받을 준비가 된 듣기 식별자 생성
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    // http transaction
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

/*
  한 개의 http 트랜잭션 처리
  라인을 읽고 분석
  get 메소드만 지원
*/
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  // 식별자 fd를 rio의 읽기 버퍼와 연결
  Rio_readinitb(&rio, fd);
  // rio에서 읽은 데이터를 buf에 복사
  Rio_readlineb(&rio, buf, MAXLINE);

  printf("Request header:\n");
  printf("%s",buf);

  // buf 값을 method, uri, version에 저장
  sscanf(buf, "%s %s %s", method, uri, version);

  if(!strcasecmp(method, "GET"))
  {
    /*
      요청 헤더 내의 어떤 정보도 사용하지 않음
    */
    read_requesthdrs(&rio);

    /* Parse URI for GET request */
    // set uri, filename, cgiargs
    is_static = parse_uri(uri, filename, cgiargs);

    if (stat(filename, &sbuf) < 0)
    {
      clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
      return;
    }

    if (is_static)    /* Serve static content */
    {
      // S_ISREG : 정규 파일인지 판별
      // S_IRUSR : 사용자 읽기
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
      {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return;
      }
      serve_static(fd, filename, sbuf.st_size);
    }
    else    /* Serve dynamic content */
    {
      // printf("log : fd = %d, filename = %s, cgiargs = %s\n", fd, filename, cgiargs);

      // S_ISREG : 정규 파일인지 판별
      // S_IXUSR : 사용자 실행
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
      {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
      serve_dynamic(fd, filename, cgiargs);
    }
  }
  else if (!strcasecmp(method, "HEAD"))
  {
    /*
      요청 헤더 내의 어떤 정보도 사용하지 않음
    */
    read_requesthdrs(&rio);

    /* Parse URI for GET request */
    // set uri, filename, cgiargs
    is_static = parse_uri(uri, filename, cgiargs);

    if (stat(filename, &sbuf) < 0)
    {
      clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
      return;
    }

    if (is_static)    /* Serve static content */
    {
      // S_ISREG : 정규 파일인지 판별
      // S_IRUSR : 사용자 읽기
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
      {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return;
      }
      head_serve_static(fd, filename, sbuf.st_size);
    }
    else    /* Serve dynamic content */
    {
      // printf("log : fd = %d, filename = %s, cgiargs = %s\n", fd, filename, cgiargs);

      // S_ISREG : 정규 파일인지 판별
      // S_IXUSR : 사용자 실행
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
      {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return;
      }
      head_serve_dynamic(fd, filename, cgiargs);
    }

  }
  else
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Pring the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n",(int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/*
  요청 헤더 내의 어떤 정보도 사용하지 않음
*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s",buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;
  
  // 정적 컨텐츠는 홈 디렉토리
  // 동적 컨텐츠는 cgi-bin 디렉토리
  if (!strstr(uri, "cgi-bin"))    /* Static content */
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    // 이어붙힘
    strcat(filename,uri);
    if (uri[strlen(uri)-1] == '/')
    {
      strcat(filename, "home.html");
    }
    return 1;
  }
  else      /* Dynamic content */
  {
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else
    {
      strcpy(cgiargs,ptr+1);
    }

    strcpy(filename,".");
    strcat(filename,uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n",buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n",buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response header:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  // Close(srcfd);
  // Rio_writen(fd, srcp, filesize);
  // Munmap(srcp, filesize);

  srcp = malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  free(srcp);
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
  {
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif"))
  {
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png"))
  {
    strcpy(filetype, "image/png");
  }
  else if (strstr(filename, ".jpg"))
  {
    strcpy(filetype, "image/jpeg");
  }
  else if (strstr(filename, ".mp4"))
  {
    strcpy(filetype, "video/mp4");
  }
  else
  {
    stpcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first partt of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)    /* Child process */
  {
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }

  Wait(NULL);
}


void head_serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n",buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n",buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response header:\n");
  printf("%s", buf);
}

void head_serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first partt of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
}