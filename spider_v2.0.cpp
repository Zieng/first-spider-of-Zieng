/*
 * spider_v2.0.cpp
 *
 *  Created on: Feb 13, 2015
 *      Author: zieng
 *  refer to and thx to :http://blog.csdn.net/huangxy10/article/details/8120106
 */

#include <string.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <string>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <queue>
#include <set>
#include <ext/hash_set>
#include <ext/hash_map>


#define PORT 80
#define DEFAULT_PAGE_BUF_SIZE 104857
#define BUFSIZE 2048

using namespace std;

queue<string> urlQueue;
//hash_set<string> visitedUrl;
set<string> visitedUrl;
//hash_set<string , hash<string> , comp > visitedUrl;
set<string> visitedImg;


bool get_http(const string &url,char *&content,unsigned long &bytesRead );
bool url_parse(const string &url,string &host,string &resource);
bool html_parse(string & content,vector<string> &imgUrl);
void BFS(const string & url);
void download_img(const string & url,vector<string> &imgUrl);
void download_html(const string & url,const string & html);
bool write_img(const char *p,unsigned long byteRead,string &imgName);

int main(int argc,char * argv[])
{
	using namespace std;

	static FILE * spidered;
	string startUrl;

	if(opendir("./img")==NULL)
		if(mkdir("./img",S_IRWXU)!=0)
			cout<<"create img folder failed!"<<endl;
	if(opendir("./html")==NULL)
		if(mkdir("./html",S_IRWXU)!=0)
			cout<<"create html folder failed!"<<endl;

	spidered=fopen("visitedUrl.txt","a+");
	if(spidered==NULL)
	{
		cout<<"fopen failed!\n"<<endl;
		return -1;
	}

	if(argc!=2)
	{
//		char choice;
		cout<<"You want to start from the default page , or from your page?"<<endl;
		cout<<"make your choices:from the default page?[y/n]"<<endl;
//		cin >>choice;
//		if(choice=='y')
			startUrl="http://hao.360.cn/meinvdaohang.html";
//		else
//			cin >> startUrl;
	}
	else
		startUrl=argv[1];

	/*​使用fwrite后，只是把内容写到了用户空间缓存区，并未同步到文件所以修改后要将内存与文件同步可以用fflush（FILE *fp）函数同步。*/
	fwrite(startUrl.c_str(),sizeof(char),strlen(startUrl.c_str()),spidered);
	fputc('\r',spidered);
	fputc('\n',spidered);
	fflush(spidered);
	BFS(startUrl);
	visitedUrl.insert(startUrl);
	while(urlQueue.size()!=0)
	{
		string url=urlQueue.front();
		std::cout <<"we are going to spider this url:\n"<<url.c_str()<<endl;
		sleep(2);
		BFS(url);
		const char *p=url.c_str();
		if(fwrite(p,sizeof(p),1,spidered)!=1)
		{
			cout<<"fwrite error!\n"<<endl;
//			return -1;
		}
		fputc('\r',spidered);
		fputc('\n',spidered);
		fflush(spidered);
		cout<<"url is write in the text!\n"<<endl;
		urlQueue.pop();
	}
	fclose(spidered);

	cout<<endl<<"------------------------------------------------------"<<endl;
	cout<<"Program finished!"<<endl;
	return 0;
}

