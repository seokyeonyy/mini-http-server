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
        ste::ifstream file(file_path, std::ios::binary);
        if (file)
        {
            
        }
    }
}
