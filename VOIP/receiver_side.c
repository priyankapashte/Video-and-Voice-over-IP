#include "ServUDP.h"
#include <time.h>
#include <iostream>
#include <fcntl.h>  
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/sendfile.h>
#include<unistd.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<signal.h>

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>           
#include <sys/stat.h>
#include <sched.h>
struct timespec start_time;
struct timespec end_time;

int dev=0,rc;

struct sched_param nrt_param;
#define NUM_THREADS	    2
#define RECV_VIDEO 		0
#define RECV_AUDIO 	    1
pthread_attr_t rt_sched_attr[NUM_THREADS];
pthread_t threads[NUM_THREADS];
struct sched_param rt_param[NUM_THREADS];

void *recv_video(void*)
{ 
    int portLocal;
    sigset_t mask;
    size_t tailleAd;
    struct sockaddr_in adClient; 
    struct sigaction actQuit;
    unsigned char buffer[SIZE_BUFFER];
    char key;
    int rc;
    int err_jpeg =0;

    unsigned long jpg_size = JPEG_SIZE;
    struct jpeg_decompress_struct cinfo;                    
    struct jpeg_error_mgr jerr;
    Mat imageCam2;
    Mat imageCam(240,320, CV_8UC3);
   /*  if (argc != 2) 
    {
        fprintf(stderr, "Syntaxe incorrecte: %s NumPort\n", argv[0]);
        return -1;
    } */

    sigemptyset(&mask);             
    actQuit.sa_mask = mask;
    actQuit.sa_handler = handlerQuitte;   
    sigaction(SIGINT, &actQuit, NULL);   
    sigaction(SIGQUIT, &actQuit, NULL);  
    sigaction(SIGHUP, &actQuit, NULL);   
   // portLocal = atoi(argv[1]);
    skDesc = ouvreSocket(15000);
    if (skDesc == -1)
    {
        perror("ouvreSocket(portLocal) erreur");   
    }
    tailleAd = sizeof(adClient);
    printf("### Serveur beagleboard : %d ###\n", 15000);

    unsigned char * Image = (unsigned char *)malloc(sizeof(char *) * 614400); 
    while (key != 'Q' && key != 'q')
    {
        if (buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF)
        {
            for (int t = 0; t < 10/2 ; t++)
            {
                if (t == 9/2)
                {
                    memcpy(Image, buffer, sizeof(char) * SIZE_UDP_END);
                    Image -= SIZE_BUFF_UDP * t;
                }
                else
                {
                    memcpy(Image, buffer, sizeof(char) * SIZE_BUFF_UDP);
                    Image += SIZE_BUFF_UDP;
                }
                retVal = recvfrom(skDesc, &buffer, sizeof(char) * SIZE_BUFF_UDP, 0, 
                        (struct sockaddr*) &adClient, (socklen_t *)(&tailleAd));
                if (retVal == -1)
                {
                    perror("recvfrom");
                    close(skDesc);
                
                }

            }
            cinfo.err = jpeg_std_error(&jerr);	
            jpeg_create_decompress(&cinfo);

            jpeg_mem_src(&cinfo,(unsigned char *)Image, jpg_size);
            rc = jpeg_read_header(&cinfo, TRUE);
            if (rc != 1) 
            {
                //exit(EXIT_FAILURE);
            }

            jpeg_start_decompress(&cinfo);
            int width = cinfo.output_width;
            int height = cinfo.output_height;
            int pixel_size = cinfo.output_components;

            int bmp_size = width * height * pixel_size;
            bmp_size = width * height * pixel_size;
            bmp_buffer = (unsigned char*)malloc(bmp_size);
            int row_stride = width * pixel_size;
            while (cinfo.output_scanline < cinfo.output_height) 
            {
                unsigned char *buffer_array[1];
                buffer_array[0] = bmp_buffer + \
                                  (cinfo.output_scanline) * row_stride;
               err_jpeg = jpeg_read_scanlines(&cinfo, buffer_array, 1);
            }
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            
            memcpy(imageCam.data, bmp_buffer, bmp_size);
            cvtColor(imageCam,imageCam,CV_BGR2RGB); 
            resize(imageCam, imageCam2, Size(imageCam.cols * 2, imageCam.rows * 2), 0, 0,  INTER_LINEAR);
            imshow("Display", imageCam2);        
            key = cvWaitKey(1);
        }
        else{
            retVal = recvfrom(skDesc, &buffer, sizeof(char) * SIZE_BUFF_UDP, 0, 
                    (struct sockaddr*) &adClient, (socklen_t *)(&tailleAd));
            if (retVal == -1)
            {
                perror("recvfrom");
                close(skDesc);
            }
        }
		
    }
    return 0;
}

