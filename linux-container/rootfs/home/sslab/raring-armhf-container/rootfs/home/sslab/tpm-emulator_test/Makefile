.Phony:all
all:client server
client:client.c
	gcc -o client client.c 
server:server.c
	gcc -o server server.c

.Phony:clean
clean:client server
	rm client server
