//编译:
//g++ -ggdb3 -o x ./udp_broadcast.cpp 

//demo 意图:
//这里只是简单演示如何发送/接收广播数据, 不做回发.
//如果你要获取<广播数据发送方的ip and port 进行回发, 请参考:demo_udp_basic.cpp

//测试demo: (server = 广播数据接收者, client = 广播数据发送者)
//你在本机运行x (不带参数运行), 就是server and client 都开启的模式.<自收自发模式>
//然后你再开一个<同一局域网的虚拟机>, 运行x 123x 任意参数. 就会开启only server 模式.



//广播理论(简要):
//*1.协议栈对广播数据的过滤:
//   广播的方式来传递数据, 会增加'对广播不感兴趣的主机 的处理负荷'.
//   你可能会问: 不想接收广播数据的'监听socket', 设置socketopt 不接收广播数据即可?
//   答: 你以为'不设置'socketopt=广播模式', 主机就不会处理广播数据??
//       这样想是错的!! 所有arp 请求和应答, 都是广播数据.
//       而且你看看多播的实现, 多播的实现是对MAC 地址进行改造, 
//       才能在'ARP 数据包层'摆脱<不感兴趣的主机 的处理负荷 问题>, 
//       然而广播'并没有这样的设计'去避免这种情况发生.
//       所以无论你有没有'不设置'socketopt=广播模式',广播域内所有的主机,都会接收广播包
//       广播包会被拆包, 直到UDP 层, 找不到端口才会被抛弃!
//   (详情的过滤细节, 请看p12-广播与多播理论)

//*2.4 种IP 广播地址(即: 目的地址!!)
//   注意: 就算是广播数据, 源地址还是不会变的, 
//        每个IP 主机发数据的时候, 会自动附上自己的IP 地址, 除非你自己封包改变IP 地址

//   x1: 255.255.255.255[INADDR_BROADCAST], 即'受限'广播地址, 本局域网内有效

//   x2: netid.255.255.255 公网广播地址, 对 静态划分 的 公网网段 进行广播.
//       没有子网掩码限制的情况下, 路由器会转发这种广播报文, 但一般会屏蔽(少用).

//   x3: IP: 192.168.1.63, 子网掩码:255.255.255.224, 主机号全为1, 网络号为001.
//           192.168.1.[001 11111] 255.255.255.[111 00000]
//       (C 类子网前3 个数字为网络号, 后5 个数字为主机号.)

//       对单个子网进行广播(含子网掩码), 受子网掩码限制.
//       路由器转发这种报文, 也就是广播数据来源可以是: 其它网络对某个子网进行广播.

//   x4: IP: 192.168.1.255, 子网掩码:255.255.255.224, 主机号全为1, 网络号全为1.
//       这种情况于上面的x3 类似, 主要用来忽略子网掩码限制, 对整个子网络进行广播. 
//       这样即使'源主机'有 子网掩码限制, 但仍然能对 192.168.1.255 这个C 类网进行广播





#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

//由于本测试主机网络号为c 类网: 192.168.0.x/24, 即子网掩码为:255.255.255.0
//所以广播地址:x2 会与x4 重叠, x3 也没办法做.
//(我的路由器是简易路由器, 不支持修改'网段&&子网掩码')

//所以, 这里测试x1 受限广播地址
//广播IP 地址(发送者)
#define broadcast_x1_ip "255.255.255.255"//受限广播地址
#define broadcast_x2_x4_ip "192.168.0.255"//对公网C 类192.168.0 广播,全子网广播


//收听者的socket 绑定地址, 必须为: 0.0.0.0/255.255.255.255
//不能为主机本身的IP 地址.
//同时需要修改socketopt 设置socket 为'广播模式'
#define broadcast_bind_ip1 "0.0.0.0"
#define broadcast_bind_ip2 "255.255.255.255"


#define broadcast_port 6000//bind 的port必须与recvfrom() 的广播接收port 一样
                           //否则就收不到广播包
                           //ip 地址无关, 只要是同一个局域网的都可以
#define buf_len 512



//广播接收段(服务端)
int server(void){
  //创建udp socket
  int sfd = socket(PF_INET, SOCK_DGRAM, 0); 
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
  //bind_addr.sin_addr.s_addr = inet_addr("192.168.0.101");//fail!!
  //bind_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);//ok
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);//ok
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

    //根据广播地址接受消息
    int ret=recvfrom(sfd, smsg, buf_len, 0, \
            (struct sockaddr*)&bind_addr,(socklen_t*)&len);
    if(ret == -1){
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
  int cfd = socket(PF_INET, SOCK_DGRAM, 0); 
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
  //受限广播地址
  //addrto.sin_addr.s_addr=htonl(INADDR_BROADCAST);//ok
  //addrto.sin_addr.s_addr=htonl(INADDR_ANY);//ok
  //addrto.sin_addr.s_addr=inet_addr(broadcast_x1_ip);//ok

  //对公网C 类192.168.0 广播,全子网广播
  addrto.sin_addr.s_addr=inet_addr(broadcast_x2_x4_ip);//ok

  addrto.sin_port=htons(broadcast_port); 

  int nlen=sizeof(addrto);
  char smsg[] = {"hello server"};
  while(1){ 
    sleep(1);//休息1 秒
  
    //根据广播地址发送消息
    int ret=sendto(cfd, smsg, strlen(smsg), 0, (sockaddr*)&addrto, nlen); 
    if(ret == -1){ 
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