void handlerQuitte(int numSig)
{
    printf("\nDisconect : Done\n");
    close(skDesc);
    cvDestroyWindow("Display");
    exit (0);
}

int ouvreSocket(int port)
{
    int skD;
    size_t tailleAd;
    struct sockaddr_in adLocale;

    adLocale.sin_family = AF_INET;               
    adLocale.sin_port = htons(port);              
    adLocale.sin_addr.s_addr = htonl(INADDR_ANY);  
    skD = socket(AF_INET, SOCK_DGRAM, 0);          
    if (skD == -1)
    {
        perror("ErreOr socket\n");   
        return -1;
    }   
    tailleAd = sizeof(adLocale);
    retVal = bind(skD, (struct sockaddr*) &adLocale, tailleAd); 
    if (retVal == -1)
    {
        perror("Erreor bind\n");
        close(skD);    
        return -1;
    }
    return skD;
}

void *recv_audio(void*)
{
	size_t tailleAd;
	int rd;
	int fd;
    struct sockaddr_in adClient;
	char msg[30000];
	skDesc = ouvreSocket(15001);
    if (skDesc == -1)
    {
        perror("ouvreSocket(portLocal) erreur");   
    }
    tailleAd = sizeof(adClient);
    printf("### Serveur beagleboard : %d ###\n", 15001);
	//fd=open("myfifo1", O_WRONLY);
	while(1)
	{
int off=0;
fd=open("received.wav", O_WRONLY);
	memset( (void*)msg, (int)'\0', 30000 );
	rd=recvfrom(skDesc, &msg, sizeof(msg), 0, 
                    (struct sockaddr*) &adClient, (socklen_t *)(&tailleAd));
     printf("Read = %d", rd);
	
		
       //do
        //{
		
			
            int written = write(fd, &msg, rd);
            /* if (written < 1)
            {
                printf("Can't write to file");
                fclose(fd);
                return 0;
            }

            off += written; */
			printf("data written = %d", written);
        //}
        //while (off < rd);
close(fd);

system("aplay -D plughw:2,0 received.wav");
//sleep(1);
	}

	close(skDesc);
	
}

int main(int argc, char** argv)
{
	int prio;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	
	prio = sched_get_priority_max(SCHED_FIFO);
	

    
    pthread_attr_init(&rt_sched_attr[RECV_VIDEO]);
    pthread_attr_setinheritsched(&rt_sched_attr[RECV_VIDEO], PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&rt_sched_attr[RECV_VIDEO], SCHED_FIFO);
    rt_param[RECV_VIDEO].sched_priority=prio-10;
    pthread_attr_setschedparam(&rt_sched_attr[RECV_VIDEO],&rt_param[RECV_VIDEO]);
	
	pthread_attr_init(&rt_sched_attr[RECV_AUDIO]);
    pthread_attr_setinheritsched(&rt_sched_attr[RECV_AUDIO], PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&rt_sched_attr[RECV_AUDIO], SCHED_FIFO);
    rt_param[RECV_AUDIO].sched_priority=prio-10;
    pthread_attr_setschedparam(&rt_sched_attr[RECV_AUDIO],&rt_param[RECV_AUDIO]);
    
	
printf("Creating RECV_VIDEO with num = %d\n", RECV_VIDEO);
   rc = pthread_create(&threads[RECV_VIDEO], &rt_sched_attr[RECV_VIDEO], recv_video, NULL);
	pthread_setaffinity_np(threads[RECV_VIDEO],sizeof(cpu_set_t),&cpuset);
	
	
	
	printf("Creating thread RECV_AUDIO with num = %d\n", RECV_AUDIO);
   rc = pthread_create(&threads[RECV_AUDIO], &rt_sched_attr[RECV_AUDIO], recv_audio, NULL);
	pthread_setaffinity_np(threads[RECV_AUDIO],sizeof(cpu_set_t),&cpuset);
	
	
	if(pthread_join(threads[RECV_VIDEO], NULL) == 0)
     printf("RECV_VIDEO done\n");
   else
     perror("RECV_VIDEO");
	
	if(pthread_join(threads[RECV_AUDIO], NULL) == 0)
     printf("RECV_AUDIO done\n");
   else
     perror("RECV_AUDIO");
 //recv_audio();
	
}
