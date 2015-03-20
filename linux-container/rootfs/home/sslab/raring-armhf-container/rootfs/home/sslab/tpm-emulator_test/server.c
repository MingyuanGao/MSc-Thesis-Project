#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


int main()
{
	int server_sockfd, client_sockfd;
	int server_addr_len, client_addr_len;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	
	unlink("server_socket");
	server_sockfd=socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family=AF_INET;
	//server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=10000;
	
	server_addr_len=sizeof(server_addr);
	bind(server_sockfd, (struct sockaddr *)&server_addr, server_addr_len);

	listen(server_sockfd, 5);
    
	signal(SIGCHLD, SIG_IGN);	
	
	while(1)
	{
		char command[50] = " ";
		char command_result[50] = "TPM Emulator testsuite has finished!\n";
       
		printf("server waiting ... \n");

		client_addr_len=sizeof(client_addr);
		client_sockfd=accept(server_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);

		if(fork()==0)
		{
			read(client_sockfd, &command, sizeof(command));
		   
            printf("%s", command);
			system("../testsuite/tsstests.sh -v 1.2");
		    	
			write(client_sockfd, &command_result, sizeof(command_result));
			printf("%s", command_result);
			close(client_sockfd);
			exit(0);
		}
		else
		{
			close(client_sockfd);
		}
	}
//main
}
