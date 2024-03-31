#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define MAXLINE 1000
#define NAME_LEN 20

char *EXIT_STRING = "exit";                                 // 종료 문자열 변수
int tcp_connect(int af, char *servip, unsigned short port); // 소켓 생성 및 서버 연결, 생성된 소켓리턴
void errquit(char *mesg)
{
    perror(mesg);
    exit(1);
}

int main(int argc, char *argv[])
{
    char bufname[NAME_LEN]; // 이름
    char bufmsg[MAXLINE];   // 메시지부분
    char bufall[MAXLINE + NAME_LEN];
    char str_connect[1024]; // connect 전송 배열
    int maxfdp1;            // 최대 소켓 디스크립터
    int s;                  // 소켓
    int namelen;            // 이름의 길이
    fd_set read_fds;

    if (argc != 4)
    {
        exit(0);
    }

    s = tcp_connect(AF_INET, argv[1], atoi(argv[2]));
    if (s == -1)
        errquit("tcp_connect fail");
    sprintf(str_connect, "%s connection\n", argv[3]);
    printf("%s\n", str_connect); // 채팅 서버 접속 메시지 서버에게 전송
    puts("서버에 접속되었습니다.");
    write(1, "\033[0G", 4); // 커서의 X좌표를 0으로 이동
    send(s, str_connect, strlen(str_connect), 0);
    maxfdp1 = s + 1;
    FD_ZERO(&read_fds);

    while (1)
    {
        FD_SET(0, &read_fds);                                 // 사용자로부터 입력값을 받아야 하기에 0(stdin)를 read_fds set에 등록
        FD_SET(s, &read_fds);                                 // connection한 sockect을 read_fds에 등록
        if (select(maxfdp1, &read_fds, NULL, NULL, NULL) < 0) // fd의 가장 큰 값(maxfdp1)까지 순차적으로 돌면서 read_fds에 이벤트가 발생하는지 확인
            errquit("select fail");
        if (FD_ISSET(s, &read_fds)) // 이벤트가 발생했고 s => connection된 sockect으로부터 온 것이면
        {
            int nbyte;
            if ((nbyte = recv(s, bufmsg, MAXLINE, 0)) > 0) // 데이터를 받아서
            {
                bufmsg[nbyte] = 0;
                write(1, "\033[0G", 4);          // 커서의 X좌표를 0으로 이동
                printf("%s", bufmsg);            // 데이터 출력
                fprintf(stderr, "%s>", argv[3]); // 내 닉네임 출력
            }
        }
        if (FD_ISSET(0, &read_fds)) // 사용자가 입력한 것
        {
            if (fgets(bufmsg, MAXLINE, stdin)) // 사용자가 입력한 값을 가져옴
            {
                char str_disconnect[1024];
                fprintf(stderr, "\033[1A");                // Y좌표를 현재 위치로부터 -1만큼 이동
                sprintf(bufall, "%s>%s", argv[3], bufmsg); // 메시지에 현재시간 추가
                if (strstr(bufmsg, EXIT_STRING) != NULL)   // exit인지 확인
                {
                    fprintf(stderr, "%s disconnect\n", argv[3]);
                    sprintf(str_disconnect, "%s disconnect\n", argv[3]);
                    send(s, str_disconnect, strlen(str_disconnect), 0); // 서버에게 종료 문자열 전송
                    close(s);
                    exit(0);
                }
                if (send(s, bufall, strlen(bufall), 0) < 0) // 서버에게 사용자가 입력한 것을 전송
                    puts("Error : Write error on socket.");
            }
        }
    } // end of while
}

int tcp_connect(int af, char *servip, unsigned short port)
{
    struct sockaddr_in servaddr;
    int s;
    // 소켓 생성
    if ((s = socket(af, SOCK_STREAM, 0)) < 0)
        return -1;
    // 채팅 서버의 소켓주소 구조체 servaddr 초기화
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = af;
    inet_pton(AF_INET, servip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);

    // 연결요청
    if (connect(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        return -1;
    return s;
}