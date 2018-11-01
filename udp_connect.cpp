//编译:
//g++ -ggdb3 -o x ./udp_connect.cpp

//测试udp 使用connect 之后, 就可以直接使用write/read 通用IO 函数了.
//而且udp 使用connect 之后, 就不能再用sendto/recvfrom 了.

//udp bind() 之后, 仍能使用已经绑定的socket, 对自己进行自收自发.
//但如果你创建一个新的udp socket 来通信, 
//你就必须用sendmsg/recvmsg 来获取对方的port和ip, 否则你没办法给对方回发数据.


#include <sys/types.h>
#include <sys/socket.h>	// for socket, bind

#include <strings.h> // for bzero
#include <string.h>	// for memset
#include <unistd.h> // for read/write

//注意: arpa/inet.h 不能与 linux/in.h 共存, 不然会有重复定义
#include <arpa/inet.h> // for htonl,htons
//#include <linux/in.h>	// for struct sockaddr_in

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>


#define buf_len 1024 //io 缓冲区长度
#define srv_port 8888	//服务器端口
#define srv_ip "127.0.0.1"

//udp connect + read/write IO 测试
bool udp_connect_test(int s, struct sockaddr *to){
  //连接客户端
	int n=connect(s, to, sizeof(*to));//(注意: sizeof() 指针的用法有点特别, 
                                    //       可以帮你恶补c 语言的语法)
  if(n == -1){
    printf("connect fail, errno = %d\n",errno);
    return false;
  }
  
  //接收数据
  char recv_buf[buf_len]; //接收缓冲区
  bzero(&recv_buf, buf_len);
  n = read(s, recv_buf, buf_len);//接收数据
  if(n == -1){
    printf("read fail, errno = %d\n",errno);
    return false;
  }
  else
    printf("socket %d read data:\n len=%d, data=%s\n\n"\
      ,s,n,recv_buf);

  //回发数据
  char send_buf[] = "hello client";
	n = write(s, send_buf, strlen(send_buf));//发送数据
  if(n == -1){
    printf("write fail, errno = %d\n",errno);
    return false;
  }
  else
    printf("socket %d sended data okay,sended size=%d\n\n",s,n);	

	return true;
}
