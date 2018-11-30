/*********************************************************************************
* tcp6_echo_select, Test on CentOS Linux release 7.5.1804, gcc 4.8.5
* make tcp6_echo_select
* change log:
*  2018-11-30, code by xiaojianping(59090939@qq.com).
*  support ipv6, dynamic port.
*  
*
*  it used to test half connection、full connection, usage: tcp6_echo_select <port>
***********************************************************************************/

#include <stdio.h> 
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BACKLOG 128         //服务最大接受半连接数
#define MAX_CON 100         //服务最大连接数


int main(int argc, char *argv[])
{
	int serv_port;
	int sockfd;
	int err;
	int i;
	int connfd;
	int fd_all[MAX_CON]; //保存所有描述符，用于select调用后，判断哪个可读
	
	//下面两个备份原因是select调用后，会发生变化，再次调用select前，需要重新赋值
	fd_set fd_read;    //fd_set数据备份
	fd_set fd_select;  //用于select

	struct timeval timeout;         //超时时间备份     
	struct timeval timeout_select;  //用于select
	
	struct sockaddr_in6 serv_addr;   //服务器地址
	struct sockaddr_in6 cli_addr;    //客户端地址
	socklen_t serv_len;
	socklen_t cli_len;
	
	//超时时间设置
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	/*检查命令行参数*/	
	if (argc != 2) {
 	     fprintf(stderr, "usage: %s <port>\n", argv[0]);
      	exit(1);
   	 }
	serv_port = atoi(argv[1]);
	
	//创建TCP套接字
	sockfd = socket(AF_INET6, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		perror("fail to socket");
		exit(1);
	}
	
	 //配置本地地址
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin6_family = AF_INET6;	//ipv6
	serv_addr.sin6_port = htons(serv_port);	//端口
	serv_addr.sin6_addr = in6addr_any; 	//ip
	
	serv_len = sizeof(serv_addr);
	
	//绑定
	err = bind(sockfd, (struct sockaddr *)&serv_addr, serv_len);
	if(err < 0)
	{
		perror("fail to bind");
		exit(1);
	}

	//监听
	err = listen(sockfd, BACKLOG);
	if(err < 0)
	{
		perror("fail to listen");
		exit(1);
	}
	
	//初始化fd_all数组
	memset(fd_all, -1, sizeof(fd_all));

	fd_all[0] = sockfd;   //第一个为监听套接字
	
	FD_ZERO(&fd_read);	   //清空
	FD_SET(sockfd, &fd_read);  //将监听套接字加入fd_read

	int maxfd = fd_all[0];  //监听的最大套接字
	

	while(1){
	
		//每次都需要重新赋值，fd_select，timeout_select每次都会变
		fd_select = fd_read;
		timeout_select = timeout;
		
		//检测监听套接字是否可读，没有可读，此函数会阻塞
   		//只要有客户连接，或断开连接，select()都会往下执行
		err = select(maxfd+1, &fd_select, NULL, NULL, NULL);
		//err = select(maxfd+1, &fd_select, NULL, NULL, (struct timeval *)&timeout_select);
		if(err < 0)
		{
			perror("fail to select");
			exit(1);
		}

		if(err == 0){
			printf("timeout\n");
		}
		
		//检测监听套接字是否可读
		if( FD_ISSET(sockfd, &fd_select) ){ //可读，证明有新客户端连接服务器
        
        	cli_len = sizeof(cli_addr);
			
			//取出已经完成的连接
			connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
			if(connfd < 0)
			{
				perror("fail to accept");
				exit(1);
			}
			
			//打印客户端的 ip 和端口
			char cli_ip[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &cli_addr.sin6_addr, cli_ip, INET6_ADDRSTRLEN);
			printf("----------------------------------------------\n");
			printf("client ip=%s,port=%d\n", cli_ip,ntohs(cli_addr.sin6_port));
			
			//将新连接套接字加入 fd_all 及 fd_read
			for(i=0; i < MAX_CON; i++){
				if(fd_all[i] != -1){
					continue;
				}else{
					fd_all[i] = connfd;
					printf("client connect is[%d] join\n", i);
					break;
				}
			}
			
			FD_SET(connfd, &fd_read);
			
			if(maxfd < connfd)
			{
				maxfd = connfd;  //更新maxfd
			}
		
		}
		
		//从1开始查看连接套接字是否可读，因为上面已经处理过0（sockfd）
		for(i=1; i < maxfd; i++){
			if(FD_ISSET(fd_all[i], &fd_select)){
				printf("client connection[%d] is ok\n", i);
				
				char buf[1024]={0};  //读写缓冲区
				int num = read(fd_all[i], buf, 1024);
				if(num > 0){

					//收到客户端数据并打印
					printf("receive buf from client connection[%d] is: %s\n", i, buf);
					
					//回复客户端
					num = write(fd_all[i], buf, num);
					if(num < 0){
						perror("fail to write ");
						exit(1);
					}else{
						printf("send connection[%d] reply is: %s\n",i,buf);
					}
					
				}
				else if(0 == num){ //客户端断开时
					
					//客户端退出，关闭套接字，并从监听集合清除
					printf("client connection[%d] close！\n", i);
					FD_CLR(fd_all[i], &fd_read);
					close(fd_all[i]);
					fd_all[i] = -1;
					
					continue;
				}
				
			}else {
                //printf("no data\n");                  
            }
		}
	}
	
    return 0;
}
