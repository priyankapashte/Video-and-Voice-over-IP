#include "camIpCli.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>           
#include <sys/stat.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/sendfile.h>
#include<unistd.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<signal.h>

struct stat stat_buf;
struct timespec start_time;
struct timespec end_time;
int dev=0,rc;
#define MAX_BUF  30000

struct sched_param nrt_param;
#define NUM_THREADS		2
#define SEND_VIDEO 		0
#define SEND_AUDIO 	    1
pthread_attr_t rt_sched_attr[NUM_THREADS];
pthread_t threads[NUM_THREADS];
struct sched_param rt_param[NUM_THREADS];

static int xioctl(int fd, int request, void* arg)
{
    int status;
    do 
    { 
        status = ioctl(fd, request, arg); 
    } 
    while (-1 == status && EINTR == errno);
    return status;
}

void* send_video(void*)
{ 
    int portServeur;
    sigset_t mask;
    size_t tailleAd;
    struct hostent* infosServeur = NULL;
    struct sigaction actQuit;
    struct sockaddr_in adServeur; 
    const char* VideoDeviceName = VideoDeviceName0; 
    int  status;

 

    sigemptyset(&mask);               
    actQuit.sa_mask = mask;
    actQuit.sa_handler = handlerQuitte;   
    sigaction(SIGINT, &actQuit, NULL);   
    sigaction(SIGQUIT, &actQuit, NULL); 
    sigaction(SIGHUP, &actQuit, NULL);  
    //portServeur = atoi(15000);           
    infosServeur = gethostbyname("10.0.0.2");  
    if (infosServeur == NULL)
    {
        perror("Erreur lors de la récupération des informations serveur\n");
        close(skDesc);
    }  
    adServeur.sin_family = infosServeur->h_addrtype;    
    adServeur.sin_port = htons(15000);            
    memcpy(&adServeur.sin_addr, infosServeur->h_addr, infosServeur->h_length);

    printf("\n\t***Beagleboard***\n\n");
    printf("Name: %s\n", infosServeur->h_name); 
    printf("Ip addr: %s\n", inet_ntoa(adServeur.sin_addr));
    printf("Send UDP\n");

    skDesc = ouvreSocket(PORTLOCAL);
    if (skDesc == -1)
    {
        perror("ouvreSocket(PORTLOCAL) erreur ");
        
    }
    tailleAd = sizeof(adServeur); 

    int webcam = open(VideoDeviceName, O_RDWR|O_NONBLOCK, 0);
    if (-1 == webcam)
    {
        fprintf(stderr, "ERROR: cannot open the device %s\n", VideoDeviceName);
        exit(EXIT_FAILURE);
    }

    struct v4l2_capability cap;
    status = xioctl(webcam, VIDIOC_QUERYCAP, &cap);
    if (0 != status)
    {
        fprintf(stderr, "ERROR: ioctl VIDIOC_QUERYCAP returned status of %d\n", status);
    }
    if ( !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) )
    {
        fprintf(stderr, "ERROR: the device does not support image capture\n");
        exit(EXIT_FAILURE);
    }
    if ( !(cap.capabilities & V4L2_CAP_STREAMING) )
    {
        fprintf(stderr, "WARNING: the device does not support streaming\n");
    }

    struct v4l2_cropcap cropcap;
    memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    status = xioctl (webcam, VIDIOC_CROPCAP, &cropcap);
    if (0 == status)
    {
        struct v4l2_crop crop;
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;

        status = xioctl(webcam, VIDIOC_S_CROP, &crop);
        if (0 != status)
        {
            if (EINVAL==errno)
                fprintf(stderr, "WARNING: the device does not support cropping\n");
            else ;
        }
    } 
    else 
    {        
        // Errors ignored
    }
	
    struct v4l2_format form;
    memset(&form, 0, sizeof(form)); 
    form.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    form.fmt.pix.width       = 320; 
    form.fmt.pix.height      = 240;
    form.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    form.fmt.pix.field       = V4L2_FIELD_ANY;
    status = xioctl(webcam, VIDIOC_S_FMT, &form);
    if (0 != status)
    {
        fprintf(stderr, "ERROR: ioctl VIDIOC_S_FMT returned status of %d (format %d is not supported)\n", 
                status, (int)form.fmt.pix.pixelformat);
        exit(EXIT_FAILURE);
    }

    int min = form.fmt.pix.width * 2;
    if (form.fmt.pix.bytesperline < min)
        form.fmt.pix.bytesperline = min;
    min = form.fmt.pix.bytesperline * form.fmt.pix.height;
    if (form.fmt.pix.sizeimage < min)
        form.fmt.pix.sizeimage = min;

    if (cap.capabilities & V4L2_CAP_STREAMING)
    {
        struct buffer* buffers   = NULL;
        unsigned int   n_buffers = 0;
        unsigned int i;

        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count               = 4;
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory              = V4L2_MEMORY_MMAP;

        status = xioctl(webcam, VIDIOC_REQBUFS, &req);
        if (0 != status)
        {
            fprintf(stderr, "ERROR: ioctl VIDIOC_REQBUFS returned status %d\n", status);
            exit (EXIT_FAILURE);
        }
        if (req.count < 2) 
        {
            fprintf (stderr, "ERROR: Insufficient buffer memory on %s\n", VideoDeviceName);
            exit (EXIT_FAILURE);
        }
        n_buffers = req.count;
        buffers = (struct buffer *)calloc(req.count, sizeof(*buffers));
        if (0 == buffers) 
        {
            fprintf (stderr, "ERROR: Out of memory\n");
            exit (EXIT_FAILURE);
        }

        for (i = 0; i < n_buffers; ++i) 
	{
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = i;
            status = xioctl(webcam, VIDIOC_QUERYBUF, &buf);
            if (0 != status)
            {
                fprintf(stderr, "ERROR: ioctl VIDIOC_QUERYBUF for buffer %d returned status %d\n", i, status);
                exit (EXIT_FAILURE);
            }

            buffers[i].length = buf.length;
            buffers[i].start = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, webcam, buf.m.offset);
            if (MAP_FAILED == buffers[i].start)
            {
                fprintf(stderr, "ERROR: MemoryMap failed for buffer %d of %d\n", i, n_buffers);
                exit (EXIT_FAILURE);
            }
	} 
        for (i = 0; i < n_buffers; ++i)
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            buf.index       = i;

            status = xioctl (webcam, VIDIOC_QBUF, &buf);
            if (0 != status)
            {
                fprintf(stderr, "ERROR: ioctl VIDIOC_QBUF returned status %d\n", status);
                exit (EXIT_FAILURE);
            }
        }

        enum v4l2_buf_type type1;    
        type1 = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        status = xioctl (webcam, VIDIOC_STREAMON, &type1);
        if (0 != status)
        {
            fprintf(stderr, "ERROR: ioctl VIDIOC_STREAMON returned status %d\n", status);
            exit (EXIT_FAILURE);
        }
        while (1)
        {
            struct timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;

            fd_set fds;
            FD_ZERO (&fds);
            FD_SET (webcam, &fds);
            status = select (webcam+1, &fds, NULL, NULL, &timeout);

            if (-1 == status)
            {
		    if (EINTR==errno)
		    {
			    continue;
		    }
		    fprintf (stderr, "ERROR: select failed\n");
		    exit (EXIT_FAILURE);
	    }
	    if (0 == status)
	    {
		    fprintf (stderr, "ERROR: webcam tiemout\n");
		    exit (EXIT_FAILURE);
	    }

	    struct v4l2_buffer buf;
	    memset(&buf, 0, sizeof(buf));
	    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    buf.memory = V4L2_MEMORY_MMAP;
	    status = xioctl(webcam, VIDIOC_DQBUF, &buf);
	    if (0 != status)
	    {
		    if (EAGAIN == errno) 
		    {
			    continue;
		    } 
		    else
		    {
			    fprintf(stderr, "ERROR: ioctl VIDIOC_DQBUF returned status %d\n", status);
			    exit(EXIT_FAILURE);
		    }
	    }

	    assert(buf.index < n_buffers);

	    char* buffer = (char *)buffers[buf.index].start;
	    int bytes_read = buffers[buf.index].length;
	    int size = bytes_read;
	    int i;
	    for (i = 0; i < size; i++)
	    {
		    if (((unsigned char)buffer[i] == 0xFF) && ((unsigned char)buffer[i + 1] == 0xC0))
		    {
			    break;
		    }
	    }

	    cameraFrame = (char *)realloc(cameraFrame, sizeof(char) * size + huffmanTablesSize);

	    int offset = i + 19;
	    memcpy(cameraFrame, buffer, offset);
	    memcpy(cameraFrame + offset, huffmanTables, huffmanTablesSize);
	    offset += huffmanTablesSize;
	    memcpy(cameraFrame + offset, buffer + i + 19, size - i - 19);
	    cameraFrameSize = size + huffmanTablesSize;
	    int j; 
	    for (j = 0; j < ((cameraFrameSize/TAILLE) + 1); j++)
	    {
		    if (j == cameraFrameSize/TAILLE)
		    {
			    retVal = sendto(skDesc, cameraFrame, sizeof(char) * (cameraFrameSize - (TAILLE * j)), 0, (struct sockaddr*) &adServeur, tailleAd);
			    if (retVal == -1)
			    {
				    perror("sendto");
				    close(skDesc);
			    }
			    cameraFrame = cameraFrame - (int)((j) * TAILLE); 
		    }
		    else
		    {
			    retVal = sendto(skDesc, cameraFrame, sizeof(char) * TAILLE, 0, (struct sockaddr*) &adServeur, tailleAd);
			    if (retVal == -1)
			    {
				    perror("sendto");
				    close(skDesc);
			    }
			    cameraFrame += TAILLE;
		    }
	    }
	

	    status = xioctl (webcam, VIDIOC_QBUF, &buf);
	    if (0!=status)
	    {
		    fprintf(stderr, "ERROR: ioctl VIDIOC_QBUF returned status %d\n", status);
	    }
	}

	enum v4l2_buf_type type2;
	type2 = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	status = xioctl(webcam, VIDIOC_STREAMOFF, &type2);
	if (0 != status)
	{
		fprintf(stderr, "ERROR: ioctl VIDIOC_STREAMON returned status %d\n", status);
		exit (EXIT_FAILURE);
	}
	
	for (i = 0; i < n_buffers; ++i)
		status = munmap(buffers[i].start, buffers[i].length);
	if (0 != status)
	{
		fprintf(stderr, "ERROR: MemoryMap unmapping failed\n");
		exit (EXIT_FAILURE);
	}
    }

    close(webcam);
    close(skDesc);

}

