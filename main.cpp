#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <sys/wait.h>
#define m_port 8888
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
    int ret = connect(sock_client,(struct sockaddr*)&server,sizeof(server));
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