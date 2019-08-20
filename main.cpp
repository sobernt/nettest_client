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
using namespace std;

#define SOCKET_PORT 5001
#define BUFFER_SIZE 10192
#define SOCKET_READ_RIMEOUT = 16
    sockaddr_in sockAddr;
    int sock;
const char* do_ping(query_ping command){
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
    string buff;
    ping.do_ping(command);
    while (std::getline(ping.textstream, buff)) {
        dataping.append(buff);
    }
    return dataping.data();
}


int main() {
    sock = socket(PF_INET, SOCK_STREAM,IPPROTO_TCP);

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
    if (listen(sock, 10) == -1) {
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
        nettest_command command;
        int bytesReaded;
        while(true){
            do{
                bytesReaded =  recv(sock,&command,(sizeof command),MSG_NOSIGNAL);
            }
            while(bytesReaded <0);
                if(command == nettest_command_exit){
                    char* data = 0;
                    nettest_header header;
                    header.command = nettest_command_exit_req;
                    header.size = 0;
                    memcpy(data, &header, sizeof header);
                    send(sock , data , sizeof(data),MSG_NOSIGNAL);
                    break;
                }
                if(command == nettest_command_ping){
                    query_ping ping_comand;
                    const char* ping_data = 0;
                    char* data = 0;
                    memset(&ping_comand,0,sizeof ping_comand);
                    bytesReaded =  recv(sock,&ping_comand,(sizeof ping_comand),MSG_NOSIGNAL);

                    ping_data = do_ping(ping_comand);
                    nettest_header header;

                    header.command = nettest_command_ping_reply;
                    header.size = strlen(ping_data);

                    memcpy(data, &header, sizeof header);
                    memcpy(data + sizeof header, ping_data, strlen(ping_data));

                    send(sock , data , sizeof(data),MSG_NOSIGNAL);
                }

        }
        shutdown(sock, SHUT_RDWR);

        close(sock);
}

