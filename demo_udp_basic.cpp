//编译
//g++ -ggdb3 -o x ./demo_udp_basic2.cpp

//************
//本程序的意图:
//************
//根据客户端发上来的udp 数据包, 
//1.获取struct sockaddr_in peeraddr; 信息作为回发凭证,
//2.用struct msghdr *cmsg; 打印对方的ip 地址;
//3.打印接收到的数据
//4.回发数据, 打印回发端口

//ps: udp 服务器基本都需要这样获取接收到的数据来自哪个客户端ip and port, 然后进行回发





#include <netinet/in.h> // for IPPROTO_TCP
#include <arpa/inet.h> // for inet_addr
//#include <netinet/tcp.h>
//#include <netinet/udp.h>
#include <sys/types.h> // for socket
#include <sys/socket.h>

#include <sys/wait.h> // for waitpid

#include <stdio.h>
#include <unistd.h> // for close
#include <strings.h> // for bzero
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h> // for exit
#include <time.h> // for time
#include <string.h> // for strlen


//宏定义
#define srv_ip "127.0.0.1"//环回
//#define srv_ip "192.168.0.101"//局域网IP
#define srv_port 6666


//IP头(MAX=60字节) + [UDP头(max=8字节) / TCP头(max=60字节)]
//具体算法: IP头加上选项最长60 字节, UDP头固定8 字节, TCP头加上选项最长60 字节
//         没有比TCP 更长的传输控制协议了, ICMP,IGMP 都不够TCP 长.
//         IP/TCP 头有4 位控制'首部长度', 2^4=16, 每一个数值代表一节[4 字节32 bit]
//         16*4=64 字节, 实际最长60 字节
//因此: 协议ip头+[udp/tcp]头最大不会超过120 字节
//struct msghdr mh;
//mh.msg_control = cmbuf;
//mh.msg_controllen = sizeof(cmbuf);
//recvmsg 用mh 结构体组装, 发送时自动将'mh.msg_control' 指向的缓冲区当成协议头
#define io_proco_buf_max 128


//socket io 数据缓冲区(ps: 可以从发送的长度入手, 做udp MTU 实验)
//[标准802.2 MTU: 1492字节,以太网(802.3) MTU: 1500字节]
//[标准802.2 最小单次: 38字节,以太网(802.3) 最小单次: 46字节]自动填充[0 / '\0']
//基本只有udp 可能少于46 字节, 20+8=28,46-28=18 字节. 
//你发送少于18 字节的数据, 可能被填充.
//#define io_buf_max 512//外网MTU 保障
//#define io_buf_max 1364//局域网MTU 保障1492-128
//#define io_buf_max 1492//测试1
//#define io_buf_max 1500//测试2
//#define io_buf_max 1501//测试3
//#define io_buf_max 2048//测试4
//#define io_buf_max 32767//IP协议最长单次:2^16-1=65536-1,部分实现为一半32768-1
//#define io_buf_max 65536//测试5--sendto() fail, errno = 90
//#define io_buf_max 32768//测试6
#define io_buf_max 65500//测试7-max almost !



//************************************************************
//解包常用的结构体'解析'(如果找不到结构体, 直接读<netinet/in.h> 就能找到!!)
//直接在<netinet/in.h> 中搜索结构体名.
//程序能编译, 证明所有定义都在头文件中, 自己慢慢找...
//************************************************************
//1.socket io 数据缓冲区'主要描述体'
//  struct msghdr mh;(结构体太大, 不展示)
/*
struct msghdr {
  void            *msg_name;
  int             msg_namelen;
  struct iovec    *msg_iov;
  __kernel_size_t msg_iovlen;
  void            *msg_control;
  __kernel_size_t msg_controllen;
  unsigned        msg_flags;
};
*/



