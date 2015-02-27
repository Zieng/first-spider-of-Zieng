/*
 * spider.cpp
 *
 *  Created on: Feb 11, 2015
 *      Author: zieng
 *      thx to and refer to :http://blog.chinaunix.net/uid-27766566-id-3510062.html
 *	often get :400 bad request while run this program
 */
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

#define PORT 80
#define BUFSIZE 8192
static FILE * spidered;

int get_http(char *url);
void parse(char *buf);

int main(int argc,char * argv[])
{
	using namespace std;

	if(argc!=2)
	{
		cout<<"error!usage:input domain name!\n"<<endl;
		exit(-1);
	}
	spidered=fopen("spidered.txt","a+");
	if(spidered==NULL)
	{
		cout<<"open spidered failed!\n"<<endl;
		exit(-1);
	}
	if(get_http( argv[1] )!=0)
	{
		cout<<"get_http failed!\n"<<endl;
		exit(-1);
	}
	char nextUrl[BUFSIZE];
	while(fgets(nextUrl,BUFSIZE,spidered)!=NULL)
	{
		get_http(nextUrl);
	}

	return 0;
}

int get_http(char *url)
{
	using namespace std;

	int sockfd,ret;
	char *host_ip;
	struct hostent *host;
	struct sockaddr_in seraddr;
//	struct in_addr **addr_list;
//	socklen_t len;
	struct timeval tv;
	fd_set set;
	char command[BUFSIZE],buf[BUFSIZE];
//	char *str;

	host=gethostbyname(url);
	if(host==NULL)
	{
		cout<<"get host by %s failed!\n"<<url<<endl;
		return -1;
	}

/*	addr_list=(struct hostent *)host->h_addr_list;
	for(int i=0;addr_list[i]!=NULL;i++)     //last addr is the valid one?
		strcpy(host_ip,inet_ntoa(addr_list[i]));*/
	host_ip=inet_ntoa(    *(struct in_addr *)host->h_addr_list[0]      );
	cout<<"the ip address of "<<url<<"is:\n"<<host_ip<<endl;

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd <= 0)
	{
		cout<<"create socket failed!\n"<<endl;
		return -2;
	}
	bzero(&(seraddr.sin_zero),8);
	seraddr.sin_family=AF_INET;
	seraddr.sin_port=htons(PORT);
	seraddr.sin_addr.s_addr=inet_addr(host_ip);

	if(connect(sockfd,(struct sockaddr *)&seraddr,sizeof(seraddr)) < 0)
	{
		cout << "connect socket failed!!\n"<<endl;
		return -3;
	}
	cout <<"connect successful!\n"<<endl;

	memset(command,0,BUFSIZE);
	strcat(command,"GET / HTTP/1.1\r\n");
	//strcat(command,"Accept: *\r\n");
/*	strcat(command,"Accept-Language: zh-CN\r\n");
	strcat(command,"User-Agent: Mozilla/4.0\r\n");
	sprintf(command,"Host: %s\r\n",url);
	strcat(command,"Connection: Keep-Alive\r\n");*/
	strcat(command,"\r\n\r\n");
	cout<<"\n"<<command<<endl;

	ret=send(sockfd,command,strlen(command),0);
	if(ret<=0)
	{
		cout<<"send failed!\n"<<endl;
		return -4;
	}
	cout<<"send successful!\n"<<endl;

	while(1)
	{
		sleep(2);
		printf("*******************************\n");
		tv.tv_sec=0;
		tv.tv_usec=0;
		int h=0;

		FD_ZERO(&set);
		FD_SET(sockfd,&set);
		h=select(sockfd+1,&set,NULL,NULL,&tv);
		if(h==0)
			continue;
		else if(h<0)
		{
			close(sockfd);
			cout<<"something read error!\n"<<endl;
			return -5;
		}
		else
		{
			memset(buf,0,BUFSIZE);
			int i;
			i=recv(sockfd,buf,BUFSIZE,0);
			if(i==0)
			{
				close(sockfd);
				cout<<"find errors while reading messages!\n"<<endl;
				return -6;
			}
			parse(buf);
			cout <<"%s\n"<<buf<<endl;
		}

	}
	close(sockfd);

	return 0;
}

void parse(char *buf)
{
	char *qts,*pts=buf;

	while(   (  pts=strstr(pts,"a href=\"http:")   ) &&   (  qts=strstr(pts+9,"\"")  )   )
	{
		fwrite(pts+15,qts-pts-15,1,spidered);
		putc('\n',spidered);
		fflush(spidered);
		pts=qts;
	}
}
