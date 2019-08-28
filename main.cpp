#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
//Базовые функции сокетов BSD и структуры данных.
#include <netinet/in.h>
//Семейства адресов/протоколов PF_INET и PF_INET6. Широко используются в сети Интернет,
//включают в себя IP-адреса, а также номера портов TCP и UDP.
#include <arpa/inet.h>
//Функции для работы с числовыми IP-адресами.
#include <netdb.h>
//Функции для преобразования протокольных имен и имен
//хостов в числовые адреса. Используются локальные данные аналогично DNS.
#include <string.h>
#include <ping.h>
#include <nettest.h>
#include <math.h>
#include <limits.h>
using namespace std;

#define SOCKET_PORT 5001
#define BUFFER_SIZE 10192
#define SOCKET_READ_TIMEOUT 16
#define SOCKET_PING_REPLY_RIMEOUT = 120
    sockaddr_in sockAddr;
    int sock;
string do_ping(query_ping command){
    /*
     * comand:ping
     * host_ip:8.8.8.8
     * type:tcp //тип пакета (TCP/UDP)
     * packetLength:54 //длину пакета (максимальные и минимальные значения зависит от типа пакетов)
     * packetCount:10 // количество пакетов
     * testTime:10 //время теста
    */
    Ping ping;
    string dataping;
    string buff = "";
    ping.do_ping(command);
    while (std::getline(ping.textstream, buff)) {
        dataping.append(buff);
        dataping.append("\n");
    }
    return dataping;
}

int send_str_by_parts(string str){
    nettest_body body;
    bzero(&body,sizeof body);
    int bytesWrited;
    int result = 0;
    const char* all_data = str.data();
    uint16_t strsize = str.size();
    body.seq_count = ceil((double)strsize/512);
    for(int8_t i=0;i<body.seq_count;i++){
        char* datapart;
        body.seq = i;
        for(uint16_t charpoint = 0;charpoint < 512;charpoint++){
            uint16_t dataindex = (body.seq+(512*i))+charpoint;
            if(dataindex>str.length()){
                break;
            }
            body.data[charpoint] = all_data[dataindex];
        }
        body.size = strlen(body.data);
        bytesWrited = send(sock , (struct nettest_body *) &body , sizeof body,0);
        if (bytesWrited <= 0) {
            usleep(SOCKET_READ_TIMEOUT);
            bytesWrited = send(sock , (struct nettest_body *) &body , sizeof body,0);
            if (bytesWrited <= 0) {
                return -1;
            }
        }
        result+=bytesWrited;
        return result;
    }
}
int main() {
    fd_set read_set;
    FD_ZERO(&read_set);

    if(getuid() != 0)
    {
       fprintf(stderr, "root privileges needed\n");
       exit(EXIT_FAILURE);
    }

    sock = socket(PF_INET, SOCK_STREAM,IPPROTO_TCP);

    struct timeval timeout = {SOCKET_READ_TIMEOUT, 0};
    if(sock == -1){
        perror("Failed open socket");
        exit(EXIT_FAILURE);
    }
    memset(&sockAddr, 0, sizeof (sockAddr));

    sockAddr.sin_family = PF_INET;
    sockAddr.sin_port = htons(SOCKET_PORT);
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*) &sockAddr, sizeof (sockAddr)) == -1) {
            perror("Failed connect to socket");
            close(sock);
            exit(EXIT_FAILURE);
    }
    if (listen(sock, 1) == -1) {
            perror("Lissen socket error");
            close(sock);
            exit(EXIT_FAILURE);
    }
    //set key for close sock by kill -9 of this app
    int optval = 1;
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof optval);
        sock = accept(sock, 0, 0);

        if (sock < 0) {
            perror("input data error");
            close(sock);
            exit(EXIT_FAILURE);
        }
        nettest_header command;
        int bytesReaded = 0;
        bzero(&command, sizeof command);
        int8_t sets_count = 0;
        while(true){

            memset(&read_set, 0, sizeof read_set);
            FD_SET(sock, &read_set);


            sets_count = select(sock + 1, &read_set, NULL, NULL, &timeout);

            if (sets_count == 0) {
                usleep(SOCKET_READ_TIMEOUT);
                continue;
            } else if (sets_count < 0) {
                usleep(SOCKET_READ_TIMEOUT);
                continue;
            }
            bytesReaded = recv(sock, (struct nettest_header *)&command, sizeof command,0);
            if (bytesReaded <= 0) {
                usleep(SOCKET_READ_TIMEOUT);
                continue;
            } else if (bytesReaded == 0) {
                usleep(SOCKET_READ_TIMEOUT);
                continue;
            }

                if(command.command == nettest_command_exit){
                    nettest_header header;
                    bzero(&header, sizeof header);
                    header.command = nettest_command_exit_req;
                    header.size = 0;
                    send(sock , (struct nettest_header *) &header , sizeof header,0);
                    usleep(SOCKET_READ_TIMEOUT);
                    break;
                } else
                if(command.command == nettest_command_ping){
                    query_ping ping_comand;
                    string ping_data = "";
                    int count_sended = 0;
                    memset(&ping_comand,0,sizeof ping_comand);

                    bytesReaded = recv(sock, (struct query_ping *)&ping_comand, sizeof ping_comand,0);
                    if (bytesReaded <= 0) {
                        usleep(SOCKET_READ_TIMEOUT);
                        continue;
                    } else if (bytesReaded == 0) {
                        usleep(SOCKET_READ_TIMEOUT);
                        continue;
                    }

                    ping_data = do_ping(ping_comand);
                    nettest_header header;

                    bzero(&header, sizeof header);

                    header.command = nettest_command_ping_reply;
                    header.size = ping_data.size();

                    send(sock ,(struct nettest_header *)  &header , + sizeof header,0);
                    count_sended = send_str_by_parts(ping_data);
                } else {
                    char* broken_buff;
                    bzero(&broken_buff,command.size);
                    recv(sock,(char *) &broken_buff,(command.size),MSG_WAITALL);
                    //read undefined packet from buffer for clear him.
                }
        }
        shutdown(sock, SHUT_RDWR);
        close(sock);

        return 0;
}