//2.[区分/识别]'主要描述体'的协议头部分:
//  ( 一般一个struct msghdr mh ;只有两个struct cmsghdr *cmsg; ) 
//  (主要从struct msghdr mh; -> mh.msg_control 中, 区分出IP/UDP/TCP/ICMP/IGMP)
/*
struct cmsghdr {
  socklen_t cmsg_len;
  int       cmsg_level;
  int       cmsg_type;
  u_char    cmsg_data[];
};
*/
//*** 识别是需要用到的宏 ***
//>2.1: cmsg = CMSG_FIRSTHDR(&mh);//第一个
//>2.2: cmsg = CMSG_NXTHDR(&mh, cmsg);//指向下一个
//>2.3: CMSG_DATA(cmsg); //提取缓冲区'cmsg_data[]'
//识别循环demo1:
//for(cmsg=CMSG_FIRSTHDR(&mh);cmsg != NULL;cmsg=CMSG_NXTHDR(&mh,cmsg)){}
//识别循环demo2:(简化版)
//for( cmsg=CMSG_FIRSTHDR(&mh); cmsg; cmsg=CMSG_NXTHDR(&mh,cmsg) ){}

//识别时常用的协议宏定义
//__cmsg_level: IPPROTO_IP | IPPROTO_UDP | IPPROTO_TCP | IPPROTO_ICMP
//              IPPROTO_IGMP | IPPROTO_STCP | IPPROTO_TIPC | ...?
//              IPPROTO_RAW	... 更多详情, 进在in.h for IPv4 !!

//__cmsg_type:  IP_PKTINFO(这个应该是唯一的, 与'socketopt 选项'对应!) 
//              UDP_PKTINFO | TCP_PKTINFO (没有这样的!!error !!)



//3.读取 选中的协议块'struct cmsghdr' 中的内容
//struct sockaddr_in, udp/tcp 协议定义容器
//struct in_pktinfo *pi //ip 协议定义容器
/*

//不知道是不是??
struct sockaddr_in {
  sa_family_t sin_family;//Address family
  __be16 sin_port;//Port number
  struct in_addr sin_addr;//Internet address
  //Pad to size of `struct sockaddr'.
  unsigned char __pad[__SOCK_SIZE__ - sizeof(short int) -
			sizeof(unsigned short int) - sizeof(struct in_addr)];
};


//肯定是!!
//不懂看网文: http://www.cnblogs.com/kissazi2/p/3158603.html
struct in_pktinfo{
  int ipi_ifindex;//接口索引--接收数据的接口
  struct in_addr ipi_spec_dst;//路由目的地址-destination address of the packet
  struct in_addr ipi_addr;//头标识目的地址-source address of the packet
};
//这种方法只能用于UDP(数据报)传输中
*/






//全局变量
int sfd_li = 0;
int sfd_cli = 0;
int sfd_acc = 0;


//函数前置声明
//自收自发测试
int io2himself(void);

//退出回收资源
void inline free_test_sfd(void);



//测试主函数
int main(void){
  //创建udp 报式 socket
  sfd_li = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sfd_li == -1){
		printf("socket() fail, errno = %d\n", errno);
		return -1;
	}
  //创建udp 报式 socket for client
  sfd_cli = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sfd_cli == -1){
		printf("socket() fail, errno = %d\n", errno);
    close(sfd_li);
		return -1;
	}
  //创建udp 报式 socket for accept client
  sfd_acc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sfd_acc == -1){
		printf("socket() fail, errno = %d\n", errno);
    close(sfd_li);
    close(sfd_cli);
		return -1;
	}



  //设置地址重用 -- 不设置重用, 可能导致二次调用程序的时候, bind() 失败
  //bind() socket 最好设置重用!! 因为系统使用端口之后, 需要等几秒才会再次投入使用.
  //真实服务器环境中, 也需要对动态端口设置重用.
  //但是动态端口有1w 多个, 这里测试只有2 个动态端口, 端口十分充足, 
  //为了节省代码, 就不做重用了.
  int opt_val = true;
  opt_val = setsockopt(sfd_li, SOL_SOCKET, SO_REUSEADDR, \
							&opt_val, sizeof(int));
	if(opt_val == -1){
		printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}



  //*************************
  //双方都设置--读解包, 即recvmsg. sendto + recvmsg 的组合是可行的.
  //如果你需要用sendmsg, 那就属于包重新封装了.
  //请使用AF_INET协议族, socket类型 = SOCK_DGRAM, 确保sfd = udp socket
  opt_val = true;
  opt_val = setsockopt(sfd_li, IPPROTO_IP, IP_PKTINFO, \
							&opt_val, sizeof(int));
	if(opt_val == -1){
		printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}

  opt_val = true;
  opt_val = setsockopt(sfd_cli, IPPROTO_IP, IP_PKTINFO, \
							&opt_val, sizeof(int));
	if(opt_val == -1){
		printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}

  //opt_val = true;//回发的socket 可以不用改解包属性...
  //opt_val = setsockopt(sfd_acc, IPPROTO_IP, IP_PKTINFO, \
							&opt_val, sizeof(int));
	//if(opt_val == -1){
		//printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		//free_test_sfd();
		//return -1;
	//}
  //*************************



  //执行bind
	struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr_in));
  addr.sin_addr.s_addr = inet_addr(srv_ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(srv_port);

  //sockaddr_in 可以直接强转为struct sockaddr
  opt_val = bind(sfd_li,(struct sockaddr*)&addr,\
                  sizeof(struct sockaddr));
  if(opt_val == -1){
    printf("socket %d bind fail, errno: %d\n", sfd_li, errno);
    free_test_sfd();
    return -1;
  }



  //执行自收自发操作
  if(io2himself() == 0)
    printf("io2himself() ok!!\n");
  else
    printf("io2himself() fail!!\n");



  //回收资源
  free_test_sfd();
  printf("father pthread quit\n");
  return 0;
}





