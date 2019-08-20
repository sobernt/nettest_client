#include "ping.h"

#define PING_REPLY_TIMEOUT 3
#define OPTIMAL_PACKET_REPLY_TIMEOUT 2
#define RTT_alpha 0.9
Ping::Ping(){
    memset(&connect_addr,0,sizeof connect_addr);
    connect_addr.sin_family = AF_INET;

    memset(&icmp_hdr,0,sizeof icmp_hdr);
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.un.echo.id = getpid();
}
int Ping::do_ping(query_ping query_ping_in){
    return this->do_ping(query_ping_in.ip,
                         query_ping_in.is_tcp,
                         query_ping_in.packet_count,
                         query_ping_in.packet_length,
                         query_ping_in.test_time);
}
int Ping::do_ping(in_addr& ip_addr,
           bool& is_tcp,
           uint16_t& packet_count,
           uint16_t& packet_length,
           uint16_t& ping_timeout)
{
    char* datapart;
    timeout = {PING_REPLY_TIMEOUT, 0};
    reset_packet_stat();

    connect_addr.sin_addr = ip_addr;

    int full_packet_length = packet_length+sizeof icmp_hdr;
    sprintf(datapart, "PING %s %i (%i) bytes of data.\n",ip_addr, packet_length,full_packet_length);
    textstream<<datapart;
    sock = socket(AF_INET,is_tcp?SOCK_PACKET :SOCK_DGRAM,IPPROTO_ICMP);
    if (sock < 0) {
        textstream<<"can't open socket";
        return 0;
    }
    auto start_microtime = std::chrono::system_clock::now().time_since_epoch();
    for(uint16_t i=0;i<=packet_count;i++){
        ping_result ping_res = this->send_ping(i,packet_length);
        if(ping_res.result_code == ERROR_SOCKET_SEND){
            textstream<<"can't send data in socket\n";
            break;
        }
        switch (ping_res.result_code) {
        case ERROR_REPLY_READ_TIMEOUT:
            textstream<<"no reply recived in "<<PING_REPLY_TIMEOUT<<"\n";
            break;
        case ERROR_REPLY_READ:
            textstream<<"reply read error\n";
            break;
        case ERROR_DEST_UNREACH:
            textstream<<"Destination Unreachable\n";
            break;
        case ERROR_SHORT_ICMP:
            textstream<<"Error, got short ICMP packet\n";
            break;
        case ERROR_REPLY_ICMP_TIMEOUT:
            textstream<<"Time Exceeded\n";
            break;
        case SUCCESS:
            memset(&datapart,0,sizeof datapart);
            sprintf(datapart, "echo: %i(%i) bytes from %s icmp_seq=%i time=%s ms\n",
                     ping_res.reply_bytes,
                     ping_res.reply_bytes_full,
                     ip_addr,
                     ping_res.seq,
                     format_double_to_str(ping_res.req_time,2));
            textstream<<datapart;
            break;
        default:
            textstream<<"Got ICMP packet with unknown type\n";
            break;
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
                                                     <<format_double_to_str(rtt.mdev,2)<<" ms\n0.";
            }
    textstream<<EOF;
    return 1;
}

void Ping::OptimalFrameTest(){
    timeout = {OPTIMAL_PACKET_REPLY_TIMEOUT, 0};
    reset_packet_stat();
}

void Ping::reset(){
    textstream.flush();
    reset_packet_stat();
}
void Ping::reset_packet_stat(){

    count_recived = 0;
    count_transmitted = 0;
}

Ping::ping_result Ping::send_ping(uint16_t seq,uint16_t packet_length){
    char* data = 0;
    char* data_in = 0;
    int send_length;
    int recive_length;
    fd_set read_set;
    socklen_t sock_len;
    struct icmphdr recive_header;
    Ping::ping_result result;

    icmp_hdr.un.echo.sequence = seq;

    memcpy(data, &icmp_hdr, sizeof icmp_hdr);
    memcpy(data + sizeof icmp_hdr, "test", packet_length);
    auto microtime = std::chrono::system_clock::now().time_since_epoch();
    send_length = sendto(sock, data, sizeof icmp_hdr + packet_length,
                      0, (struct sockaddr*)&connect_addr, sizeof connect_addr);
      if (send_length <= 0) {
          result.result_code = ERROR_SOCKET_SEND;
          return result;
      }

      count_transmitted++;

      memset(&read_set, 0, sizeof read_set);
      FD_SET(sock, &read_set);


     recive_length = select(sock + 1, &read_set, NULL, NULL, &timeout);
     if (recive_length == 0) {
         result.result_code = ERROR_REPLY_READ_TIMEOUT;
         return result;
     } else if (recive_length < 0) {
         result.result_code = ERROR_REPLY_READ;
         return result;
     }

     sock_len = 0;
     recive_length = recvfrom(sock, data_in, sizeof data_in, 0, NULL, &sock_len);
     if (recive_length <= 0) {
         result.result_code = ERROR_REPLY_READ;
         return result;
     } else if (recive_length < (int)sizeof recive_header) {
         result.result_code = ERROR_SHORT_ICMP;
         result.reply_bytes = recive_length;
         return result;
     }

     memcpy(&recive_header, data_in, sizeof recive_header);
     double timemsec = std::chrono::duration_cast<std::chrono::microseconds>(microtime).count()/1000;
     rtts.push_back(timemsec);
     result.result_code = this->get_error_code(recive_header.type);
     if (result.result_code == SUCCESS) {
         result.reply_bytes = (int)(sizeof(data_in) - sizeof(recive_header));
         result.reply_bytes_full = (int)sizeof(data_in);
         result.seq = recive_header.un.echo.sequence;
         result.req_time = timemsec;
         count_recived++;
     }
    return result;
}
Ping::ping_result_code Ping::get_error_code(int header_type){
    switch (header_type) {
    case ICMP_DEST_UNREACH:
        return ERROR_DEST_UNREACH;
        break;
    case ICMP_TIME_EXCEEDED:
        return ERROR_REPLY_ICMP_TIMEOUT;
        break;
    case ICMP_ECHOREPLY:
        return SUCCESS;
        break;
    default:
        return ERROR_UNKNOWN_REPLY;
        break;
    }
}

double Ping::get_loss_percent(int count_transmitted,int count_recived){
    return count_transmitted*(100-count_recived)/100;
}
const char* Ping::format_double_to_str(double val,int precision){
    std::ostringstream ost;
    ost << std::fixed << std::setprecision(precision)<<val;
    return ost.str().data();
}
double Ping::getRTTW(double old_rtt,double rtt){
    //rtt by https://ru.wikipedia.org/wiki/%D0%9A%D1%80%D1%83%D0%B3%D0%BE%D0%B2%D0%B0%D1%8F_%D0%B7%D0%B0%D0%B4%D0%B5%D1%80%D0%B6%D0%BA%D0%B0
    return (RTT_alpha * old_rtt) + ((1 - RTT_alpha ) * rtt);
}

Ping::rtt_stat Ping::getrtt(){
    Ping::rtt_stat rtt;
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
