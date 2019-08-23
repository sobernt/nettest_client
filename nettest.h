#ifndef NETTEST_H
#define NETTEST_H
#include <iostream>

    namespace std {
    enum nettest_command{
        nettest_command_default,
        nettest_command_exit,
        nettest_command_exit_req,
        nettest_command_ping,
        nettest_command_ping_reply,
        nettest_command_optimal_frame,
        nettest_command_optimal_frame_req
    };
        struct query_ping{
            uint16_t packet_count = 4;
            uint16_t packet_length = 54;
            uint16_t test_time = 10;
            bool is_tcp = true;
            struct in_addr ip;
        };
        struct nettest_header{
            nettest_command command;
            size_t size;
        };
        struct nettest_body{
            size_t size;
            uint16_t seq;
            uint16_t seq_count;
            char data[512];
        };
    }
#endif // NETTEST_H
