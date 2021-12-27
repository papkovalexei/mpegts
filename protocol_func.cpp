#include "protocol_func.hpp"

#define FIFO_BUFFER_SIZE 256

size_t write_callback(void *ptr, size_t size, size_t nmemb, cbuffer *buffer)
{
    if (!buffer->get_pmt_ready()) {
        for (size_t i = 0; i < size * nmemb;) {
            uint8_t *data = static_cast<uint8_t *>(ptr);
            i += buffer->push_data(data, size * nmemb);
        }
    }
    else {
        return CURLE_ABORTED_BY_CALLBACK;
    }
    return size * nmemb;
}

void http_protocol(cbuffer &buffer, std::string path_to_parse)
{
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, path_to_parse.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
}

void udp_protocol(cbuffer &buffer, std::string path_to_parse)
{
    path_to_parse = path_to_parse.substr(6);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        std::cerr << "Socket creation failed: " << std::strerror(errno) << std::endl;
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(path_to_parse.substr(path_to_parse.find(':') + 1).c_str()));
    inet_pton(AF_INET, path_to_parse.substr(0, path_to_parse.find(':')).c_str(), &(addr.sin_addr.s_addr));

    if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "Incorrect address: " << path_to_parse << std::endl;
        return;
    }
    struct ip_mreq mreq;
    inet_aton(path_to_parse.substr(0, path_to_parse.find(':')).c_str(), &(mreq.imr_multiaddr));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    sockaddr_in cs_addr;
    socklen_t cs_addrsize = sizeof(cs_addr);

    int res = 0;

    do {
        uint8_t buffer_read[PACKET_SIZE * CHUNK_READ];

        res = recv(sockfd, buffer_read, PACKET_SIZE * CHUNK_READ, 0);
        buffer.push_data(buffer_read, res);

        if (buffer.get_pmt_ready()) {
            break;
        }
    } while (res > 0);

    shutdown(sockfd, 2);
    close(sockfd);
}
