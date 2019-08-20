#ifndef PING_H
#define PING_H
#include <iostream>
#include <linux/icmp.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <arpa/inet.h>
#include <iomanip>
#include <vector>
#include <float.h>
#include <sstream>
#include <math.h>
#include <nettest.h>
using namespace std;

class Ping
{
public:
    Ping();
    std::stringstream getStream();
    int do_ping(in_addr &ip_addr,
                          bool &is_tcp,
                          uint16_t &packet_count,
                          uint16_t &packet_length,
                          uint16_t &ping_timeout);
    int do_ping(query_ping query_ping_in);
    std::stringstream textstream;
private:
    struct icmphdr icmp_hdr;
    struct sockaddr_in connect_addr;
    struct timeval timeout;
    enum ping_result_code{
        ERROR_SOCKET_SEND,
        ERROR_REPLY_READ_TIMEOUT,
        ERROR_REPLY_READ,
        ERROR_DEST_UNREACH,
        ERROR_REPLY_ICMP_TIMEOUT,
        ERROR_SHORT_ICMP,
        ERROR_UNKNOWN_REPLY,
        SUCCESS
    };
    struct rtt_stat{
        double min;
        double max;
        double middle;
        double mdev;
    };
    struct ping_result{
        unsigned int reply_bytes;
        unsigned int reply_bytes_full;
        unsigned int seq;
        double req_time;
        ping_result_code result_code;
    };
    std::vector<int> rtts;
    int sock;
    uint8_t count_recived = 0;
    uint8_t count_transmitted = 0;
    double get_loss_percent(int count_transmitted,int count_recived);
    const char *format_double_to_str(double val,int precision);
    rtt_stat getrtt();
    double getRTTW(double old_rtt,double rtt);
    void reset();
    void reset_packet_stat();
    ping_result send_ping(uint16_t seq, uint16_t packet_length);
    void OptimalFrameTest();
    ping_result_code get_error_code(int header_type);
};

#endif // PING_H
