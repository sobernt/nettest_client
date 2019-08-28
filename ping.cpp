#include "ping.h"
#define PING_REPLY_TIMEOUT 3
#define OPTIMAL_PACKET_REPLY_TIMEOUT 2
#define RTT_alpha 0.9
Ping::Ping(){
    memset(&connect_addr,0,sizeof connect_addr);

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
    timeout = {PING_REPLY_TIMEOUT, 0};
    reset_packet_stat();



    int full_packet_length = sizeof(struct iphdr) + sizeof(struct icmphdr)+packet_length;
    char ip_str[INET_ADDRSTRLEN];
    textstream<<"PING "<< ip_str <<" "<<packet_length<<" ("<<full_packet_length<<") bytes of data.\n";
    switch (open_connection(is_tcp,ip_addr)) {
        case OPEN_SOCK_ERROR:
            textstream<<"can't open socket: "<<strerror(errno)<<endl;
            return 0;
            break;
        case BIND_ADDR_ERROR:
            textstream<<"bind error: "<<strerror(errno)<<endl;
            return 0;
            break;
        case LISTEN_EROR:
            textstream<<"can't listen socket:"<<strerror(errno)<<endl;
            return 0;
            break;
    }
    auto start_microtime = std::chrono::system_clock::now().time_since_epoch();
    for(uint16_t i=0;i<packet_count;i++){
        try{
            ping_result ping_res = this->send_ping(i,packet_length,is_tcp,ip_str);

            if(ping_res.result_code == ERROR_SOCKET_SEND){
                textstream<<"can't send data in socket\n";
                break;
            }
            switch (ping_res.result_code) {
            case ERROR_REPLY_READ_TIMEOUT:
                textstream<<"no reply recived in "<<PING_REPLY_TIMEOUT<<" s\n";
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
                    textstream<<"echo:  "<< ping_res.reply_bytes <<"("<<ping_res.reply_bytes_full<<")  bytes from "
                    <<ip_str<<" icmp_seq="<<ping_res.seq<<" time="<<format_double_to_str(ping_res.req_time,2)<<" ms\n";
                break;
            default:
                textstream<<"Got ICMP packet with unknown type\n";
                break;
            }
        } catch (int ex){
            textstream<<"Woops! app error code "<<ex<<".go out.\n";
            return 0;
        }
        usleep(ping_timeout);
    }
    double all_timemsec = std::chrono::duration_cast<std::chrono::microseconds>(start_microtime).count()/1000;

    textstream<<count_transmitted<<" packets transmitted, "<<count_recived<<" received, "
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

Ping::open_result_code Ping::open_connection(bool is_tcp,in_addr& ip_addr){
    bzero(&connect_addr,sizeof connect_addr);
    connect_addr.sin_addr = ip_addr;
    connect_addr.sin_family = AF_INET;
//    connect_addr.sin_port = 0;
    sock = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);

    if (sock < 0) {
        return OPEN_SOCK_ERROR;
    }
    /*
    * IP_HDRINCL must be set on the socket so that
    * the kernel does not attempt to automatically add
    * a default ip header to the packet
    */
    int optval = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int));
    setsockopt(sock, SOL_RAW, ICMP_FILTER, &optval, sizeof(int));

    //noblock socket
    fcntl(sock, F_SETFL, O_NONBLOCK);

//    if (connect(sock, (struct sockaddr*) &connect_addr, sizeof (connect_addr)) == -1) {
//        close(sock);
//        return BIND_ADDR_ERROR;
//    }

//    if (listen(sock, 10) == -1) {
//        close(sock);
//        return LISTEN_EROR;
//    }
    return OPEN_OK;
}

void Ping::OptimalFrameTest(){
    timeout = {OPTIMAL_PACKET_REPLY_TIMEOUT, 0};
    reset_packet_stat();
    uint16_t packet_sizes[] = {64,128,256,512,1024,2048};
}

void Ping::reset(){
    textstream.flush();
    reset_packet_stat();
}
void Ping::reset_packet_stat(){

    count_recived = 0;
    count_transmitted = 0;
}

