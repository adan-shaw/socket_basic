//编译:
//g++ -ggdb3 -o x ./multicast.cpp


#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>

#define mcast_port 6666
#define mcast_ip "224.0.0.12"//必须为多播段内的ip 地址才生效
#define buf_len 512



//多播接收端(服务端)
int server(void){
  //获取多播地址的host 信息(如果找不到这个IP/host 则会出错)
	struct hostent *host;
	host = gethostbyname(mcast_ip);
	if(host == NULL){
		printf("gethostbyname fail, errno = %d\n",errno);
		return -1;
	}
	

  //创建udp socket
	int sfd = socket(PF_INET, SOCK_DGRAM, 0);
	if(sfd == -1){
		printf("socket fail, errno = %d\n",errno);
		return -1;
	}
	
	// 调用bind之前, 设置套接口选项启用多播IP支持
	int opt_val = 1;
	opt_val = setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,
              &opt_val, sizeof(opt_val));
	if(opt_val == -1){
		printf("1.setsockopt fail, errno = %d\n",errno);
		return -1;
	}
	
  //设置sfd 多播的ttl 数值
	opt_val = 8;
	opt_val = setsockopt(sfd,IPPROTO_IP, IP_MULTICAST_TTL,
              &opt_val, sizeof(opt_val));
	if(opt_val == -1){
		printf("2.setsockopt fail, errno = %d\n",errno);
		return -1;
	}

  //设置sfd 多播, 不允许给<环回地址> 回送数据
	opt_val = 0;
	opt_val = setsockopt(sfd,IPPROTO_IP, IP_MULTICAST_LOOP,
              &opt_val, sizeof(opt_val));
	if(opt_val == -1){
		printf("3.setsockopt fail, errno = %d\n",errno);
		return -1;
	}


  //初始化udp bind 地址(多播接收端的地址信息)
  struct sockaddr_in local_addr;
	bzero(&local_addr, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);//默认网卡ip
	local_addr.sin_port = htons(mcast_port);//多播端口
  
  //执行绑定地址
	opt_val = bind(sfd,(struct sockaddr*)&local_addr, sizeof(local_addr)) ;
	if(opt_val == -1){
		printf("bind fail, errno = %d\n",errno);
		return -1;
	}
	

	//初始化多播ip and port 信息
  //进一步告诉Linux内核, 特定的套接口即将接受多播数据
  struct ip_mreq mreq;
	//mreq.imr_multiaddr.s_addr = htonl(INADDR_ANY);
  mreq.imr_multiaddr.s_addr = inet_addr(mcast_ip);//指定接收多播地址
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);//指定接收网络接口(默认网卡ip)
	if(mreq.imr_multiaddr.s_addr == -1){
		printf("%s not a legal multicast address\n",mcast_ip);
		return -1;
	}
	
  //将sfd 拉入一个多播组.
	opt_val = setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
              &mreq, sizeof(mreq));
	if(opt_val == -1){
		printf("4.setsockopt fail, errno = %d\n",errno);
		return -1;
	}
	
  
  //开始接收多播数据
  int iter = 0;
	while(iter++ < 8){
	  char message[buf_len];
    bzero(&message, sizeof(message));
		socklen_t sin_len = sizeof(local_addr);
		opt_val = recvfrom(sfd, message, buf_len, 0,\
                (struct sockaddr *)&local_addr, &sin_len);
    if(opt_val == -1){
	    printf("recvfrom fail, errno = %d\n",errno);
	    return -1;
    }
		printf("Response #%-2d from server: %s\n", iter, message);
		sleep(2); 
	}
	

  //sfd 退出多播组.
	opt_val = setsockopt(sfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
              &mreq, sizeof(mreq));
	if(opt_val == -1){
		printf("5.setsockopt fail, errno = %d\n",errno);
		return -1;
	}
	shutdown(sfd,2);
	close(sfd);

  return 0;
}



//多播发送端(客户端)
int client(void){
	//创建socket
	int cfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (cfd == -1){
		printf("socket fail, errno = %d\n",errno);
		return -1;
	}
	

	//初始化多播IP 地址
  struct sockaddr_in mcast_addr;
	memset(&mcast_addr, 0, sizeof(mcast_addr));
	mcast_addr.sin_family = AF_INET;
	mcast_addr.sin_addr.s_addr = inet_addr(mcast_ip);
	mcast_addr.sin_port = htons(mcast_port);

  char sbuf[] = "hi server, this is from cli-mcast";
	while(1) {
    //根据多播地址, 发送多播数据
    //注意: 只要向224.0.0.x 这个多播地址发送数据, 多播组内的主机都能收到.
    //     发送者甚至不需要加入多播组. 奇怪, 多播是怎样实现的??
		int n = sendto(cfd,&sbuf,sizeof(sbuf),0,
				    (struct sockaddr *)&mcast_addr, sizeof(mcast_addr)) ;
		if(n == -1){
	    printf("sendto fail, errno = %d\n",errno);
	    return -1;
    }

		sleep(2);
	}
	
  shutdown(cfd,2);
	close(cfd);
  return 0;
}



//测试主函数
int main(int argc, char* argv[]){
  //只要是非空参数输入, 则启动只接收模式
  if(argc >= 2){
    if(server() == -1){
      printf("server() fail\n\n");
        return -1;
    }
    return 0;
  }



  //自收自发模式
  int ppid = getpid();
  printf("father pid = %d\n",ppid);
  
  int pid = fork();
  if(pid == 0){
    //子进程
    if(client() == -1){
      printf("client() fail\n\n");
      return -1;
    }
  }
  else{
    printf("son pid = %d\n",pid);
    if(server() == -1){
      printf("server() fail\n\n");
        return -1;
    }
  }
  
  return 0;
}
