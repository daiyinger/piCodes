#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <linux/videodev2.h>
#include <asm/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define SERVERPORT 8086
#define SERVERIP "192.168.1.101"

typedef struct VideoBuffer
{
		void *start;
		size_t length;
}VideoBuffer;
typedef struct data
{
		unsigned int datasize;
		char buf[];
}buf_t;
buf_t *databuf;
static VideoBuffer *buffers=NULL;
pthread_mutex_t g_lock;
pthread_cond_t g_cond;
int fd;
//设置视频的制式及格式
int mark()
{
		int ret;
		struct v4l2_capability cap;//获取视频设备的功能
		struct v4l2_format fmt;

		do
		{
				ret=ioctl(fd,VIDIOC_QUERYCAP,&cap);
		}while(ret==-1 && errno==EAGAIN);

		if(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
		{
				printf("capability is V4L2_CAP_VIDEO_CAPTURE\n");
		}
		
		memset(&fmt,0,sizeof(fmt));
		fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;//数据流类型
		fmt.fmt.pix.width=320;//宽，必须是16的倍数 320 640
		fmt.fmt.pix.height=240;//高，必须是16的倍数 240 480
		fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;// 视频数据存储类型
		if (ioctl(fd, VIDIOC_S_FMT, &fmt) <0)
		{
				printf("set format failed\n");
				return -1;
		}

}

//申请物理内存
int localMem()
{
		int numBufs=0;
		struct v4l2_requestbuffers  req;//分配内存
		struct v4l2_buffer buf;
		req.count=4;
		req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory=V4L2_MEMORY_MMAP;
		buffers=calloc(req.count,sizeof(VideoBuffer));

		if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) 
		{
				return -1;
		}

		for(numBufs=0;numBufs<req.count;numBufs++)
		{
				memset(&buf,0,sizeof(buf));

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = numBufs;

				if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)//读取缓存信息 
				{
						printf("VIDIOC_QUERYBUF error\n");
						return -1;
				}
				buffers[numBufs].length=buf.length;
				buffers[numBufs].start=mmap(NULL,buf.length,
								PROT_READ | PROT_WRITE,
								MAP_SHARED,fd, buf.m.offset);//转换成相对地址
				if(buffers[numBufs].start==MAP_FAILED)
				{
						return -1;
				}
				if(ioctl(fd,VIDIOC_QBUF,&buf)==-1)//放入缓存队列
				{
						return -1;
				}
		}
}

void video_on()
{
		enum v4l2_buf_type type;
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
		if (ioctl (fd, VIDIOC_STREAMON, &type) < 0) 
		{
				printf("VIDIOC_STREAMON error\n");
				// return -1;
		}
}
void *pthread_video(void *arg)
{
		pthread_detach(pthread_self());
		//char bufs[1024*64];
		video_on();
		databuf=(buf_t *)malloc(sizeof(buf_t)+buffers[0].length); //bufs;
		while(1)
		{
			video();
		}
//		video_off();
		return NULL;
}

int video()
{
		struct v4l2_buffer buf;
		memset(&buf,0,sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = 0;
		
		while(1)
		{
			if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1)
			{
					return -1;
			}
			memcpy(databuf->buf,buffers[buf.index].start,buffers[buf.index].length);
			databuf->datasize=buf.bytesused;
			pthread_cond_signal(&g_cond);
			usleep(1000);
			//printf("send\n");
			if(ioctl(fd,VIDIOC_QBUF,&buf)==-1)
			{
					return -1;
			}
		}
		return 0;
}

void video_off()
{
		enum v4l2_buf_type type;
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		int ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
		if(ret ==-1)
		{
			printf("vidio OFF error!\n");
		}

		close(fd);
}
void *pthread_snd(void *socketsd)
{
		unsigned int count=0;
		pthread_detach(pthread_self());
		int sd=((int )socketsd);
		int newsd,ret,i=0;
		newsd=accept(sd,NULL,NULL);
		if(newsd==-1)
		{
			perror("accept");
			return NULL;
		}
		while(1)
		{
			pthread_mutex_lock(&g_lock);
			pthread_cond_wait(&g_cond,&g_lock);
			write(newsd,databuf->buf,databuf->datasize);
			if(ret==-1)
			{
					printf("client is out\n");
			}
			printf("send size:%5d count:%d \n",databuf->datasize,count++);
			pthread_mutex_unlock(&g_lock);
		}
		return NULL;
}

int main()
{
		signal(SIGPIPE,SIG_IGN);
		int ret;
		struct sockaddr_in server_addr;
		pthread_t tid;
		socklen_t addrlen;
		pthread_mutex_init(&g_lock,NULL);
		pthread_cond_init(&g_cond,NULL);

		server_addr.sin_family=AF_INET;
		server_addr.sin_port=htons(SERVERPORT);
		inet_pton(AF_INET,SERVERIP,&server_addr.sin_addr);
		addrlen=sizeof(struct sockaddr_in);
		int sd=socket(AF_INET,SOCK_STREAM,0);
		if(sd==-1)
		{
				perror("socket\n");
				exit(1);
		}
//		fd=open("dev/video0",O_RDWR | O_NONBLOCK,0);
		fd=open("/dev/video0",O_RDWR,0);
		if(fd==-1)
		{
				perror("open");
				return 0;
		}

		ret=bind(sd,(struct sockaddr *)&server_addr,addrlen);
		if(ret==-1)
		{
				perror("bind");
				exit(1);
		}
		ret=listen(sd,20);
		if(ret==-1)
		{
				perror("listen\n");
				exit(1);
		}
		fd_set fdsr;
		int maxsock=sd;
		struct timeval tv;
		mark();
		localMem();
		ret=pthread_create(&tid,NULL,pthread_video,NULL);
		while(1)
		{
				FD_ZERO(&fdsr);
				FD_SET(sd,&fdsr);

				tv.tv_sec=30;
				tv.tv_usec=0;
				ret=select(maxsock+1,&fdsr,NULL,NULL,NULL);
				if(ret<0)
				{
						perror("select");
						break;
				}
				else if(ret==0)
				{
						printf("timeout\n");
						continue;
				}
				if(FD_ISSET(sd,&fdsr))
				{
						ret=pthread_create(&tid,NULL,pthread_snd,((void *)sd));
				}
		}
		return 0;
}
