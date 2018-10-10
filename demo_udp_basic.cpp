//编译
//g++ -ggdb3 -o x ./demo_udp_basic.cpp

//根据发上来的数据包, 获取ip 地址后, 回发向来源的对段ip + 端口回发数据...
//当然, 不占用原bind 监听的socket,,,
//创建一个新的socket 做回发, 
//接收数据的时候最好能够做到截断！！ 这样每次只接收一个512 包, 多都不要!!

//广播, 多播, 都需要收到包后原路返回...
//原路返回是udp 的第一个门槛...


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
#define srv_ip "127.0.0.1"
#define srv_port 6666

#define io_buf_max 2048 // 缓冲区max

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

  //创建udp 报式 socket for client
  sfd_acc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sfd_acc == -1){
		printf("socket() fail, errno = %d\n", errno);
    close(sfd_li);
    close(sfd_cli);
		return -1;
	}


  //设置地址重用 -- 不设置重用, 可能导致二次调用程序的时候, bind() 失败
  //因为系统资源释放没有那么快??
  int opt_val = true;
  opt_val = setsockopt(sfd_li, SOL_SOCKET, SO_REUSEADDR, \
							&opt_val, sizeof(int));
	if(opt_val == -1){
		printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}

  opt_val = true;
  opt_val = setsockopt(sfd_cli, SOL_SOCKET, SO_REUSEADDR, \
							&opt_val, sizeof(int));
	if(opt_val == -1){
		printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}

  opt_val = true;
  opt_val = setsockopt(sfd_acc, SOL_SOCKET, SO_REUSEADDR, \
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

  //client send to server
  //设置发送接受地址
  struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = inet_addr(srv_ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(srv_port);

  //组装发送数据
  char sbuf[io_buf_max];//发送缓冲区
  bzero(&sbuf, io_buf_max);
  snprintf(sbuf, io_buf_max, \
    "hello server, i am the client. %d\n",time(NULL));

  //执行发送操作
  int opt_val = sendto(sfd_cli, &sbuf, strlen(sbuf), 0,\
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
		printf("client socket %d sended data:\n%s", sfd_cli, sbuf);
    printf("sended data len = %d,port = %d\n\n",opt_val,srv_port);
	}


  //************************
  //server recv from client
  char cmbuf[128];//控制数据, 是脏数据?? 没用的???
  bzero(&cmbuf, 128);

  char rbuf[io_buf_max];//数据接收缓冲区
  bzero(&rbuf, io_buf_max);

  struct sockaddr_in peeraddr;// 对方的IP地址
  bzero(&peeraddr, sizeof(struct sockaddr_in));

  struct msghdr mh;
  mh.msg_name = &peeraddr;
  mh.msg_namelen = sizeof(peeraddr);
  mh.msg_control = cmbuf;
  mh.msg_controllen = sizeof(cmbuf);

  struct iovec iov[1];//数据接收缓冲区
  iov[0].iov_base=rbuf;
  iov[0].iov_len=sizeof(rbuf);
  mh.msg_iov=iov;
  mh.msg_iovlen=1;

  //接收数据
  opt_val = recvmsg(sfd_li, &mh, 0);//同步接收
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket recving data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("recvmsg() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//收到数据
    struct cmsghdr *cmsg ;
    for( // 遍历所有的控制头(the control headers)
      cmsg = CMSG_FIRSTHDR(&mh);
      cmsg != NULL;
      cmsg = CMSG_NXTHDR(&mh, cmsg)){
      // 忽略我们不需要的控制头(the control headers)
      if(cmsg->cmsg_level != IPPROTO_IP ||
         cmsg->cmsg_type != IP_PKTINFO){
        continue;
      }
    struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsg);

    //将地址信息转换后输出
    // 在这里, peeraddr是本机的地址(the source sockaddr)
    // pi->ipi_spec_dst 是UDP包中路由目的地址(the destination in_addr)
    // pi->ipi_addr 是UDP包中的头标识目的地址(the receiving interface in_addr)

    char dst[128],ipi[128];//用来保存转化后的源IP地址, 目标主机地址
    inet_ntop(AF_INET,&(pi->ipi_spec_dst),dst,sizeof(dst));
    if((inet_ntop(AF_INET,&(pi->ipi_spec_dst),dst,sizeof(dst))) != NULL){
      printf("路由目的IP地址IPdst=%s\n",dst);
    }
    
    if((inet_ntop(AF_INET,&(pi->ipi_addr),ipi,sizeof(ipi))) != NULL){
      printf("头标识目的地址ipi_addr=%s\n",ipi);
    }
    
    }//for end
    printf("server socket %d recved data:\n%s", sfd_li, rbuf);
    printf("recved data len = %d\n\n",opt_val);
	}
  //************************


  //中场清空缓冲区
  printf("\n");


  //server send to client
  //组装发送数据
  bzero(&sbuf, io_buf_max);
  snprintf(sbuf, io_buf_max, \
    "hello client, i am the server. %d\n",time(NULL));

  //执行发送操作--服务器回发, 直接利用收到包时候的peeraddr, 端口=peeraddr.sin_port
  opt_val = sendto(sfd_acc, &sbuf, strlen(sbuf), 0,\
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
		printf("server socket %d sended data:\n%s", sfd_acc, sbuf);
    printf("sended data len = %d,port = %d\n\n",opt_val,peeraddr.sin_port);
	}


  
  //************************
  //client recv from server
  bzero(&cmbuf, 128);
  bzero(&rbuf, io_buf_max);
  bzero(&peeraddr, sizeof(struct sockaddr_in));
  bzero(&mh, sizeof(struct msghdr));
  bzero(&iov[0], sizeof(struct iovec));

  mh.msg_name = &peeraddr;
  mh.msg_namelen = sizeof(peeraddr);
  mh.msg_control = cmbuf;
  mh.msg_controllen = sizeof(cmbuf);

  iov[0].iov_base=rbuf;
  iov[0].iov_len=sizeof(rbuf);
  mh.msg_iov=iov;
  mh.msg_iovlen=1;

  //接收数据
  opt_val = recvmsg(sfd_cli, &mh, 0);//同步接收
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket recving data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("recvmsg() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//收到数据
    struct cmsghdr *cmsg ;
    for( // 遍历所有的控制头(the control headers)
      cmsg = CMSG_FIRSTHDR(&mh);
      cmsg != NULL;
      cmsg = CMSG_NXTHDR(&mh, cmsg)){
      // 忽略我们不需要的控制头(the control headers)
      if(cmsg->cmsg_level != IPPROTO_IP ||
         cmsg->cmsg_type != IP_PKTINFO){
        continue;
      }
    struct in_pktinfo *pi = (struct in_pktinfo *)CMSG_DATA(cmsg);

    //将地址信息转换后输出
    // 在这里, peeraddr是本机的地址(the source sockaddr)
    // pi->ipi_spec_dst 是UDP包中路由目的地址(the destination in_addr)
    // pi->ipi_addr 是UDP包中的头标识目的地址(the receiving interface in_addr)

    char dst[128],ipi[128];//用来保存转化后的源IP地址, 目标主机地址
    inet_ntop(AF_INET,&(pi->ipi_spec_dst),dst,sizeof(dst));
    if((inet_ntop(AF_INET,&(pi->ipi_spec_dst),dst,sizeof(dst))) != NULL){
      printf("路由目的IP地址IPdst=%s\n",dst);
    }
    
    if((inet_ntop(AF_INET,&(pi->ipi_addr),ipi,sizeof(ipi))) != NULL){
      printf("头标识目的地址ipi_addr=%s\n",ipi);
    }
    
    }//for end
    printf("client socket %d recved data:\n%s", sfd_cli, rbuf);
    printf("recved data len = %d\n\n",opt_val);
	}
  //************************
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
