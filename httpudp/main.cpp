#include <errno.h>

#include "protocol_func.hpp"

#define PATH_TO_FIFO "/tmp/mpegtsfifo.1"
#define FIFO_BUFFER_SIZE 2048

int main(int argc, char *argv[])
{
    cmpegts_parser parser;
    cbuffer buffer{parser};

    unlink(PATH_TO_FIFO);
    int err = mkfifo(PATH_TO_FIFO, O_RDWR);
    if (err != 0) {
        std::cerr << strerror(err) << std::endl;
        return err;
    }

    chmod(PATH_TO_FIFO, S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR);

    std::cout << "FIFO: " << PATH_TO_FIFO << std::endl;

    int fifo_descriptor_read = open(PATH_TO_FIFO, O_RDONLY);
    int res = 0;

    do {
        std::vector<char> buffer_read(FIFO_BUFFER_SIZE);

        res = read(fifo_descriptor_read, buffer_read.data(), FIFO_BUFFER_SIZE);

        if (res != 0) {
            std::string command;
            std::vector<std::string> array_commands;

            command.resize(res);

            command = buffer_read.data();

            size_t pos = 0;
            std::string token;
            while ((pos = command.find('\n')) != std::string::npos) {
                token = command.substr(0, pos);
                
                if (token != "\n" && !token.empty()) {
                    array_commands.push_back(token);
                }
                command.erase(0, pos + 1);
            }
            if (command != "\n" && !command.empty()) {
                array_commands.push_back(command);
            }

            for (auto& cmd : array_commands) {
                if (cmd.back() == '\n') {
                    cmd.erase(cmd.begin() + cmd.size() - 1);
                }

                if (cmd == "exit") {
                    res = -1;
                    break;
                }
                buffer.clean();

                std::cout << "URL: " << cmd << std::endl;

                if (std::regex_match(cmd, std::regex("udp://.*"))) {
                    udp_protocol(buffer, cmd);
                } else if (std::regex_match(cmd, std::regex("http://.*"))) {
                    http_protocol(buffer, cmd);
                } else {
                    std::cout << "This protocol is not supported" << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    } while (res != -1);

    unlink(PATH_TO_FIFO);
    return 0;
}