bool get_http(const string &url,char *&content,unsigned long &bytesRead )
{
	using namespace std;

	int sockfd,ret;
	char *host_ip;
	struct hostent *host;
	struct sockaddr_in seraddr;
	struct timeval tv;
	fd_set set;
	string url_host;
	string url_resource;
	string command;

	if(url_parse(url,url_host,url_resource)==false)
	{
		cout<<"parse url failed!\n"<<endl;
		return false;
	}

	host=gethostbyname(url_host.c_str());
	if(host==NULL)
	{
		cout<<"get host by "<<url.c_str()<<" failed!\n"<<endl;
		return false;
	}
//	cout<<"We are going to connect to :"<<url_host.c_str()<<"..."<<endl;
	host_ip=inet_ntoa(    *(struct in_addr *)host->h_addr_list[0]      );
//	cout<<"the ip address of "<<url<<" is: "<<host_ip<<endl;

	sockfd=socket(AF_INET,SOCK_STREAM,0);
	if(sockfd <= 0)
	{
		cout<<"create socket failed!"<<endl;
		return false;
	}
	bzero(&(seraddr.sin_zero),8);
	seraddr.sin_family=AF_INET;
	seraddr.sin_port=htons(PORT);
	seraddr.sin_addr.s_addr=inet_addr(host_ip);

	if(connect(sockfd,(struct sockaddr *)&seraddr,sizeof(seraddr)) < 0)
	{
		cout << "connect socket failed!!"<<endl;
		return false;
	}
//	cout <<"connect successful!"<<endl;

//	memset(command,0,BUFSIZE);
	char * sendbuf=(char *)malloc(BUFSIZE);
	command=sendbuf;
	command="GET " + url_resource + " HTTP/1.1\r\nHost:" + url_host + "\r\nConnection:Close\r\n\r\n";

	ret=send(sockfd,command.c_str(),strlen(command.c_str()),0);
	if(ret<=0)
	{
		cout<<"send failed!\n"<<endl;
		return false;
	}
//	cout<<"send successful!\n"<<endl;
	free(sendbuf);

	tv.tv_sec=0;
	tv.tv_usec=0;
	int h=0;

	sleep(2);
	FD_ZERO(&set);
	FD_SET(sockfd,&set);
	h=select(sockfd+1,&set,NULL,NULL,&tv);
	if(h<0)
	{
		close(sockfd);
		cout<<"something read error!\n"<<endl;
		return false;
	}
	else
	{
		long contentLength=DEFAULT_PAGE_BUF_SIZE;
		char *buf=(char *)malloc(contentLength);
		memset(buf,0,contentLength);
		int ret=1;
		int bytesRead=0;
		while(ret>0)
		{
			ret=recv(sockfd,buf+bytesRead,contentLength-bytesRead,0);
			if(ret>0)
				bytesRead+=ret;
			if(contentLength-bytesRead < 128)
			{
				contentLength*=2;
				buf=(char *)realloc(buf,contentLength);
			}
		}
		buf[bytesRead]='\0';
		content=buf;
//			html_parse(content);
//		cout <<buf<<endl;
	}
	close(sockfd);

	return true;
}

bool url_parse(const string &url,string &host,string &resource)
{
	using namespace std;
	if(strlen(url.c_str()) > 1024)
	{
		cout<<"url too long !\n"<<endl;
		return false;
	}
	const char * front_pos=strstr(url.c_str(),"http://");
	if(front_pos==NULL)
	{
		cout<<"can not find the \"http://\"in the url!\n"<<endl;
		return false;
	}
	else
		front_pos+=strlen("http://");
	if(strstr(front_pos,"/")==0)
	{
		cout<<"can not find the end \"/\" in the url !\n"<<endl;
		return false;
	}
	char pHost[BUFSIZE];
	char pResource[BUFSIZE];
	sscanf(front_pos,"%[^/]%s",pHost,pResource);
	host=pHost;
	resource=pResource;

	return true;
}

bool html_parse(string &content,vector<string> & imgUrl)
{
	using namespace std;
	const char * html=content.c_str();

	cout<<"Start parsing the html ....."<<endl;
	const char * urlStartPos=strstr(html,"href=\"");
	while(urlStartPos)
	{
		urlStartPos+=strlen("href=\"");
		const char * urlEndPos=strstr(urlStartPos,"\"");

		if(urlEndPos)
		{

			if(urlEndPos <= urlStartPos)
			{
					urlStartPos=strstr(urlStartPos,"href=\"");
					continue;
			}
			char * newUrl=new char[urlEndPos-urlStartPos+1];
			sscanf(urlStartPos,"%[^\"]",newUrl);
//			cout<<"Find the url : "<<newUrl<<endl;
			string tempUrl=newUrl;
			if(visitedUrl.find(tempUrl)==visitedUrl.end())//indicate tempUrl not in visitedUrl
			{
				visitedUrl.insert(tempUrl);
				urlQueue.push(tempUrl);
			}

			delete [] newUrl;
		}
		urlStartPos=strstr(urlStartPos,"href=\"");
	}
	const char * imgFinder=strstr(html,"<img ");
	while(imgFinder)
	{
		imgFinder+=strlen("<img ");
		const char * imgUrlStart=strstr(imgFinder,"src=\"");
		/*note in html there may not be a src=" tag but a lazy-src=" tag instead */
		if(imgUrlStart==NULL) //之前没有对imgUrlStart==NULL加以判断
			continue;
		else
		{
			imgUrlStart+=strlen("src=\"");
//			cout<<"check the imgUrlStart: "<<imgUrlStart<<endl;
			const char * imgUrlEnd=strstr(imgUrlStart,"\"");
//			cout<<"check the imgUrlEnd: "<<imgUrlEnd<<endl;
			if( imgUrlEnd )
			{
				if(imgUrlEnd <= imgUrlStart)
				{
						imgFinder=strstr(imgFinder,"<img ");
						continue;
				}
				int uL=imgUrlEnd-imgUrlStart;
				char * imgScr=new char[uL+1];

				sscanf(imgUrlStart,"%[^\"]",imgScr);
				string img=imgScr;
//				cout<<"check the lenth of imgScr: "<<uL<<endl;
//				cout<<"check the imgScr: "<<imgScr<<endl;
//				cout<<"check the imgUrl: "<<img.c_str()<<endl;
				if(visitedImg.find(img)==visitedImg.end())
				{
					visitedImg.insert(img);
					imgUrl.push_back(img);
				}
				delete [] imgScr;
			}
		}
		imgFinder=strstr(imgFinder,"<img ");
	}
//	cout<<"end of parsing the html!\n"<<endl;
	return true;
}