void handlerQuitte(int numSig)
{
	printf("\nDisconect\n");
	close(skDesc);
	exit(0);
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
		perror("Erreor socket\n");   
		return -1;
	}
	tailleAd = sizeof(adLocale);
	retVal = bind(skD, (struct sockaddr*)&adLocale, tailleAd); 
	if (retVal == -1)
	{
		perror("Erreor bind\n");
		close(skD);  
		return -1;
	}
	return skD;
}

void *send_audio(void*)
{
	struct sockaddr_in adServeur;
	 struct hostent* infosServeur = NULL;
	 int pipefd[2];
	 int sent;
	  char buf[MAX_BUF];
	 int ret;
	 pipe(pipefd);
	 int rd;
	
	 
	          char command[100];
			  int fd;
    infosServeur = gethostbyname("127.0.0.1");  
    if (infosServeur == NULL)
    {
        perror("Erreur lors de la récupération des informations serveur\n");
        close(skDesc);
    }  
    adServeur.sin_family = infosServeur->h_addrtype;    
    adServeur.sin_port = htons(15001);            
    memcpy(&adServeur.sin_addr, infosServeur->h_addr, infosServeur->h_length);

    printf("\n\t***Beagleboard***\n\n");
    printf("Name: %s\n", infosServeur->h_name); 
    printf("Ip addr: %s\n", inet_ntoa(adServeur.sin_addr));
    printf("Send UDP audio\n");
int fd1;
    skDesc = ouvreSocket(PORTLOCAL + 1);
	system("rm myfifo");
	char * myfifo = "myfifo";
	 mkfifo(myfifo, 0777);
	 //fd1=open("myfifo", O_RDONLY);
	while(1)
	{
	system("arecord -d 1 -D plughw:1,0 recording.wav");
	//sleep(1);
fd1=open("recording.wav", O_RDONLY);
   //fstat(fd1, &stat_buf);
   memset( (void*)buf, (int)'\0', MAX_BUF );
      
	  rd = read(fd1, buf, MAX_BUF);
	  printf("Read data = %d", rd);
	  //sent = sendfile(skDesc, fd1, NULL, stat_buf.st_size);
	  sent = sendto(skDesc, buf, MAX_BUF, 0, (struct sockaddr*) &adServeur, sizeof(adServeur));
	  printf("sent data= %d", sent);
	 close(fd1);
	}
	unlink(myfifo);
	close(skDesc);
	
}

