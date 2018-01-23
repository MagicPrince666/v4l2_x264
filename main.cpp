#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>


#include "h264encoder.h"
#include "video_capture.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define DelayTime 33*1000//(33us*1000=33ms 30f/s)

#define DEVICE "/dev/video0"

#define SET_WIDTH 640
#define SET_HEIGHT 480

struct cam_data Buff[2];

pthread_t thread[2];
int flag[2],point=0;

int h264_length=0;
int framelength=0;
extern Encoder en;
struct camera *cam;

extern char h264_file_name[100];
extern FILE *h264_fp;
extern uint8_t *h264_buf;


static void init(struct cam_data *c);
void *video_Capture_Thread(void *arg);
void *video_Encode_Thread(void *arg);
void thread_create(void);
void thread_wait(void);


int main(int argc, char *argv[])
{
	remove("test.264");
	cam = (struct camera *) malloc(sizeof(struct camera));
	if (!cam) {
		printf("malloc camera failure!\n");
		exit(1);
	}
	cam->device_name = (char *)DEVICE;
	cam->buffers = NULL;
	cam->width = SET_WIDTH;
	cam->height = SET_HEIGHT;
	cam->fps = 30;//设置 30 fps

	framelength=sizeof(unsigned char)*cam->width * cam->height * 2;

	v4l2_init(cam);

	init(Buff);

	//创建线程
	printf("Making thread...\n");

	thread_create();

	printf("Waiting for thread...\n");

	thread_wait();

	printf("-----------end program------------");
	v4l2_close(cam);

	return 0;

}


static void
init(struct cam_data *c)
{
	flag[0]=flag[1]=0;

	c= (struct cam_data *)malloc(sizeof(struct cam_data ));

	pthread_mutex_init(&c->lock,NULL); //以动态方式创建互斥锁

	pthread_cond_init(&c->captureOK,NULL); //初始化captureOK条件变量

	pthread_cond_init(&c->encodeOK,NULL);//初始化encodeOK条件变量

	c->rpos=0;
	c->wpos=0;
}



void
*video_Capture_Thread(void *arg)
{
	compress_begin(&en, cam->width, cam->height);//初始化编码器

	int i=0;

	//unsigned char *data;

	int len=framelength;
	
	struct timeval now;

	struct timespec outtime;

	while(1)
	{
		usleep(DelayTime);

		gettimeofday(&now, NULL);

		outtime.tv_sec =now.tv_sec;

		outtime.tv_nsec =DelayTime * 1000;

		pthread_mutex_lock(&(Buff[i].lock)); /*获取互斥锁,锁定当前缓冲区*/
		if(i)   printf("----video_Capture_Thread Buff 1\n");
		if(!i)   printf("----video_Capture_Thread Buff 0\n");

		while((Buff[i].wpos + len)%BUF_SIZE==Buff[i].rpos && Buff[i].rpos != 0) /*等待缓存区处理操作完成*/
		{
			printf("***********video_Capture_Thread ************阻塞\n");
			//pthread_cond_wait(&(Buff[i].encodeOK),&(Buff[i].lock));
			pthread_cond_timedwait(&(Buff[i].encodeOK),&(Buff[i].lock),&outtime);
		}


		if(buffOneFrame(&Buff[i] , cam))//采集一帧数据
		{

			pthread_cond_signal(&(Buff[i].captureOK)); /*设置状态信号*/

			pthread_mutex_unlock(&(Buff[i].lock)); /*释放互斥锁*/
			//convert_yuv_to_rgb_buffer();

			flag[i]=1;//缓冲区i已满

			Buff[i].rpos=0;

			i=!i;	//切换到另一个缓冲区
			
			Buff[i].wpos=0;

			flag[i]=0;//缓冲区i为空


		}

		pthread_cond_signal(&(Buff[i].captureOK)); /*设置状态信号*/

		pthread_mutex_unlock(&(Buff[i].lock)); /*释放互斥锁*/
	}
}

void
*video_Encode_Thread(void *arg)
{
	int i=-1;

	while(1)
	{	
		if((flag[1]==0 && flag[0]==0) || (flag[i]==-1)) continue;

		if(flag[0]==1) i=0;

		if(flag[1]==1) i=1;

		pthread_mutex_lock(&(Buff[i].lock)); /*获取互斥锁*/
		if(i)   printf("-------------video_Encode_Thread Buff 1\n");
		if(!i)   printf("-------------video_Encode_Thread Buff 0\n");

		/*H.264压缩视频*/
		//encode_frame(Buff[i].cam_mbuf + Buff[i].rpos,0);
		int h264_length = 0;
		h264_length = compress_frame(&en, -1, Buff[i].cam_mbuf + Buff[i].rpos, h264_buf);
		
		if (h264_length > 0) {
	
			printf("%s%d\n","-----------h264_length=",h264_length);
	
			//写h264文件
			fwrite(h264_buf, h264_length, 1, h264_fp);
			//memcpy(fTo, h264_buf, h264_length);
		}

		Buff[i].rpos+=framelength;

		if(Buff[i].rpos>=BUF_SIZE) { Buff[i].rpos=0;Buff[!i].rpos=0;flag[i]=-1;}

		/*H.264压缩视频*/
		pthread_cond_signal(&(Buff[i].encodeOK));

		pthread_mutex_unlock(&(Buff[i].lock));/*释放互斥锁*/
	}
}

void
thread_create(void)
{
        int temp;

        memset(&thread, 0, sizeof(thread));  

		/*创建线程*/
        if((temp = pthread_create(&thread[0], NULL, video_Capture_Thread, NULL)) != 0)   
                printf("video_Capture_Thread create fail!\n");

        if((temp = pthread_create(&thread[1], NULL, video_Encode_Thread, NULL)) != 0)  
                printf("video_Encode_Thread create fail!\n");
}

void
thread_wait(void)
{
        /*等待线程结束*/
        if(thread[0] !=0) {  
                pthread_join(thread[0],NULL);
        }
        if(thread[1] !=0) {   
                pthread_join(thread[1],NULL);
        }
}
