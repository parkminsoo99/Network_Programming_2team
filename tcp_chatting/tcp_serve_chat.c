#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define MAXLINE 511
#define MAX_SOCK 1024

char *EXIT_STRING = "exit";                         // 클라이언트의 종료요청 문자열
char *START_STRING = "Connected to chat_server \n"; // 클라이언트의 채팅서버 접속시 성공 문자열 변수
int maxfdp1;                                        // 최대 소켓번호 +1
int num_user = 0;                                   // 채팅 참가자 수
int clisock_list[MAX_SOCK];                         // 채팅에 참가자 소켓번호 목록
char ip_list[MAX_SOCK][20];                         // 접속한 ip목록
int listen_sock;                                    // 서버의 리슨 소켓

void addClient(int s, struct sockaddr_in *newcliaddr, char *new_user); // 새로운 채팅 참가자 처리
int getmax();                                                          // 최대 소켓 번호 찾기
void removeClient(int s, char *buf, int listen_sock);                  // 채팅 탈퇴 처리 함수
int tcp_listen(int host, int port, int backlog);                       // 소켓 생성 및 listen
void errquit(char *mesg)
{
    perror(mesg);
    exit(1);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in cliaddr;
    char buf[MAXLINE + 1];      // 클라이언트에서 받은 메시지
    char new_user[MAXLINE + 1]; // 클라이언트 처음 첩속시 받는 메시지
    int i, j, nbyte, accp_sock, addrlen = sizeof(struct sockaddr_in);
    fd_set read_fds; // 읽기를 감지할 fd_set 구조체

    if (argc != 2)
    {
        exit(0);
    }

    // tcp_listen(host, port, backlog) 함수 호출 => 모든 host에서 5개까지만 연결을 받는다.
    listen_sock = tcp_listen(INADDR_ANY, atoi(argv[1]), 5);
    while (1)
    {
        FD_ZERO(&read_fds);             // read_fds구조체의 모든 비트를 제로로 만든다.
        FD_SET(listen_sock, &read_fds); // 우리가 듣고자 하는 fd(listen_sock)를 read_fds fd_set변수에 저장한다. => 소켓을 관리 대상으로 지정
        for (i = 0; i < num_user; i++)
            FD_SET(clisock_list[i], &read_fds); // clisock_list(채팅 참가자)에 있는 fd를 모두 read_fds fd_set변수에 저장한다.

        maxfdp1 = getmax() + 1;                               // maxfdp1 재 계산 => 최대 파일 디스크립터 값 계산
        if (select(maxfdp1, &read_fds, NULL, NULL, NULL) < 0) // fd의 가장 큰 값(maxfdp1)까지 순차적으로 돌면서 read_fds에 이벤트가 발생하는지 확인
            errquit("select failed");

        if (FD_ISSET(listen_sock, &read_fds)) // FD_ISSET이라는 함수를 통해 우리가 관찰하는 read_fds 중에 이벤트가 발생하면, 그 이벤트가 소켓을 리슨하는 파일 디스크립터라면 어떤 새로운 요청이 들어온 것이기에 accpet
        {
            accp_sock = accept(listen_sock,
                               (struct sockaddr *)&cliaddr, &addrlen); // 새로운 클라이언트에 대한 accept
            if (accp_sock == -1)
                errquit("accept fail");
            addClient(accp_sock, &cliaddr, new_user);               // 새로운 채팅 참가자 추가
            send(accp_sock, START_STRING, strlen(START_STRING), 0); // 클라이언트의 채팅서버 접속시 성공 문자열 전송
            write(1, "\033[0G", 4);                                 // 커서의 X좌표를 0으로 이동
        }

        for (i = 0; i < num_user; i++)
        {
            if (FD_ISSET(clisock_list[i], &read_fds)) // clisock_list배열(채팅 참가자)에서 서버에게 전송한 것
            {
                nbyte = recv(clisock_list[i], buf, MAXLINE, 0); // 데이터를 받아서
                if (nbyte <= 0)                                 // 0보다 작다면
                {
                    removeClient(i, buf, listen_sock); // 클라이언트의 종료
                    continue;
                }
                buf[nbyte] = 0;                       // 종료문자 처리
                if (strstr(buf, EXIT_STRING) != NULL) // exit면
                {
                    removeClient(i, buf, listen_sock); // 클라이언트의 종료
                    continue;
                }

                for (j = 0; j < num_user; j++)
                    send(clisock_list[j], buf, nbyte, 0); // 모든 채팅 참가자에게 메시지 전송
                printf("\033[0G");                        // 커서의 X좌표를 0으로 이동
                printf("%s", buf);                        // 메시지 출력
            }
        }
    } // end of while
    return 0;
}

// 새로운 채팅 참가자 처리
void addClient(int s, struct sockaddr_in *newcliaddr, char *new_user)
{
    char buf[20];
    inet_ntop(AF_INET, &newcliaddr->sin_addr, buf, sizeof(buf));
    write(1, "\033[0G", 4);          // 커서의 X좌표를 0으로 이동
    printf("new client: %s\n", buf); // ip출력
    // 채팅 클라이언트 목록에 추가
    clisock_list[num_user] = s;
    strcpy(ip_list[num_user], buf);
    num_user++;                                                    // 유저 수 증가
    int size = recv(clisock_list[num_user], new_user, MAXLINE, 0); // 데이터를 받아서
    for (int i = 0; i < num_user; i++)
    {
        send(clisock_list[i], new_user, size, 0); // 모든 채팅 참가자에게 새로운 소켓의 채팅 접속 메시지 전송
    }
}

// 채팅 탈퇴 처리
void removeClient(int s, char *buf, int listen_sock)
{
    int a;
    close(clisock_list[s]); // 소켓 종료
    if (s != num_user - 1)
    { // 저장된 리스트 재배열
        clisock_list[s] = clisock_list[num_user - 1];
        strcpy(ip_list[s], ip_list[num_user - 1]);
    }
    num_user--;             // 유저 수 감소
    write(1, "\033[0G", 4); // 커서의 X좌표를 0으로 이동
}

// 최대 소켓번호 찾기
int getmax()
{
    // Minimum 소켓번호는 가정 먼저 생성된 listen_sock
    int max = listen_sock;
    int i;
    for (i = 0; i < num_user; i++)
        if (clisock_list[i] > max)
            max = clisock_list[i];
    return max;
}

// listen 소켓 생성 및 listen
int tcp_listen(int host, int port, int backlog)
{
    int sd;
    struct sockaddr_in servaddr;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
    {
        perror("socket fail");
        exit(1);
    }
    // servaddr 구조체의 내용 세팅
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);
    if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind fail");
        exit(1);
    }
    // 클라이언트로부터 연결요청을 기다림
    listen(sd, backlog);

    return sd;
}