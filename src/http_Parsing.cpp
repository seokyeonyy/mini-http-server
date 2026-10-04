#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using std::cout;
using std::endl;
using std::string;


#define PORT 8080
#define BUFFER_SIZE 2048

//1.HTTP Struct
struct HttpRequest
{
    string method;
    string path;
    string version;
};

//2.HTTP Request Parshing Function
HttpRequest parse_request(const string& request_str)
{
    HttpRequest req;
    std::istringstream stream(request_str);

    //Request Line parshing :"GET /index.html HTTP/1.1"
    stream>>req.method>>req.path>>req.version;

    //root path => default index.html
    if(req.path=="/")
    {
        req.path="/index.html";
    }
    return req;


}

//3.file exist check & HTTP Response Header/body F
string handle_request(const HttpRequest& req)
{
    string file_path="./www"+req.path;

    struct stat file_stat;
    //[200 OK]+normal
    if(stat(file_path.c_str(), &file_stat)==0 && S_ISREG(file_stat.st_mode))
    {
        std::ifstream file(file_path, std::ios::binary);
        if (file)
        {
            std::ostringstream ss;
            ss << file.rdbuf();
            string body=ss.str();

            string response="HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/html; charset=UTF-8\r\n";
            response += "Content-Length:" + std::to_string(body.size())+"\r\n";
            response += "Connection: close\r\n\r\n";
            response += body;

            return response;

        }
    }

    //[404 Not Found] 
    string body_404 = "<html><body><h1>404 Not Found</h1></body></html>";
    string response_404 = "HTTP/1.1 404 Not Found\r\n";
    response_404 += "Content-Type: text/html; charset=UTF-8\r\n";
    response_404 += "Content-Length: " + std::to_string(body_404.size()) + "\r\n";
    response_404 += "Connection: close\r\n\r\n";
    response_404 += body_404;

    return response_404;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt=1;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE]={0};

    //1.Socket
    if ((server_fd=socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        perror("socket failed");
        return 1;
    }
    //2.Socket Option
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

    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    //4.connection wait 
    if(listen(server_fd, 5) <0)
    {
        perror("listen failed");
        close(server_fd);
        return 1;
    }
    cout<<"Server running on port"<< PORT<<"..."<<endl;
    
    //5.client accept&handle request
    while (true)
    {
        if((client_fd=accept(server_fd, (struct sockaddr*)&address, &addrlen))<0)
        {
            perror("accept failed");
            continue;
        }
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_read=read(client_fd, buffer, BUFFER_SIZE-1);
        if(bytes_read>0)
        {
            buffer[bytes_read]='\0';

            //parsing&response
            HttpRequest req=parse_request(buffer);
            cout<<"Request:"<<req.method<<" "<<req.path<<endl;

            //make response -> client
            string response=handle_request(req);
            write(client_fd, response.c_str(), response.length());
        }
        close(client_fd);
    }
    close(server_fd);
    return 0;
}
