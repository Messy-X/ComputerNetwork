all:server client

server:server.cpp
	g++ -o server server.cpp -lpthread

client:client.cpp
	g++ -o client client.cpp -lpthread -std=c++11

.PHONY:clean
clean:
	rm -f server
	rm -f client
