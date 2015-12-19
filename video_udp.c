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

#define SERVERPORT 6666
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
int g_udp_fd;
struct sockaddr_in g_addr;

//设置视频的制式及格式
int setMark(void)
{
		int ret;
		struct v4l2_capability cap;		//获取视频设备的功能
		struct v4l2_format fmt;			//设置视频的格式
		//struct v4l2_control ctrl;		//

		do
		{
				ret=ioctl(fd,VIDIOC_QUERYCAP,&cap);	//查询驱动功能
		}while(ret==-1 && errno==EAGAIN);

		if(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)	//查询视频设备是否支持数据流类型
		{
			printf("capability is V4L2_CAP_VIDEO_CAPTURE\n");
		}
		
		memset(&fmt,0,sizeof(fmt));
		fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;//数据流类型
		fmt.fmt.pix.width=320;//宽，必须是16的倍数
		fmt.fmt.pix.height=240;//高，必须是16的倍数
		fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;// 视频数据存储类型
		if (ioctl(fd, VIDIOC_S_FMT, &fmt) <0)	//设置摄像头视频格式
		{
			printf("set format failed\n");
			return -1;
		}


}

//申请物理内存
int AllocMem(void)
{
	int numBufs=0;
	struct v4l2_requestbuffers  req;	//分配内存
	struct v4l2_buffer buf;				//V4L2视频Buffer

	req.count=4;											//缓存数量
	req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;					//缓存类型 数据流
	req.memory=V4L2_MEMORY_MMAP;							//
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

//开启视频采集
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

//视频采集线程函数
void *pthread_video(void *arg)
{
		pthread_detach(pthread_self());
		video_on();
		databuf=(buf_t *)malloc(sizeof(buf_t)+buffers[0].length);
		while(1)
		{
			video();
		}
//		video_off();
		return NULL;
}

//视频采集循环函数
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
			if(ioctl(fd,VIDIOC_QBUF,&buf)==-1)
			{
					return -1;
			}
		}
		return 0;
}
//关闭视频采集
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

//视频发送线程函数
void *pthread_snd(void *socketsd)
{
		pthread_detach(pthread_self());
		int newsd,ret,i=0;
		int len=0,img_len=0;
		int count = 0;
		unsigned char check_sum;
		char *imgbuf=(char *)calloc(1,1024*100);

		while(1)
		{
				pthread_mutex_lock(&g_lock);
				pthread_cond_wait(&g_cond,&g_lock);
				//printf("start\n");
				img_len = databuf->datasize;
				memcpy(imgbuf,databuf->buf,databuf->datasize);
				check_sum = 0;
				int cnt=0;
				int pos[5]={0};
				for(i = 0 ; i < img_len ; i++)
				{
					check_sum += imgbuf[i];
					if(imgbuf[i] == 0xFF)
					{
						if(imgbuf[i+1] == 0xD9)
						{
							pos[cnt++] = i;
						}
					}
				}
				if(cnt>1)
				{
					for(i=1;i<cnt;i++)
					{
						printf("S:%d:%d ",pos[cnt-1],cnt);
					}
					printf("\n");
				}
				imgbuf[img_len] = check_sum;
				imgbuf[img_len+1] = (databuf->datasize)&0xFF;
				imgbuf[img_len+2] = (databuf->datasize>>8)&0xFF;
				imgbuf[img_len+3] = (databuf->datasize>>16)&0xFF;
				imgbuf[img_len+4] = (databuf->datasize>>24)&0xFF;
				for(len = 0 ; len < img_len ; len += 1024)
				{
					if((img_len-len) > 1024)
					{
						ret = sendto(g_udp_fd,imgbuf+len,1024,0,(struct sockaddr *)&g_addr,sizeof(struct sockaddr_in)); 
					}
					else
					{
						ret = sendto(g_udp_fd,imgbuf+len,(img_len-len+5),0,(struct sockaddr *)&g_addr,sizeof(struct sockaddr_in)); 
					}
					if(ret==-1)
					{
							printf("client is out\n");
					}
					//usleep(1);
				}
				usleep(1);
				//printf("send size:%5d count:%d \n",databuf->datasize,count++);
				pthread_mutex_unlock(&g_lock);
		}
		free(imgbuf);
		return NULL;
}

//初始化UDP
int init_udp(int argc,char **argv)
{
	int sockfd; 
	struct sockaddr_in addr; 
	char ip_data[20];

	if(argc!=2) 
	{ 
		fprintf(stderr,"Usage:%s server_ip\n",argv[0]); 
		sprintf(ip_data,SERVERIP);
		fprintf(stderr,"Default ip %s\n",ip_data);
	}
   else
	{
		sprintf(ip_data,"%s",argv[1]);
         }
	/* 建立 sockfd描述符 */ 
	sockfd=socket(AF_INET,SOCK_DGRAM,0); 
	if(sockfd<0) 
	{ 
		fprintf(stderr,"Socket Error:%s\n",strerror(errno)); 
		exit(1); 
	} 

	/* 填充服务端的资料 */ 
	bzero(&addr,sizeof(struct sockaddr_in)); 
	addr.sin_family=AF_INET; 
	addr.sin_port=htons(8087);
	if(inet_aton(ip_data,&addr.sin_addr)<0)  /*inet_aton函数用于把字符串型的IP地址转化成网络2进制数字*/ 
	{ 
		fprintf(stderr,"Ip error:%s\n",strerror(errno)); 
		exit(1); 
	} 
	g_addr = addr;
	return sockfd;
}

int main(int argc,char **argv)
{
		signal(SIGPIPE,SIG_IGN);
		int ret;
		pthread_t tid;
		pthread_mutex_init(&g_lock,NULL);
		pthread_cond_init(&g_cond,NULL);
//		fd=open("dev/video0",O_RDWR | O_NONBLOCK,0);
		fd=open("/dev/video0",O_RDWR,0);	//打开视频设备
		if(fd == -1)
		{
			perror("open");
			return 0;
		}

		setMark();	//设置V4L2格式
		AllocMem();	//设置内存映射
		g_udp_fd = init_udp(argc,argv);	//初始化UDP客户端 获得socket描述符
		ret=pthread_create(&tid,NULL,pthread_video,NULL);	//创建视频采集线程
		pthread_create(&tid,NULL,pthread_snd,NULL);			//创建视频发送线程
		while(1)
		{
			usleep(1000);
		}
		return 0;
}
