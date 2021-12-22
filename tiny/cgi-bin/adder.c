/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  // getenv("QUERY_STRING") : "QUERY_STRING"이 가리키는 문자열과 일치하는 환경 변수 리스트 탐색
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    // buf에 '&'이 있는지 확인하고, 존재하는 곳의 포인터 반환
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);

    p = strchr(arg1,'=');
    strcpy(arg1,p+1);

    p = strchr(arg2,'=');
    strcpy(arg2,p+1);

    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sThe Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  printf("Connection : close\r\n");
  printf("Content-length : %d\r\n", (int)strlen(content));
  printf("Content-type : text/html\r\n\r\n");
  printf("%s",content);
  // 남아있는 데이터를 모두 씀
  fflush(stdout);

  exit(0);
}
/* $end adder */