//自收自发测试
int io2himself(void){
  //**********************
  //client send to server
  //**********************
  //设置发送接受地址
  struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr_in));
	//addr.sin_addr.s_addr = inet_addr(srv_ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(srv_port);


  //组装发送数据
  char sbuf[io_buf_max];//发送缓冲区
  bzero(&sbuf, io_buf_max);
  snprintf(sbuf, io_buf_max, \
    "hello server, i am the client. %d\n",time(NULL));

  //执行发送操作
  //发送'实际字符串长度'
  //int opt_val = sendto(sfd_cli, &sbuf, strlen(sbuf), 0,\
                       (struct sockaddr*)&addr, sizeof(addr));
  //发送'整个缓冲区的总长'
  int opt_val = sendto(sfd_cli, &sbuf, sizeof(sbuf), 0,\
                       (struct sockaddr*)&addr, sizeof(addr));
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket sending data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("sendto() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//回送数据成功
		printf("client socket %d sended data:\n  %s", sfd_cli, sbuf);
    printf("data len = %d,port = %d\n\n",opt_val,srv_port);
	}






  //************************
  //server recv from client
  //************************
  struct msghdr mh;//(IP头 + UDP头 + 数据) recvmsg/sendmsg 缓冲区 描述结构体


  //***接收缓冲区指认IP+UDP 头***
  char cmbuf[io_proco_buf_max];//IP头(MAX=60字节) + [UDP头(max=8字节)
  bzero(&cmbuf, io_proco_buf_max);

  //我想要获取的信息, 的缓冲区
  struct sockaddr_in peeraddr;//对方的IP地址
  bzero(&peeraddr, sizeof(struct sockaddr_in));

  //接收缓冲区指认(IP+UDP 头)
  mh.msg_name = &peeraddr;
  mh.msg_namelen = sizeof(peeraddr);
  mh.msg_control = cmbuf;
  mh.msg_controllen = sizeof(cmbuf);


  //***数据缓冲区指认***
  char rbuf[io_buf_max];//udp 数据接收-缓冲区(最小46 字节, 最大512 单帧)
  bzero(&rbuf, io_buf_max);

  struct iovec iov[1];//接收的 数据缓冲区'描述结构体'
  iov[0].iov_base=rbuf;
  iov[0].iov_len=sizeof(rbuf);
  mh.msg_iov=iov;//指认
  mh.msg_iovlen=1;



  //执行'接收数据'操作
  opt_val = recvmsg(sfd_li, &mh, 0);//同步接收
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket recving data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("recvmsg() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//确认'收到数据'
    //**数据分析开始**
    struct cmsghdr *cmsg;//单节协议提取ip 或者udp 或者tcp 

    // 遍历所有的控制头(the control headers)
    for( cmsg=CMSG_FIRSTHDR(&mh); cmsg; cmsg=CMSG_NXTHDR(&mh,cmsg) ){
      //挑选出IP 头
      if(cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO)
        continue;
      
      struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsg);


      //将地址信息转换后输出
      char dst[128],ipi[128];//用来保存转化后的源IP地址, 目标主机地址
      bzero(&dst, 128);
      bzero(&ipi, 128);
      const char *pstr = NULL;

      pstr = inet_ntop(AF_INET,&(pi->ipi_spec_dst),dst,sizeof(dst));
      if(pstr != NULL){
        printf("server recv from client:\n \
                路由目的地址(destination address of the packet)=%s\n",dst);
        pstr = NULL;
      }

      pstr = inet_ntop(AF_INET,&(pi->ipi_addr),ipi,sizeof(ipi));
      if(pstr != NULL){
        printf("server recv from client:\n \
                头标识目的地址(source address of the packet)=%s\n\n",ipi);
      }

    }//for end


    printf("server socket %d recved data:\n  %s", sfd_li, rbuf);
    printf("data len = %d\n\n",opt_val);
    //**数据分析结束**
	}//if end



  //*********************
  //server send to client
  //*********************
  //组装发送数据
  bzero(&sbuf, io_buf_max);
  snprintf(sbuf, io_buf_max, \
    "hello client, i am the server. %d\n",time(NULL));

  //执行发送操作--服务器回发, 直接利用收到包时候的peeraddr, 端口=peeraddr.sin_port
  //发送'实际字符串长度'
  //opt_val = sendto(sfd_acc, &sbuf, strlen(sbuf), 0,\
                   (struct sockaddr*)&peeraddr, sizeof(peeraddr));
  //发送'整个缓冲区的总长'
  opt_val = sendto(sfd_acc, &sbuf, sizeof(sbuf), 0,\
                   (struct sockaddr*)&peeraddr, sizeof(peeraddr));
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket sending data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("sendto() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//回送数据成功
		printf("server socket %d sended data:\n  %s", sfd_acc, sbuf);
    printf("data len = %d,port = %d\n\n",opt_val,peeraddr.sin_port);
	}




  //中场清空缓冲区
  printf("\n\n");



  //************************
  //client recv from server
  //************************
  //重置缓冲区数据for client 接收
  bzero(&cmbuf, io_proco_buf_max);
  bzero(&rbuf, io_buf_max);
  bzero(&peeraddr, sizeof(struct sockaddr_in));
  bzero(&mh, sizeof(struct msghdr));
  bzero(&iov[0], sizeof(struct iovec));


  //接收缓冲区指认
  mh.msg_name = &peeraddr;
  mh.msg_namelen = sizeof(peeraddr);
  mh.msg_control = cmbuf;
  mh.msg_controllen = sizeof(cmbuf);

  iov[0].iov_base=rbuf;
  iov[0].iov_len=sizeof(rbuf);
  mh.msg_iov=iov;
  mh.msg_iovlen=1;


  //执行接收数据
  opt_val = recvmsg(sfd_cli, &mh, 0);//同步接收
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket recving data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("recvmsg() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//确认'收到数据'
    //**数据分析开始**
    struct cmsghdr *cmsg;//单节协议提取ip 或者udp 或者tcp 

    // 遍历所有的控制头(the control headers)
    for( cmsg=CMSG_FIRSTHDR(&mh); cmsg; cmsg=CMSG_NXTHDR(&mh,cmsg) ){
      //挑选出IP 头
      if(cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO)
        continue;
      
      struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsg);


      //将地址信息转换后输出
      char dst[128],ipi[128];//用来保存转化后的源IP地址, 目标主机地址
      bzero(&dst, 128);
      bzero(&ipi, 128);
      const char *pstr = NULL;

      pstr = inet_ntop(AF_INET,&(pi->ipi_spec_dst),dst,sizeof(dst));
      if(pstr != NULL){
        printf("client recv from server:\n \
                路由目的地址(destination address of the packet)=%s\n",dst);
        pstr = NULL;
      }

      pstr = inet_ntop(AF_INET,&(pi->ipi_addr),ipi,sizeof(ipi));
      if(pstr != NULL){
        printf("client recv from server:\n \
                头标识目的地址(source address of the packet)=%s\n\n",ipi);
      }

    }//for end


    printf("client socket %d recved data:\n  %s", sfd_li, rbuf);
    printf("data len = %d\n\n",opt_val);
    //**数据分析结束**
	}//if end

  return 0;
}





//退出回收资源
void inline free_test_sfd(void){
  shutdown(sfd_li,2);
  close(sfd_li);
  shutdown(sfd_cli,2);
  close(sfd_cli);
  shutdown(sfd_acc,2);
  close(sfd_acc);
}