void BFS(const string & url)
{
	char *content;
	unsigned long byte;
	if(get_http(url,content,byte)==false)
	{
		cout<<"url wrong ! ignore!\n"<<endl;
		return ;
	}
	string html=content;
	free(content);
	download_html(url,html);
	vector<string> imgUrl;
	html_parse(html,imgUrl);
	//check imgUrl
	/*cout<<"Now checking the imgUrl..."<<endl;
	for(unsigned int i=0;i<imgUrl.size();i++)
	{
		cout<<imgUrl[i].c_str()<<endl;
	}*/
	download_img(url,imgUrl);
}

string url_to_filename(const string & url)
{
	string t=strstr(url.c_str(),"http://");
	string s;
	s.resize(t.size());
	int k=0;
	for(unsigned int i=0;i<t.size();i++)
	{
		char ch=url[i];
		if(ch!='\\'&&ch!='/'&&ch!=':'&&ch!='*'&&ch!='?'&&ch!='"'&&ch!='<'&&ch!='>'&&ch!='|')
			s[k++]=ch;
	}
	return s.c_str();
}

void download_img(const string & url,vector<string> & imgUrl)
{
	using namespace std;

//	cout<<"Going to download the images ...."<<endl;
	string foldname="./img/"+url_to_filename(url);
//	cout<<"Creating the folder: "<<foldname.c_str()<<"...";
	if(opendir(foldname.c_str())==NULL) // indicate no such a folder so we can create it
//	{
		if(mkdir(foldname.c_str(),S_IRWXU)!=0)
		{
//			cout<<endl<<"create fold: "<<foldname.c_str()<<" failed!"<<endl;
			return ;
		}
//		cout<<"-------->>> OK!"<<endl;
//	}
//	else
//	{
//		cout <<endl<<"folder "<<foldname.c_str()<< " already exist!"<<endl;
//	}

	char *image;
	unsigned long byteRead;
//	cout<<"There are "<<imgUrl.size()<<" pictures to download...."<<endl;
	for(unsigned int i=0;i<imgUrl.size();i++)
	{
		string s=imgUrl[i];

		unsigned int pos=s.find_last_of(".");
		if(pos==string::npos)    /*说明查找没有匹配。string 类将 npos 定义为保证大于任何有效下标的值。*/
			continue;
		string type=s.substr(pos+1,s.size()-pos-1);

		/*check if its a picture*/
		if(type!="bmp"&&type!="jpg"&&type!="png"&&type!="gif"&&type!="jpeg"&&type!="tiff")
		{
			cout<<"not a valid image type! We will move on...."<<endl;
			continue;
		}


		//start downloading the picture
//		cout<<"Prepare downloading Pictures...."<<endl;
		if(get_http(s,image,byteRead)==true)
		{
			if(strlen(image)==0)
				continue;
			const char *p=image;
			const char *pos=strstr(p,"\r\n\r\n")+strlen("\r\n\r\n");  //clever
			unsigned int slashPos=s.find_last_of("/");
			if(slashPos!=string::npos)
			{
				string imgName=foldname+s.substr(slashPos,s.size());
				if(imgName.c_str()==NULL)
					continue;

				if(write_img(pos,byteRead,imgName)==false)
				{
					cout<<"can not write data in the picture correctly!"<<endl;
					continue;
				}
//				write_img(pos,byteRead,imgName);
//				cout<<"the length of data is :"<<strlen(pos)<<endl;
//				cout<<"picture named "<<imgName.c_str()<<" download successful!"<<endl;
			}

			free(image);
			image=NULL;
		}
	}
//	cout<<"Download completed!"<<endl;
}

void download_html(const string & url,const string & html)
{
	string filename="./html/"+url;
	FILE * fp;
	fp=fopen(filename.c_str(),"w+");
	if(fp==NULL)
	{
		cout<<"Create html "<<filename.c_str()<<" failed!"<<endl;
		return ;
	}
	if(fwrite(html.c_str(),html.size(),1,fp)!=1)
	{
		cout<<"write data in the "<<filename.c_str()<<" failed! We will move on...."<<endl;
		return ;
	}
	fflush(fp);
	fclose(fp);
}

bool write_img(const char *p,unsigned long byteRead,string &imgName)
{
	ofstream ofile;
	ofile.open(imgName.c_str(),ios::binary|ios::out|ios::trunc);
	ofile.write(p,byteRead);
	ofile.close();

	return true;
}
