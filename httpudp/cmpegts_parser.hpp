#ifndef H_CMPEGTS_PARSER
#define H_CMPEGTS_PARSER

#include <queue>
#include <array>
#include <mutex>
#include <thread>
#include <iostream>
#include <endian.h>
#include <cstring>

#define PACKET_SIZE 188
#define SYNC_BYTE 0x47

class cmpegts_parser
{
public:
    using packet_t = std::array<uint8_t, PACKET_SIZE>;

    cmpegts_parser();
    ~cmpegts_parser();

    void push_packet(uint8_t *data);

    uint16_t get_pmt_pid() const;
    bool get_pmt_ready() const;

    void clean();
private:
    void _analyzer_queue();
    void _analyzer_pmt(packet_t &packet);

    std::recursive_mutex _mutex_packets_queue;
    std::queue<packet_t> _packets_queue;

    std::thread _analyzer_thread;
    bool _analyzer_active;

    struct _program_map_section
    {
        std::vector<uint8_t> data;
        uint16_t PID;
        uint16_t length;
        std::recursive_mutex mutex;
        bool ready;
    } _pmt;
};

#endif
