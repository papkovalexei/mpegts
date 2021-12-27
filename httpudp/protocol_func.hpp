#ifndef H_PROTOCOL_FUNC
#define H_PROTOCOL_FUNC

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <cstring>
#include <curl/curl.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex>
#include <fcntl.h>

#include "cbuffer.hpp"

#define CHUNK_READ 7

void http_protocol(cbuffer &buffer, std::string path_to_parse);
void udp_protocol(cbuffer &buffer, std::string path_to_parse);

#endif
