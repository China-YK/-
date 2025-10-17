#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <sys/wait.h>
#define m_port 8889
int getanser(std::vector<int>&p1,std::vector<char>&p2,int number_len){
    for (int i = 0;i < p2.size();i++){//先算乘法
        if (p2[i]=='*'){
            p1[i]*=p1[i+1];
            p1.erase(p1.begin()+i+1);
            p2.erase(p2.begin()+i);
            i--;
        }
    }
    int ret=p1[0];
    for (int i = 0;i < p2.size();i++){
        if (p2[i]=='+')ret+=p1[i+1];
        else ret-=p1[i+1];
    }
    return ret;
}
void CreateServer(){
    int sock_server,sock_client;
    struct sockaddr_in server,client;
    sock_server = socket(PF_INET,SOCK_STREAM,0);
    if (sock_server < 0){
        std::cout<<"server_sockte failed\n";
        return ;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(m_port);
    int ret = bind(sock_server,(struct sockaddr*)&server,sizeof(server));
    if (ret == -1){
        std::cout<<"bind failed\n";
        close(sock_server);
        return ;
    }
    ret = listen(sock_server,5);
    if (ret == -1){
        std::cout<<"listen failed\n";
        close(sock_server);
        return ;
    }
    socklen_t client_len=sizeof(client);
    while (1)
    {
        sock_client = accept(sock_server,(struct sockaddr*)&client,&client_len);
        //客户端的 sock_client ≠ 服务器的 accept() 返回的 socket（每个客户端分配一个）。
        //它们是两个不同的文件描述符，分别属于不同进程；
        // 但它们在 TCP 层面上是“一对”通信通道的两端（所以一段write，另一端就可以read到）；
        //服务器的 sock_server 只是“监听门口”的，不负责通信（唯一）。
        if (sock_client == -1){
            std::cout<<"accept failed\n";
            close(sock_server);
        }
        // int number_len;
        // 客户端的是unsigned short，是两个字节，你这里用4个字节接收，不炸了？
        // 客户端发出的字节流： 05 00
        // 服务器内存布局读取： [05][00][??][??]

        char number_len_onebyte;
        unsigned short number_len;
        int received;
        int total;
        read(sock_client,&number_len_onebyte,sizeof(number_len_onebyte));
        number_len=(unsigned short)number_len_onebyte;
        // std::cout<<number_len<<std::endl;
        // std::unique_ptr<int[]> p1 = std::make_unique<int[]>(number_len);//别用智能指针，后面的运算操作不好调用库函数
        // int *p = new int[number_len];
        std::vector<int>p1;
        p1.resize(number_len);

        int i=0;
        // while (i<number_len&&(ret = read(sock_client,&p1[i],sizeof(p1[i])) ) > 0)i++;
        //万一client发送第一个int的时候分了两次发送了，一次发送2个字节，那服务端的读也应该多次读，直到成功读到4个字节
        for (;i<number_len;i++){
            received=0;
            total=sizeof(p1[i]);
            while (received<total){
                ret = read(sock_client,(char*)&p1[i]+received,sizeof(p1[i])-received);
                if (ret < 0){
                    if (errno==EINTR)continue;
                    std::cout<<"server_read_failed"<<std::endl;
                    close(sock_client);
                    return ;
                }else if (ret == 0){
                    std::cout<<"client_closed"<<std::endl;
                    // close(sock_client);
                }
                received+=ret;
            }
        }
        std::vector<char>p2;
        p2.resize(number_len-1);
        i=0;
        while (i<number_len-1&&(ret = read(sock_client,&p2[i],sizeof(p2[i])) ) > 0)i++;
        int count = getanser(p1,p2,number_len);
        ret = write(sock_client,&count,sizeof(count));
        if (ret == -1){
            std::cout<<"server_write_count failed\n";
            close(sock_client);
            return;
        }
    }
}

void CreateClient(){
    int sock_client;
    struct sockaddr_in server;
    sock_client=socket(PF_INET,SOCK_STREAM,0);
    if (sock_client == -1){
        std::cout<<"client_sockte failed\n";
        return ;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(m_port);

    sockaddr_in local{};
    socklen_t len = sizeof(local);
    getsockname(sock_client, (sockaddr*)&local, &len);
    printf("Local IP: %s, port: %d\n",
            inet_ntoa(local.sin_addr), ntohs(local.sin_port));

    int ret = connect(sock_client,(struct sockaddr*)&server,sizeof(server));
    //以前都忽视了这个connect函数，以为只是简单的连接server服务器，其实这里挺复杂的
    //1.会给sock_client(客户端套接字自动分配一个ip和port，前提是你主动给它bind过)
    //在调用connect之前，sock_client只是一个空壳套接字，没有和任何IP/端口绑定
    //当你调用connect时，Linux内核会检查：这个套接字有没有绑定本地地址(IP+Port)
    //如果已经bind(),那就用你自己指定的
    //如果没有bind(),内核自动完成隐式绑定(根据路由表选定的本机IP，再从系统的临时端口范围挑一个49152~65535)

    //2.发起TCP三次握手，尝试连接目标服务器
    //完成隐式绑定后，内核开始真正的连接流程:
    //     SYN=1 Sqe=1000
    //客户端---------------->服务器，服务器收到包后状态LISTEN------>SYN_RCVD(半连接状态)
    //     SYN=1 ACK=1
    //     ack=1001 Seq=3000
    //客户端<----------------服务器,客户端收到包后状态SYN_SENT---->ESTABLISHED
    //    ACK=1 ack=3001 Seq=1001
    //客户端---------------->服务器,服务器收到包后状态SYN_RCBD------>ESTABLISHED

    //查看sock_client绑定的IP和Port
    getsockname(sock_client, (sockaddr*)&local, &len);
    printf("Local IP: %s, port: %d\n",
            inet_ntoa(local.sin_addr), ntohs(local.sin_port));

    if (ret == -1){
        std::cout<<"client_connect failed\n";
        return ;
    }
    unsigned short number_len,temp;
    std::cin>>number_len;
    char number_len_onebyte=(char)number_len;
    // std::cout<<number_len_onebyte<<std::endl;
    int sent=0;
    int total=sizeof(number_len);
    while (sent<total){
        ret = write(sock_client,&number_len_onebyte,sizeof(number_len_onebyte));
        if (ret < 0){
            if (errno==EINTR)continue;
            std::cout<<"client_write_failed"<<std::endl;
            close(sock_client);
            return ;
        }else if (ret == 0){
            std::cout<<"server_no_start"<<std::endl;
            close(sock_client);
            break;
        }
        sent+=ret;
    }
    temp = number_len;
    while (temp--){
        int number;
        sent=0;
        total=sizeof(number);
        std::cin>>number;
        while (sent<total){
            ret = write(sock_client,(char*)&number+sent,sizeof(number));
            if (ret < 0){
                if (errno==EINTR)continue;
                std::cout<<"client_write_failed"<<std::endl;
                close(sock_client);
                return ;
            }else if (ret == 0){
                std::cout<<"server_no_start"<<std::endl;
                close(sock_client);
                break;
            }
            sent+=ret;
        }
    }
    // fputs("Please input %d operators",stdout);
    fprintf(stdout,"Please input %d operators\n",number_len-1);
    //fputs无法像printf那样输出指定内容，但是我们这里可以使用fprintf，将内容输入到stdout里面去
    for (int i=0;i<number_len-1;i++){
        char ch;
        std::cin>>ch;
        write(sock_client,&ch,sizeof(ch));//因为TCP（字节流协议）保证不会出现
        //不会出现“半个字节” 被拆开，因为传输的最小单位是 1 字节；
        //即使底层拆包、延迟发送（在这里凑成一个字节），也不会“只发半个 char”。
    }

    int count;
    ret = read(sock_client,&count,sizeof(count));
    if (ret == -1){
        std::cout<<"client_read_count failed\n";
        close(sock_client);
        return ;
    }
    std::cout<<"anser is :"<<count<<std::endl;
}
void CreateProcess(){
    pid_t pid = fork();
    if (pid==0){//子进程选择用来创建客户端进程
        sleep(1);
        CreateClient();
        std::cout<<"第二个客户端发起连接\n"<<std::endl;
        CreateClient();
    }else if (pid>0){//父进程选择用来创建服务端进程
        CreateServer();
        int status=0;
        wait(&status);
    }else{
        std::cout<<"fork failed!\n";
        return ;
    }
}
int main(){
    CreateProcess();
    return 0;
}