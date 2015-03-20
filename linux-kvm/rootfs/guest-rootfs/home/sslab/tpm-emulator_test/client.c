#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/times.h>

void start_clock();
void end_clock();
int get_ticks_per_second();

static clock_t start_time;
static clock_t end_time;
static struct tms start_cpu;
static struct tms end_cpu;

void start_clock()
{
	start_time = times(&start_cpu);
}

void end_clock()
{
	end_time = times(&end_cpu);
//	printf("Real time:%jd, User time:%jd, System time: %jd\n",
//	       (intmax_t)(end_time -  start_time),
//		   (intmax_t)(end_cpu.tms_utime-start_cpu.tms_utime),
//		   (intmax_t)(end_cpu.tms_stime-start_cpu.tms_stime));
}

int get_ticks_per_second()
{
	return sysconf(_SC_CLK_TCK);
}


int main(int argc, char *argv[])
{
	int sockfd;
	struct sockaddr_in addr;
	int addr_len;
	int result;
	char command[50]="TPM Emulator testsuite has started ... \n";
    char command_result[50]=" ";

	sockfd=socket(AF_INET, SOCK_STREAM, 0);
	
	addr.sin_family=AF_INET;
    if(argc==1)
		addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    else 
		addr.sin_addr.s_addr=inet_addr(argv[1]);
	addr.sin_port=10000;
	addr_len=sizeof(addr);

	result=connect(sockfd, (struct sockaddr *)&addr, addr_len);

	if(result==-1)
	{
		perror("oops, client");
		exit(1);
	}
   
    start_clock();   
	printf("%s", command);
	write(sockfd, &command, sizeof(command));
	result = read(sockfd, &command_result, sizeof(command_result));
	end_clock();
	printf("The system has %d ticks per second.\n", get_ticks_per_second());
    printf("The TPM testsuite's execution time is %d ticks.\n",(intmax_t)(end_time-start_time));

	printf("%s\n", command_result);
	
	close(sockfd);
	
	exit(0);
}
