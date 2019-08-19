#include "ping.h"

#define REPLY_TIMEOUT 3
#define RTT_alpha 0.9
ping::ping(){
    memset(&connect_addr,0,sizeof connect_addr);
    connect_addr.sin_family = AF_INET;

    memset(&icmp_hdr,0,sizeof icmp_hdr);
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.un.echo.id = getpid();

    timeout = {REPLY_TIMEOUT, 0};
}
int ping::do_ping(char ip_addr[INET_ADDRSTRLEN],
           bool is_tcp,
           uint16_t packet_count,
           uint16_t packet_length,
           uint16_t ping_timeout)
{
    char* datapart;
    uint8_t count_recived = 0;
    uint8_t count_transmitted = 0;

    inet_pton(AF_INET, ip_addr, &(connect_addr.sin_addr));

    int full_packet_length = packet_length+sizeof icmp_hdr;
    sprintf(datapart, "PING %s %i (%i) bytes of data.\n",ip_addr, packet_length,full_packet_length);
    textstream<<datapart;
    int sock = socket(AF_INET,is_tcp?SOCK_PACKET :SOCK_DGRAM,IPPROTO_ICMP);
    if (sock < 0) {
        textstream<<"can't open socket";
        return 0;
    }
    auto start_microtime = std::chrono::system_clock::now().time_since_epoch();
    for(uint16_t i=0;i<=packet_count;i++){
        char* data = 0;
        char* data_in = 0;
        int send_length;
        int recive_length;
        fd_set read_set;
        socklen_t sock_len;
        struct icmphdr recive_header;

        icmp_hdr.un.echo.sequence = i;

        memcpy(data, &icmp_hdr, sizeof icmp_hdr);
        memcpy(data + sizeof icmp_hdr, "test", packet_length);
        auto microtime = std::chrono::system_clock::now().time_since_epoch();
        send_length = sendto(sock, data, sizeof icmp_hdr + packet_length,
                          0, (struct sockaddr*)&connect_addr, sizeof connect_addr);
          if (send_length <= 0) {
              textstream<<"can't send data in socket\n";
              break;
          }

          count_transmitted++;

          memset(&read_set, 0, sizeof read_set);
          FD_SET(sock, &read_set);


         recive_length = select(sock + 1, &read_set, NULL, NULL, &timeout);
         if (recive_length == 0) {
             memset(&datapart,0,sizeof datapart);
             sprintf(datapart, "no reply recived in %i\n",REPLY_TIMEOUT);
             textstream<<datapart;
             continue;
         } else if (recive_length < 0) {
             textstream<<"reply read error\n";
             continue;
         }

         sock_len = 0;
         recive_length = recvfrom(sock, data_in, sizeof data_in, 0, NULL, &sock_len);
         if (recive_length <= 0) {
             textstream<<"reply read error\n";
             break;
         } else if (recive_length < (int)sizeof recive_header) {
             memset(&datapart,0,sizeof datapart);
             sprintf(datapart, "Error, got short ICMP packet, %i bytes\n",recive_length);
             textstream<<datapart;
             break;
         }

         memcpy(&recive_header, data_in, sizeof recive_header);
         //after request
         double timemsec = std::chrono::duration_cast<std::chrono::microseconds>(microtime).count()/1000;
         rtts.push_back(timemsec);
         if (recive_header.type == ICMP_ECHOREPLY) {
             memset(&datapart,0,sizeof datapart);
             sprintf(datapart, "echo: %i(%i) bytes from %s icmp_seq=%i time=%s ms\n",
                     (int)(sizeof(data_in) - sizeof(recive_header)),
                     (int)sizeof(data_in),
                     ip_addr,
                     recive_header.un.echo.sequence,
                     format_double_to_str(timemsec,2));
             textstream<<datapart;
             count_recived++;
         } else if (recive_header.type == ICMP_DEST_UNREACH) {
             textstream<<"Destination Unreachable\n";
             break;
         } else if(recive_header.type == ICMP_TIME_EXCEEDED){
             textstream<<"Time Exceeded\n";
         }else {
             textstream<<"Got ICMP packet with type "<<recive_header.type<<"\n";
         }
         usleep(ping_timeout);
    }
    double all_timemsec = std::chrono::duration_cast<std::chrono::microseconds>(start_microtime).count()/1000;

    textstream<<count_transmitted<<"packets transmitted, "<<count_recived<<" received, "
             <<format_double_to_str(get_loss_percent(count_transmitted,count_recived),2)<<"% packet loss, time "
             <<format_double_to_str(all_timemsec,2)<<"ms\n";

        rtt_stat rtt = getrtt();
            if(rtts.size()>0){
                textstream<<"rtt min/avg/max/mdev = "<<format_double_to_str(rtt.min,2)<<"/"
                                                     <<format_double_to_str(rtt.middle,2)<<"/"
                                                     <<format_double_to_str(rtt.max,2)<<"/"
                                                     <<format_double_to_str(rtt.mdev,2)<<" ms\n";
            }

    return 1;
}

double ping::get_loss_percent(int count_transmitted,int count_recived){
    return count_transmitted*(100-count_recived)/100;
}
const char* ping::format_double_to_str(double val,int precision){
    std::ostringstream ost;
    ost << std::fixed << std::setprecision(precision)<<val;
    return ost.str().data();
}
double ping::getRTTW(double old_rtt,double rtt){
    //rtt by https://ru.wikipedia.org/wiki/%D0%9A%D1%80%D1%83%D0%B3%D0%BE%D0%B2%D0%B0%D1%8F_%D0%B7%D0%B0%D0%B4%D0%B5%D1%80%D0%B6%D0%BA%D0%B0
    return (RTT_alpha * old_rtt) + ((1 - RTT_alpha ) * rtt);
}

ping::rtt_stat ping::getrtt(){
    ping::rtt_stat rtt;
    rtt.min = DBL_MAX;
    rtt.max = 0;
    double sqr_summ = 0;
    if(rtts.size()==0){
        return rtt;
    }
    for(unsigned int i=0;i<=rtts.size()-1;i++){
        sqr_summ+=rtts.at(i)*rtts.at(i);
        rtt.middle = getRTTW(rtt.middle,rtts.at(i));
        if(rtts.at(i)>rtt.max){
            rtt.max = rtts.at(i);
        }
        if(rtts.at(i)<rtt.min){
            rtt.min = rtts.at(i);
        }
    }
    rtt.mdev = sqrt(sqr_summ/rtts.size());
    return rtt;
}