Ping::ping_result Ping::send_ping(uint16_t seq,uint16_t packet_length,bool is_tcp,char ip_str[INET_ADDRSTRLEN]){
    struct icmphdr recive_header;
    struct iphdr ipc_header;
    struct icmphdr icmp_hdr;

    ping_result result;

    result.reply_bytes= 0;
    result.reply_bytes_full = 0;
    result.req_time = 0;
    result.seq = 0;
    result.result_code = ERROR_SOCKET_SEND;

    unsigned int packet_size = sizeof(struct iphdr) + sizeof(struct icmphdr)+packet_length;

    char data[packet_size];
    char data_in[packet_size];
    char payload[packet_length];



    bzero(data,sizeof data);
    bzero(data_in,sizeof data_in);
    bzero(&recive_header,sizeof recive_header);
    bzero(&result,sizeof result);
    int send_length = 0;
    int recive_length = 0;
    fd_set read_set;
    socklen_t sock_len;


    fill_payload(payload,sizeof payload);
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.code = 0;
    icmp_hdr.un.echo.id = getpid();
    icmp_hdr.un.echo.sequence = seq+1;
    memcpy(&icmp_hdr+sizeof (iphdr),payload,packet_length);
    icmp_hdr.checksum = 0;

    ipc_header.protocol = IPPROTO_ICMP;
    ipc_header.ttl = 64;
    ipc_header.version = 4;
    ipc_header.daddr = connect_addr.sin_addr.s_addr;//deistation addr
    ipc_header.saddr = 0;
    ipc_header.tot_len = sizeof ipc_header + sizeof icmp_hdr+packet_length;
    ipc_header.id = htons(seq);
    ipc_header.tos = 0;
    ipc_header.ihl = 5;
    ipc_header.frag_off = 0;

    memcpy(data, &ipc_header, sizeof ipc_header);
    memcpy(data+sizeof(ipc_header), &icmp_hdr, sizeof icmp_hdr);
    memcpy(data + sizeof(ipc_header)+sizeof(icmp_hdr), &payload, packet_length);

    icmp_hdr.checksum = ICMPChecksum((uint16_t *)&data+sizeof ipc_header, sizeof(struct icmphdr) + packet_length);
    memcpy(data+sizeof ipc_header, &icmp_hdr, sizeof icmp_hdr);

    auto microtime = std::chrono::system_clock::now().time_since_epoch();
    send_length = sendto(sock, data, packet_size,
                      0, (struct sockaddr*)&connect_addr,
                      sizeof connect_addr);
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
     } else{

     recive_length = recvfrom(sock, data_in, packet_size
                              , 0, (struct sockaddr *)&connect_addr, (socklen_t *)sizeof(connect_addr));
     if (recive_length <= 0) {
         result.result_code = ERROR_REPLY_READ;
         return result;
     } else if (recive_length < (int)sizeof recive_header) {
         result.result_code = ERROR_SHORT_ICMP;
         result.reply_bytes = recive_length;
         return result;
     } else{
         memcpy(&ipc_header, data_in, sizeof ipc_header);
         memcpy(&recive_header, data_in+sizeof ipc_header, sizeof recive_header);
         double timemsec = std::chrono::duration_cast<std::chrono::microseconds>(microtime).count()/1000000.0;
         rtts.push_back(timemsec);
         result.result_code = this->get_error_code(recive_header.type);
         if (result.result_code == SUCCESS) {
             result.reply_bytes = (int)(sizeof(data_in) - sizeof(recive_header));
             result.reply_bytes_full = (int)sizeof(data_in);
             result.seq = recive_header.un.echo.sequence;
             result.req_time = timemsec;
             count_recived++;
         }
     }
     return result;
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
    double result =(double)count_transmitted*(100-(double)count_recived);
    return result;
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

uint16_t Ping::ICMPChecksum(uint16_t *icmph, int len)
{
//    assert(len >= 0);

    uint16_t ret = 0;
    uint32_t sum = 0;
    uint16_t odd_byte;

    while (len > 1) {
        sum += *icmph++;
        len -= 2;
    }

    if (len == 1) {
        *(uint8_t*)(&odd_byte) = * (uint8_t*)icmph;
        sum += odd_byte;
    }

    sum =  (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    ret =  ~sum;

    return ret;
}


void Ping::fill_payload(char* data,size_t size){
    for(size_t i=0;i<=size;i++){
        data[i] = (char)rand();
    }
}
