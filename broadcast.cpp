//编译:
//g++ -ggdb3 -o x ./broadcast.cpp 

//这里只是简单演示如何发送/接收广播数据, 不做回发.
//如果你要获取<广播数据发送方的ip and port 进行回发, 请参考:demo_udp_basic.cpp

//测试:
//你在本机运行x (不带参数运行), 就是server and client 都开启的模式.
//这是自收自发的, 这样你在一个机器中, 就实现了基本的<自收自发>

//然后你再开一个<同一局域网的虚拟机>, 运行x 123x 任意参数.
//这样就会开启only server 模式, 你就可以检验广播是否有效了.

//目的就是为了证明: 一个发送, n 个都能收到.


#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

//关于网络地址(如果你的本机有虚拟网卡, 那么bind 地址必须是虚拟网卡地址)
//发送/接收 广播包的地址是:INADDR_BROADCAST, 也可以是INADDR_ANY
#define broadcast_ip "192.168.0.101"//??

#define broadcast_port 6000//bind 的port必须与recvfrom() 的广播接收port 一样
                           //否则就收不到广播包
                           //ip 地址无关, 只要是同一个局域网的都可以
#define buf_len 512



//广播接收段(服务端)
int server(void){
  //创建udp socket
  int sfd = socket(AF_INET, SOCK_DGRAM, 0); 
  if(sfd == -1){ 
    printf("socket fail,errno = %d\n", errno);
    return -1; 
  }

  const int opt = 1; //设置该套接字为广播类型 
  int nb = setsockopt(sfd, SOL_SOCKET, SO_BROADCAST, \
                        (char *)&opt, sizeof(opt)); 
  if(nb == -1){ 
    printf("setsockopt fail,errno = %d\n", errno);
    return -1; 
  }


  //初始化udp bind 地址(广播接收端的地址信息)
  struct sockaddr_in bind_addr; 
  bzero(&bind_addr, sizeof(struct sockaddr_in)); 
  bind_addr.sin_family = AF_INET; 
  //bind_addr.sin_addr.s_addr = inet_addr(broadcast_ip);//不知道为什么失败??
  //bind_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_addr.sin_port = htons(broadcast_port); 

  //执行绑定地址
  nb=bind(sfd,(struct sockaddr*)&bind_addr, sizeof(struct sockaddr));
  if(nb == -1){ 
    printf("bind fail,errno = %d\n", errno);
    return -1; 
  }

  //接收广播数据
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  while(1){
    int len = sizeof(sockaddr);
    char smsg[buf_len] = {0};
    bzero(&smsg, sizeof(smsg));

    //从广播地址接受消息
    int ret=recvfrom(sfd, smsg, buf_len, 0, \
            (struct sockaddr*)&bind_addr,(socklen_t*)&len);
    if(ret<=0){
      printf("recvfrom fail,errno = %d\n", errno);
      return -1;
    }
    else
      printf("sfd %d recvfrom ok, len=%d, data:%s\n\n",\
        sfd,ret,smsg);

    sleep(1);
  }

  return 0;
}




//广播数据发送端(客户读)
int client(void){
  //创建udp socket
  int cfd = socket(AF_INET, SOCK_DGRAM, 0); 
  if (cfd== -1) { 
    printf("socket fail,errno = %d\n", errno);
    return -1;
  }

  const int opt = 1;//设置该套接字为广播类型
  int nb = setsockopt(cfd, SOL_SOCKET, SO_BROADCAST, \
             (char *)&opt, sizeof(opt)); 
  if(nb == -1){ 
    printf("setsockopt fail,errno = %d\n", errno);
    return -1;
  }

  
  //设置要发出广播的地址信息
  struct sockaddr_in addrto; 
  bzero(&addrto, sizeof(struct sockaddr_in)); 
  addrto.sin_family=AF_INET; 
  addrto.sin_addr.s_addr=htonl(INADDR_BROADCAST);//发送地址是广播地址 
  addrto.sin_port=htons(broadcast_port); 

  int nlen=sizeof(addrto);
  char smsg[] = {"hello server"};
  while(1){ 
    sleep(1);//休息1 秒
  
    //从广播地址发送消息 
    int ret=sendto(cfd, smsg, strlen(smsg), 0, (sockaddr*)&addrto, nlen); 
    if(ret<0){ 
      printf("sendto fail,errno = %d\n", errno);
      return -1;
    }
    else{ 
      printf("cfd %d sendto() ok, len = %d, data:%s\n\n",\
        cfd,ret,smsg); 
    }
  }

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