int main(int argc, char** argv)
{
	int prio;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	
	prio = sched_get_priority_max(SCHED_FIFO);
	
 
    
    pthread_attr_init(&rt_sched_attr[SEND_VIDEO]);
    pthread_attr_setinheritsched(&rt_sched_attr[SEND_VIDEO], PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&rt_sched_attr[SEND_VIDEO], SCHED_FIFO);
    rt_param[SEND_VIDEO].sched_priority=prio-10;
    pthread_attr_setschedparam(&rt_sched_attr[SEND_VIDEO],&rt_param[SEND_VIDEO]);
	
	pthread_attr_init(&rt_sched_attr[SEND_AUDIO]);
    pthread_attr_setinheritsched(&rt_sched_attr[SEND_AUDIO], PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&rt_sched_attr[SEND_AUDIO], SCHED_FIFO);
    rt_param[SEND_AUDIO].sched_priority=prio-10;
    pthread_attr_setschedparam(&rt_sched_attr[SEND_AUDIO],&rt_param[SEND_AUDIO]);
    
	
printf("Creating SEND_VIDEO with num = %d\n", SEND_VIDEO);
   rc = pthread_create(&threads[SEND_VIDEO], &rt_sched_attr[SEND_VIDEO], send_video,NULL);
	pthread_setaffinity_np(threads[SEND_VIDEO],sizeof(cpu_set_t),&cpuset);
	
	
	
	printf("Creating thread SEND_AUDIO with num = %d\n", SEND_AUDIO);
   rc = pthread_create(&threads[SEND_AUDIO], &rt_sched_attr[SEND_AUDIO], send_audio, NULL);
	pthread_setaffinity_np(threads[SEND_AUDIO],sizeof(cpu_set_t),&cpuset);
	
	
	if(pthread_join(threads[SEND_VIDEO], NULL) == 0)
     printf("SEND_VIDEO done\n");
   else
     perror("SEND_VIDEO");
	
	if(pthread_join(threads[SEND_AUDIO], NULL) == 0)
     printf("SEND_AUDIO done\n");
   else
     perror("SEND_AUDIO");
	
	
	//send_audio();
	
	
}
