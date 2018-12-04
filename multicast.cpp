//编译:
//g++ -ggdb3 -o x ./udp_multicast.cpp 


//理论部分
/*
1.多播的实现与优缺点:
  x.1 多波的实现:
    -1, 多播IP 地址到以太网地址的转换:
        多播的'以太网地址'首字节, 必须是01 !(48 bit 剩下5 字节)
        多播的'以太网地址'高位, 一般为01:00:5e (IANA 多播'以太网地址')

        多播的'以太网地址'低位, 由'多播IP 地址'的低23 bit 直接'复制粘帖'.
        '多播IP 地址'高5 bit 通常被忽略, 直接0 或者随机值.      

    -2, 多播IP 地址:
        1110 [28 bit 多播组ID], 详细划分请看下面<2.广域网的多播>.

    -3. 局域网内多播:
        不需要路由支持, 性能最出色, 局域网内不需要跨网, 就不需要IGMP.

    -4. 多子网多播:(需要路由器支持, 发送IGMP 查询与回复报告)
        多播路由器, 使用IGMP 报文来记录'与该路由器相连'的网络中, 多播组成员的变化情况.
        
        1) 当主机的某个进程, 加入一个多播组时(多播的单位粒度是'主机进程').
           主机就会主动发送一个IGMP 报告, 到路由器'相连的接口'上.

           ps: 关于路由器, 理论上应该有两个接口的! 
               路由器是交换机, 负责'单, 多, 广播'的数据推送,
               同时路由器也是两个网络中的一个主机!!
               软路由有两个网卡, 一个网络一个网卡接口.
               硬核路由虽然只有一个网卡, 但是这个网卡也是虚拟成两个网络中的主机.
               原理都是一样的.

           所以: 主机主动发送的IGMP 报告, 存放在本网络, 路由器与之相连的接口上.

        
        2) 路由器的每个接口, 都有一个多播组表, 这个表记录着该网络的'多播组成员&&组号'.
           一个网络可以有多个多播组, 一个多播组可以有多个成员.
           反正路由器根据'组号'+'组内成员名单', 就可以直接多播了.


        3) 路由器定期发送查询报文给'主机进程', 每个多播组发送一个查询报文.
           发送报文的方式, 同样是多播发送, 只有多播用户会接收查询报文,
           也就是因为这样, 才会让'主机先主动加入多播组', 多播逻辑才能进行.
           
           '多播用户进程'收到IGMP 查询报文必须回复!! 让路由器知道你还在组内.
           偶尔一次半次不回复IGMP 查询报文, 路由器不会认定你已经离开多播组.
           目测路由器并没有'实时'监控多播组资源的能力, 因为本身实时监控的消耗也很大.
           < <否则一次不回复, 路由器认为你已经离开多播组.> 错误理论>


        4) 离开多播组不需要再发送IGMP 报告!! 路由器会对每个多播组发送IGMP 查询报文,
           如果连一个IGMP 报告都收不到, 路由器就会认为改多播组已经不存在, 
           即清空该多播组的表记录.



  x.2 多播的优缺点...!
  多播的优点：
  单播发送 群组 数据, 需要占用信道, 逐个逐个用户单播, 逐份逐份数据发送.
  占用带宽多, 流量大, 效率低, 


  广播发送 群组 数据, 占用信道只需一次, 局域网内所有用户都可以收到, 每份数据也只发一次.
  但由于没有监管, 每个用户都得对'广播数据进行处理', 消耗'无所谓用户'的处理时间.


  多播发送 群组 数据, 继承了广播, 节省带宽, 流量的优点,
  同时由于有监管, '无所谓用户'可以在MAC 地址层, 摆脱不属于自己的多播包, 节省时间.
  ps: 广播MAC 地址为 全FF, 多播则: 首字节为01, 高位一般为01:00:5e, 低位根据组号来定

  而且只要路由器支持, 多播一般可以跨网络, 广播包则一般被路由器屏蔽, 实用性不高.
  多播对'收听用户'的控制更精准, 广播则需要用4 种广播地址来控制划分.



  组播的缺点：
  多播与单播相比没有纠错机制, 当发生错误的时候难以弥补, 但是可以在应用层来实现此种功能.
  (由于多播始终是基于UDP 协议, 面对较为准确的数据传递是, 需要自己实现数据安全传输保证.
   在数据量不大的情况下, 直接用现成的TCP 单播, 似乎是最好的捷径.)

  多播的网络支持存在缺陷, 需要路由器及网络协议栈的支持. 

  多播的应用主要有网上视频、网上会议等. 


实践问题:
    //根据多播地址, 发送多播数据
    //注意: 只要向224.0.0.x 这个多播地址发送数据, 多播组内的主机都能收到.
    //     发送者甚至不需要加入多播组. 奇怪, 多播是怎样实现的??

    //     证明: 多播组的目的, 只是为了接收数据时, 分发出多份数据.
    //           至于数据输入, 可以不是多播组内的用户, 甚至是跨网络用户都行.
    //           但一个多播组, 绑定一个多播IP, 不会存在两个'相同多播IP'的多播组,
    //           路由器中只会认定'一个多播IP'='一个多播组'





2.广域网的多播:
多播的地址是特定的, D类地址用于多播. 
D类IP地址就是多播IP地址, 即224.0.0.0至239.255.255.255之间的IP地址, 
并被划分为局部连接多播地址、预留多播地址和管理权限多播地址3类：

局部多播地址：在224.0.0.0～224.0.0.255之间.
            这是为路由协议和其他用途保留的地址, 路由器并不转发属于此范围的IP包. 

预留多播地址：在224.0.1.0～238.255.255.255之间.
            可用于全球范围(如Internet)或网络协议. 

管理权限多播地址：在239.0.0.0～239.255.255.255之间.
               可供组织内部使用, 类似于私有IP地址,不能用于Internet,可限制多播范围. 

(ps：多播地址很重要, 不是多播域的IP地址不能用来做多播.
     加入多播组时, 如果多播地址不是<多播地址段内的ip>, setsocketopt 会出错.
     是参数错误的类型, 说明内核会检索多播地址的正确性.)





3.多播的编程
  多播的程序设计使用setsockopt()函数和getsockopt()函数来实现, 
  组播的选项是IP层的, 其选项值和含义参见11.5所示. 


xx:多播相关的选项
getsockopt()/setsockopt()的选项       含义
IP_MULTICAST_TTL                     设置多播组数据的TTL值
IP_ADD_MEMBERSHIP                    在指定接口上加入组播组
IP_DROP_MEMBERSHIP                   退出组播组
IP_MULTICAST_IF                      获取默认接口或设置接口
IP_MULTICAST_LOOP                    禁止组播数据回送


x1．选项IP_MULTICAST_TTL
  允许设置超时TTL, 范围为0～255之间的任何值, 例如：
  unsigned char ttl=64;//设置64 跳
  setsockopt(sfd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl));


x2．选项IP_MULTICAST_IF
  用于设置组播的默认默认网络接口, 会从给定的网络接口发送, 另一个网络接口会忽略此数据. 
  例如：
  struct in_addr addr;
  setsockopt(sfd,IPPROTO_IP,IP_MULTICAST_IF,&addr,sizeof(addr));

  参数 addr 是你指定的多播输出接口的IP地址, 使用 INADDR_ANY 地址回送到默认接口.


x3．选项IP_ADD_MEMBERSHIP
  用于加入一个组播组. 例如:
  struct ip_mreq mreq;
  setsockopt(s,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq));

  每次只能加入一个网络接口的IP地址到多播组, 
  但并不是一个多播组仅允许一个主机IP地址加入, 
  可以多次调用IP_ADD_MEMBERSHIP选项, 来实现<多个IP地址>加入同一个广播组, 
  或者<同一个IP地址>加入多个广播组.

  当imr_ interface为INADDR_ANY时, 选择的是默认组播接口。

  struct ip_mreq{
    struct in_addr imn_multiaddr; //多播组IP地址(必须是224.0.0.0 网域)
    struct in_addr imr_interface; //指定的IO 发送/接收的本机网卡地址
                                    (INADDR_ANY 为默认)
  }
***
//多播io 中, 发送和接收差异很大, 要分清楚!!
//注意: 只要向224.0.0.x 这个多播地址发送数据, 多播组内的主机都能收到.
//     发送者甚至不需要加入多播组. 奇怪, 多播是怎样实现的??
//     相当于广播, 但是只是对某个IP 进行广播.
//     所以其实发送数据还是: 只发了一个.
***


x4．选项IP_DROP_MEMBERSHIP
  用于从一个广播组中退出. 例如：
  struct ip_mreq mreq;
  setsockopt(sfd,IPPROTP_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(sreq));


x5.默认情况下, 当本机发送<组播数据>到某个网络接口时, 
   在IP层, 数据会回送到本地的回环接口.
   选项IP_MULTICAST_LOOP用于控制数据是否回送到本地的回环接口.
   参数 xloop 设置为0禁止回送, 设置为1允许回送.
   int xloop = 0;
   setsockopt(sfd,IPPROTO_IP,IP_MULTICAST_IF,&xloop,sizeof(xloop));
*/


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

    //     证明: 多播组的目的, 只是为了接收数据时, 分发出多份数据.
    //           至于数据输入, 可以不是多播组内的用户, 甚至是跨网络用户都行.
    //           但一个多播组, 绑定一个多播IP, 不会存在两个'相同多播IP'的多播组,
    //           路由器中只会认定'一个多播IP'='一个多播组'
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

