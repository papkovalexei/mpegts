CC=g++

all: mpegdecode

mpegdecode: main.o cbuffer.o cmpegts_parser.o protocol_func.o
	$(CC) main.o cbuffer.o cmpegts_parser.o protocol_func.o -o mpegdecode -lpthread -lcurl
cmpegts_parser.o: cmpegts_parser.cpp cmpegts_parser.hpp
	$(CC) -c cmpegts_parser.cpp
cbuffer.o: cbuffer.cpp cbuffer.hpp
	$(CC) -c cbuffer.cpp
protocol_func.o: protocol_func.hpp protocol_func.cpp
	$(CC) -c protocol_func.cpp
main.o: main.cpp protocol_func.hpp
	$(CC) -c main.cpp
clean:
	rm -rf *.o *.gch mpegdecode