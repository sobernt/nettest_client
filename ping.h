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

using namespace std;

class ping
{
public:
    ping();
    std::stringstream getStream();
    int do_ping(char ip_addr[INET_ADDRSTRLEN],
                          bool is_tcp,
                          uint16_t packet_count,
                          uint16_t packet_length,
                          uint16_t ping_timeout);
    std::stringstream textstream;
private:
    struct icmphdr icmp_hdr;
    struct sockaddr_in connect_addr;
    struct timeval timeout;
    struct rtt_stat{
        double min;
        double max;
        double middle;
        double mdev;
    };
    std::vector<int> rtts;

    double get_loss_percent(int count_transmitted,int count_recived);
    const char *format_double_to_str(double val,int precision);
    rtt_stat getrtt();
    double getRTTW(double old_rtt,double rtt);
};

#endif // PING_H
