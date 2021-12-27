#ifndef H_BUFFER
#define H_BUFFER

#include "cmpegts_parser.hpp"

#define BUFFER_SIZE 1316
#define COUNT_PACKAGES_TO_CHECK 3

class cbuffer
{
public:
    cbuffer(cmpegts_parser &parser);
    ~cbuffer();

    int push_data(uint8_t *data, size_t size);
    bool get_pmt_ready() const;

    void clean();
private:
    uint8_t *_data;
    size_t _size;
    size_t _available;
    cmpegts_parser& _parser;
};

#endif
