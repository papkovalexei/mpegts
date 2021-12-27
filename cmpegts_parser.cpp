#include "cmpegts_parser.hpp"

cmpegts_parser::cmpegts_parser()
    : _analyzer_active(true)
{
    _pmt.ready = false;
    _pmt.PID = 0;
    _analyzer_thread = std::thread([this]{_analyzer_queue();});
}

cmpegts_parser::~cmpegts_parser()
{
    _analyzer_active = false;
    _analyzer_thread.join();
}

void cmpegts_parser::push_packet(uint8_t *data)
{
    packet_t packet;
    memcpy(packet.data(), data, PACKET_SIZE);

    _mutex_packets_queue.lock();
    _packets_queue.push(packet);
    _mutex_packets_queue.unlock();
}

std::string h222_stream_type_str(unsigned s)
{
    switch (s) {
    case 0x00:
        return "Reserved";
    case 0x01:
        return "11172-2 video (MPEG-1)";
    case 0x02:
        return "H.262/13818-2 video (MPEG-2) or 11172-2 constrained video";
    case 0x03:
        return "11172-3 audio (MPEG-1)";
    case 0x04:
        return "13818-3 audio (MPEG-2)";
    case 0x05:
        return "H.222.0/13818-1  private sections";
    case 0x06:
        return "H.222.0/13818-1 PES private data (maybe Dolby/AC-3 in DVB)";
    case 0x07:
        return "13522 MHEG";
    case 0x08:
        return "H.222.0/13818-1 Annex A - DSM CC";
    case 0x09:
        return "H.222.1";
    case 0x0a:
        return "13818-6 type A";
    case 0x0b:
        return "13818-6 type B";
    case 0x0c:
        return "13818-6 type C";
    case 0x0d:
        return "13818-6 type D";
    case 0x0e:
        return "H.222.0/13818-1 auxiliary";
    case 0x0f:
        return "13818-7 Audio with ADTS transport syntax";
    case 0x10:
        return "14496-2 Visual (MPEG-4 part 2 video)";
    case 0x11:
        return "14496-3 Audio with LATM transport syntax (14496-3/AMD 1)";
    case 0x12:
        return "14496-1 SL-packetized or FlexMux stream in PES packets";
    case 0x13:
        return "14496-1 SL-packetized or FlexMux stream in 14496 sections";
    case 0x14:
        return "ISO/IEC 13818-6 Synchronized Download Protocol";
    case 0x15:
        return "Metadata in PES packets";
    case 0x16:
        return "Metadata in metadata_sections";
    case 0x17:
        return "Metadata in 13818-6 Data Carousel";
    case 0x18:
        return "Metadata in 13818-6 Object Carousel";
    case 0x19:
        return "Metadata in 13818-6 Synchronized Download Protocol";
    case 0x1a:
        return "13818-11 MPEG-2 IPMP stream";
    case 0x1b:
        return "H.264/14496-10 video (MPEG-4/AVC)";
    case 0x24:
        return "HEVC video stream";
    case 0x25:
        return "HEVC temporal video subset (profile Annex A H.265)";
    case 0x42:
        return "AVS Video";
    case 0x7f:
        return "IPMP stream";
    case 0x81:
        return "User private (commonly Dolby/AC-3 in ATSC)";
    default:
        if ((0x1c < s) && (s < 0x7e)) {
            return "H.220.0/13818-1 reserved";
        } else if ((0x80 <= s) && (s <= 0xff)) {
            return "User private";
        }
        return "Unrecognized";
    }
}

uint16_t cmpegts_parser::get_pmt_pid() const
{
    return _pmt.PID;
}

bool cmpegts_parser::get_pmt_ready() const
{
    return _pmt.ready;
}

void cmpegts_parser::clean()
{
    _pmt.mutex.lock();
    _pmt.length = 0;
    _pmt.PID = 0;
    _pmt.ready = false;

    if (!_pmt.data.empty()) {
        _pmt.data.clear();
    }

    while (!_packets_queue.empty()) {
        _packets_queue.pop();
    }
    _pmt.mutex.unlock();
}

