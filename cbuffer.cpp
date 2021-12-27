#include "cbuffer.hpp"

#define ECANTFIND -2

cbuffer::cbuffer(cmpegts_parser &parser)
    : _parser(parser), _size(0), _available(BUFFER_SIZE)
{
    _data = new uint8_t[BUFFER_SIZE];
}

cbuffer::~cbuffer()
{
    delete[] _data;
}

int start_packet_index(uint8_t *buffer, size_t size)
{
    for (size_t i = 0; i < size - 2 * PACKET_SIZE; ++i) {
        if (*(buffer + i) == SYNC_BYTE && *(buffer + PACKET_SIZE + i) == SYNC_BYTE && *(buffer + 2 * PACKET_SIZE + i) == SYNC_BYTE) {
            return i;
        }
    }
    return ECANTFIND;
}

bool cbuffer::get_pmt_ready() const
{
    return _parser.get_pmt_ready();
}

void cbuffer::clean()
{
    _size = 0;
    _available = BUFFER_SIZE;
    _parser.clean();
}

int cbuffer::push_data(uint8_t *data, size_t size)
{
    int read = std::min(_available, size);

    memcpy(_data, data, read);
    _available -= read;
    _size = BUFFER_SIZE - _available;

    if (_size >= PACKET_SIZE * COUNT_PACKAGES_TO_CHECK) {
        int start_packet = start_packet_index(_data, _size);

        if (start_packet != ECANTFIND) {
            for (int i = start_packet; i < _size; i += PACKET_SIZE) {
                if (i + PACKET_SIZE <= _size) {
                    _parser.push_packet(_data + i);
                } else {
                    memcpy(_data, _data + i, _size - i);
                    _size -= i;
                    _available = BUFFER_SIZE - _size;

                    return read;
                }
            }
        }
    }

    _size = 0;
    _available = BUFFER_SIZE;
    return read;
}
