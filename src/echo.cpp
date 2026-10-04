#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using std::cout;
using std::endl;


#define PORT 8080
#define BUFFER_SIZE 1024

int main ()
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt=1;
    socklen_t addrlen=sizeof(address);
    char buffer[BUFFER_SIZE]={0};

    //1.socket 
    if((server_fd=socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        perror("socket failed");
        return 1;
    }

    //2.socket option
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }

    //3.address&port binding
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address))<0)
    {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    //4.connection wait
    if(listen(server_fd, 3)<0)
    {
        perror("listen failed");
        close(server_fd);
        return 1;
    }
    cout<<"Server runnint on port"<<PORT<<"..."<<endl;

    //5.client accept & Echo
    while(true)
    {
        if((client_fd=accept(server_fd, (struct sockaddr*)&address, &addrlen))<0)
        {
            perror("accept failed");
            continue;
        }
        cout<<"Client connected!"<<endl;

        //data receive & transmit
        size_t bytes_read=read(client_fd, buffer, BUFFER_SIZE-1);
        if(bytes_read>0)
        {
            buffer[bytes_read]='\0';
            cout<<"Received:"<<buffer;
            write(client_fd, buffer, bytes_read); //Echo
        }
        close(client_fd);
        cout<<"Client Disconnected."<<endl;
    }
    
    close(server_fd);
    return 0;

}