void print_info_pmt(std::vector<uint8_t> &pmt)
{
    for (int i = 0; i < pmt.size() - 4;) { // Do not take into account CRC32 at the end
        std::cout << "PID 0x" << std::hex << (be16toh(*reinterpret_cast<uint16_t*>(pmt.data() + i + 1)) & 0x1fff)
                << " -> Stream type 0x" << (unsigned int)pmt[i] << " " << h222_stream_type_str((unsigned int)pmt[i]) << std::endl;

        uint16_t length_info = (be16toh(*reinterpret_cast<uint16_t*>(pmt.data() + i + 3)) & 0xfff); // 3 - offset to length bytes
        std::cout << "\tES info (" << std::dec << length_info << " bytes): ";

        for (int j = 0; j < length_info; j++) {
            std::cout << std::hex << (unsigned int)pmt[i + j + 5] << " ";
        }
        std::cout << std::endl;

        for (int j = 0; j < length_info;) {
            if (pmt[j + i + 5] == 0xa) { // Descriptor tag = Languages (5 - offset to Tag)
                std::cout << "\t Languages: " << (unsigned char)pmt[j + i + 7] << (unsigned char)pmt[j + i + 8] << (unsigned char)pmt[j + i + 9] << std::endl;
            }
            j += 1 + pmt[j + i + 6]; // 6 - offset to next descriptor
        }
        i += length_info + 5; // 5 - offset to next elemntary PID
    }
}

void cmpegts_parser::_analyzer_pmt(packet_t &packet)
{
    uint16_t index_table_id = 4; // Index table ID in packet
    uint16_t PID = (be16toh(*reinterpret_cast<uint16_t*>(&packet[1])) & 0x1FFF);

    if (((packet[3] & 0x30) >> 4) == 2 || ((packet[3] & 0x30) >> 4) == 3) { // Checking for the presence of the adaptation field
        index_table_id += packet[4] + 1;
    }

    if (((packet[1] & 0x40) >> 6) == 1) { // shift it if there is a point of the beginning of the data
        index_table_id++;
    }
    _pmt.mutex.lock();
    if (PID == 0x0) { // PAT table
        _pmt.PID = (be16toh(*reinterpret_cast<uint16_t*>(&packet[index_table_id + 10])) & 0x1FFF);
    }

    if (_pmt.PID != 0 && _pmt.PID == PID && packet[index_table_id] == 0x2 && _pmt.data.empty()) {
        uint16_t offset_to_descriptor = 9 + (be16toh(*reinterpret_cast<uint16_t*>(&packet[index_table_id + 10])) & 0xfff);
        _pmt.length = (be16toh(*reinterpret_cast<uint16_t*>(&packet[index_table_id + 1])) & 0xfff) - offset_to_descriptor;
        _pmt.data.clear();

        for (uint16_t i = 3 + index_table_id + offset_to_descriptor; i < packet.size(); ++i) {
            if (packet[i] == 0xff || (i + 1 == packet.size() && _pmt.data.size() + 1 == _pmt.length)) { // End of data
                _pmt.ready = true;
                try {
                    print_info_pmt(_pmt.data);
                } catch(const std::exception& e) {
                    std::cerr << e.what() << '\n';
                }
                break;
            } else if (i + 1 == packet.size() && _pmt.data.size() + 1 == _pmt.length) {
                _pmt.data.push_back(packet[i]);
                _pmt.ready = true;
                try {
                    print_info_pmt(_pmt.data);
                } catch(const std::exception& e) {
                    std::cerr << e.what() << '\n';
                }
                break;
            }
            _pmt.data.push_back(packet[i]);
        }
    } else if (_pmt.PID != 0 && _pmt.PID == PID
                && _pmt.data.size() != _pmt.length
                && _pmt.data.size() != 0
                && packet[index_table_id] != 0x2) { // start of the PMT table
        for (uint16_t i = index_table_id; i < packet.size(); ++i) {
            if (packet[i] == 0xff) { // End of data
                _pmt.length = _pmt.data.size();
                break;
            }
            _pmt.data.push_back(packet[i]);
        }

        _pmt.ready = true;
        try {
            print_info_pmt(_pmt.data);
        } catch(const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
    }
    _pmt.mutex.unlock();
}

void cmpegts_parser::_analyzer_queue()
{
    while (_analyzer_active) {
        if (!_packets_queue.empty()) {
            _mutex_packets_queue.lock();
            auto packet = _packets_queue.front();
            _packets_queue.pop();
            _mutex_packets_queue.unlock();

            if (packet[0] != 0x47 || (packet[1] & 0x80) >> 7) { // 0x47 - sync byte, bit mask for checking the packet correctness bit
                continue;
            }

            if (((packet[3] & 0x30) >> 4) == 2 || ((packet[3] & 0x30) >> 4) == 3) { // Checking for the presence of the adaptation field
                if (packet[4] + 4 >= packet.size()) {
                    continue;
                }
            }
            if (!_pmt.ready)
                _analyzer_pmt(packet);
            }
    }
}
