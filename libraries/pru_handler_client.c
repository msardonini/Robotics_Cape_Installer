#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include "roboticscape.h"
#include "pru_handler_client.h"
#include <unistd.h>

#define NUM_CHANNELS 4

//Make this pthread variable global to this C file so we can join()
pthread_t pru_sender_thread;

int start_pru_client(pru_client_data_t* pru_client_data)
{
	//start the commands at zero
	int i;
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		pru_client_data->u[i] = 0;
	}

	//Check to see if the pru server is runningi, if not start it
	if(access(PRU_PID_FILE, F_OK) == -1)
	{
		printf("Pru handler is not runnining, starting a new process now");
		system("nohup pru_handler >/dev/null 2>&1 &");
		//Give it some time to initialize
		sleep(1);
		
		//Check again to make sure it's on
		if(access(PRU_PID_FILE, F_OK) == -1)
		{
			printf("Error! pru_handler will not start\n");
			return -1;
		}
	}
	if(pthread_create(&pru_sender_thread, NULL, pru_sender, pru_client_data))
	{
		printf("Error starting pru_sender thread\n");
		return -1;
	}
	return 0;
}

int join_pru_client()
{
	return pthread_join(pru_sender_thread, NULL);
}


void* pru_sender(void* ptr){
	
	pru_client_data_t *client_data = (pru_client_data_t*) ptr;
	int sockfd = 0;
    char sendBuff[16];
    struct sockaddr_in serv_addr; 

    memset(sendBuff, '0', sizeof(sendBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return NULL;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000); 

   // if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)<=0)
    if(inet_pton(AF_INET, "127.0.0.1" , &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return NULL;
    } 

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return NULL;
    } 	
	uint16_t tmp16 = 0x0000;
	int i;
    while (rc_get_state()!=EXITING)
    {
		if(client_data->send_flag)
		{				
			sendBuff[0] = 0xAA;
			sendBuff[1] = 0xBB;
			sendBuff[10] = 0xEE;
			sendBuff[11] = 0xFF;
			
			for (i = 0; i < NUM_CHANNELS ; i++) 
			{
				if (client_data->u[i] == 0.0f) tmp16 = 0;
				else tmp16 = (uint16_t)(client_data->u[i]*65536.0f)-1;
				
				sendBuff[2*i+2] = tmp16 & 0xFF;
				sendBuff[2*i+2] = tmp16 >> 8;
			}			
			write(sockfd, sendBuff, sizeof(sendBuff)-1);
			client_data->send_flag = 0;
		}
		usleep(2500); //run at 400hz just to check
    }
	
	//Issue the shutdown command to the pru handler
	for (i = 0; i < 4; i++)
	{
		sendBuff[0] = 0xAA;
		sendBuff[1] = 0xBB;
		sendBuff[10] = 0xEE;
		sendBuff[11] = 0xFF;
		sendBuff[2] = 's';
		sendBuff[3] = 'h';
		sendBuff[4] = 'u';
		sendBuff[5] = 't';
		sendBuff[6] = 'd';
		sendBuff[7] = 'o';
		sendBuff[8] = 'w';
		sendBuff[9] = 'n';
		write(sockfd, sendBuff, sizeof(sendBuff)-1);
		usleep(10000);
	}
	
	return NULL;
}
