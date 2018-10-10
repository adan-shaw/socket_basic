//编译
//g++ -ggdb3 -o x ./getnameinfo.cpp

#include <netinet/in.h> // for IPPROTO_TCP
#include <arpa/inet.h> // for inet_addr
//#include <netinet/tcp.h>
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

#include <netdb.h> // for getnameinfo

//宏定义
#define srv_ip "127.0.0.1"
#define srv_port 6666
#define BACKLOG 64 //监听队列最大长度 64
#define io_buf_max 2048 // 缓冲区max

//全局变量
int ppid = 0;//父进程pid
int sfd_li = 0;
int sfd_cli = 0;

//函数前置声明
//客户端执行链接操作 -- fork() 子进程
int cli_conn(void);

//自收自发测试
int io2himself(int sfd_acc);

//退出回收资源
void inline free_test_sfd(void);

//测试主函数
int main(void){
  //创建tcp 流式 socket
  sfd_li = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sfd_li == -1){
		printf("socket() fail, errno = %d\n", errno);
		return -1;
	}

  //创建tcp 流式 socket for client
  sfd_cli = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sfd_cli == -1){
		printf("socket() fail, errno = %d\n", errno);
    close(sfd_li);
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
  opt_val = setsockopt(sfd_li, SOL_SOCKET, SO_REUSEADDR, \
							&opt_val, sizeof(int));
	if(opt_val == -1){
		printf("set_sockopt_reuseaddr() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}
  

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


  //执行listen
  opt_val = listen(sfd_li, BACKLOG);
  if(opt_val == -1){
    printf("socket %d listen fail, errno: %d\n", sfd_li, errno);
    free_test_sfd();
    return -1;
  }





  //fork() 拆分<客户端/服务端>
  ppid = getpid();
  printf("father pid: %d\n", ppid);

  int pid_son = fork();
  if(pid_son == -1){
		printf("fork() fail, errno = %d\n", errno);
		free_test_sfd();
		return -1;
	}
  else{//fork() 成功
	  if(pid_son == 0){
    	//<< 当前新建的子进程 >>
		  pid_son = getpid();//子进程保存自己的pid
      printf("son pid: %d\n", pid_son);

      if(cli_conn() == -1){
        printf("son doing mission fail !!\n");
        exit(-1);
      }
      else{
        printf("son doing mission well.\n");
        exit(0);
      }
      //son pthread has exit !!, no more ~~
    }
    else{
    	//<< 父进程需要做的事 >>
      sleep(1);//防止父进程跑得比子进程快

      //等待子进程执行connect() -- 阻塞等待
      struct sockaddr addr2;
	    socklen_t addr_len = sizeof(struct sockaddr);
	    bzero(&addr2, sizeof(struct sockaddr));
      int sfd_acc = accept(sfd_li, &addr2, &addr_len);
      if(sfd_acc == -1){
	      printf("accept() fail, errno: %d\n", errno);
        free_test_sfd();
	      return -1;
      }
		  else{
        //子进程正确链接到父进程
        printf("father pthread accept client by socket %d\n",sfd_acc);
        printf("waiting for son %d\n", pid_son);
        //询问子进程结束状态, 并清空僵尸进程
        int son_ret = -1;
        int tmp = waitpid(pid_son, &son_ret, WNOHANG);//死等子进程退出
        if(tmp == -1){
          printf("waitpid() fail, errno = %d\n", errno);
          free_test_sfd();
	        return -1;
        }
        if(tmp == pid_son){//子进程已经结束
          printf("son pid = %d form waitpid(), exit status = %d\n\n",\
            pid_son, son_ret);
          if(son_ret != 0){
            //子进程不正常结束, 不用继续往下走了
            free_test_sfd();
	          return -1;
          }
          else{
            //子进程正常结束, socket 链接ok
            //执行自收自发操作
            if(io2himself(sfd_acc) == 0)
              printf("io2himself() ok!!\n");
            else
              printf("io2himself() fail!!\n");
            //父进程任务结束
          }
        }
        if(tmp == 0)//子进程还没结束, 是一个错误
          printf("son pthread has not end, it's a fault\n");
        
      }//accept if end


      //***********************
      //加插任务: 解析对方的ip and port
      //根据自身的sfd, 反向解析出struct sockaddr* [服务器只知道sfd_acc]
      //(如果struct sockaddr* 是局部变量, 可能会丢失, 所以自身解析也有用)
      struct sockaddr addr1;
      bzero(&addr1,sizeof(struct sockaddr));
      socklen_t len1 = sizeof(struct sockaddr);//uint32
      int tmp = getsockname(sfd_acc,&addr1,&len1);
      if(tmp == -1)
        printf("getsockname() fail, errno = %d\n",errno);
      else{
        char host_buf[64];
        char serv_buf[64];
        bzero(&host_buf,sizeof(host_buf));
        bzero(&serv_buf,sizeof(serv_buf));
        tmp = getnameinfo(&addr1, len1, host_buf, sizeof(host_buf),\
                          serv_buf, sizeof(host_buf), NI_NAMEREQD);
        if(tmp != 0)
          printf("getnameinfo() fail!!errno = %d\n", errno);
        else
          printf("1.host_buf = %s, serv_buf = %s\n",host_buf,serv_buf);
      }
      
      //根据对方的sfd, 反向解析出struct sockaddr* [服务器只知道sfd_acc]
      bzero(&addr1,sizeof(struct sockaddr));
      len1 = sizeof(struct sockaddr);
      tmp = getpeername(sfd_acc,&addr1,&len1);
      if(tmp == -1)
        printf("getpeername() fail, errno = %d\n",errno);
      else{
        char host_buf[64];
        char serv_buf[64];
        bzero(&host_buf,sizeof(host_buf));
        bzero(&serv_buf,sizeof(serv_buf));
        tmp = getnameinfo(&addr1, len1, host_buf, sizeof(host_buf),\
                          serv_buf, sizeof(host_buf), NI_NAMEREQD);
        if(tmp != 0)
          printf("getnameinfo() fail!!errno = %d\n", errno);
        else
          printf("2.host_buf = %s, serv_buf = %s\n",host_buf,serv_buf);
      }

      //***********************


      //父进程结束, 回收资源
      shutdown(sfd_acc,2);
      close(sfd_acc);

      free_test_sfd();
      printf("father pthread quit\n");
    }
  }

  return 0;
}


//创建客户端 && 执行链接操作 -- fork() 子进程
int cli_conn(void){
  //执行connect() -- sockaddr_in 可以直接强转为struct sockaddr
  //等待父进程accept(), 阻塞等待
	struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = inet_addr(srv_ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(srv_port);

  int opt_val=connect(sfd_cli,(struct sockaddr*)&addr,\
                sizeof(struct sockaddr));
  if(opt_val== -1){
		printf("connect() fail, errno = %d\n", errno);
		close(sfd_cli);
		return -1;
  }

  return 0;
}


//自收自发测试
int io2himself(int sfd_acc){
  char rbuf[io_buf_max];//接收缓冲区
  char sbuf[io_buf_max];//发送缓冲区
  bzero(&rbuf, io_buf_max);
  bzero(&sbuf, io_buf_max);

  //client send to server
  //组装发送数据
  snprintf(sbuf, io_buf_max, \
    "hello server, i am the client. %d\n",time(NULL));
  //执行发送操作
  int opt_val = send(sfd_cli, &sbuf, strlen(sbuf), 0);
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket sending data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("send() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//回送数据成功
		printf("client socket %d sended data:\n%s", sfd_cli, sbuf);
    printf("sended data len = %d\n\n",opt_val);
	}


  //server recv from client
  //接收数据
  opt_val = recv(sfd_acc, &rbuf, io_buf_max, 0);
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket recving data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("recv() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//收到数据
		printf("server socket %d recved data:\n%s", sfd_acc, rbuf);
    printf("recved data len = %d\n\n",opt_val);
	}


  //中场清空缓冲区
  printf("\n");
  bzero(&rbuf, io_buf_max);
  bzero(&sbuf, io_buf_max);


  //server send to client
  //组装发送数据
  snprintf(sbuf, io_buf_max, \
    "hello client, i am the server. %d\n",time(NULL));
  //执行发送操作
  opt_val = send(sfd_acc, &sbuf, strlen(sbuf), 0);
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket sending data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("send() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//回送数据成功
		printf("server socket %d sended data:\n%s", sfd_acc, sbuf);
    printf("sended data len = %d\n\n",opt_val);
	}


  //client recv from server
  //接收数据
  opt_val = recv(sfd_cli, &rbuf, io_buf_max, 0);
	if(opt_val == 0){//对端已经关闭
		printf("each other terminal has close when socket recving data\n");
		return -1;
	}
	if(opt_val == -1){//recv 错误
		printf("recv() fail, errno = %d\n", errno);
		return -1;
	}
	if(opt_val > 0){//收到数据
		printf("client socket %d recved data:\n%s", sfd_cli, rbuf);
    printf("recved data len = %d\n\n",opt_val);
	}

  return 0;
}


//退出回收资源
void inline free_test_sfd(void){
  shutdown(sfd_li,2);
  close(sfd_li);
  shutdown(sfd_cli,2);
  close(sfd_cli);
}



