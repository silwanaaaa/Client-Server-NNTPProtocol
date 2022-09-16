#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <fstream>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <algorithm>
#include <dirent.h>
#include <istream>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstring>
#include <iterator>
#include <regex>
#include <string>

#define MAXDATASIZE 100000 // max number of bytes we can get at once

using namespace std;

string deleteWhitespaces(string &path)
{
    path.erase(remove_if(path.begin(), path.end(), ::isspace), path.end());
    return path;
}

string getInfoFromFile(string filename, int count)
{
    ifstream configFile(filename);
    if (!configFile.is_open())
    {
        cout << "Config file not open";
    }
    string line;
    string toReturn;
    int linecount = 0;

    while (getline(configFile, line))
    {
        if (linecount == count)
        {
            if (count == 1)
            {
                line.erase(0, 10);
                toReturn = line;
            }
            else if (count == 2)
            {
                toReturn = line;
                toReturn.erase(0, 12);
            }
        }

        linecount++;
    }
    // cout << toReturn << endl;
    return toReturn;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char const *argv[])
{
    int sockfd, numbytes;
    char buffer[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 2)
    {
        printf("Argument Error");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int n = 0;

    string configFile = argv[1];
    string ip = getInfoFromFile(configFile, 1);
    string portNum = getInfoFromFile(configFile, 2);

    deleteWhitespaces(ip);
    deleteWhitespaces(portNum);

    if ((rv = getaddrinfo(ip.c_str(), portNum.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "[C] getaddrinfo client: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("[S] Connection Failed\n");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "[C] Connection Failed\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("[C] Client connected to Server\n");

    freeaddrinfo(servinfo); // all done with this structure
    // read(sockfd, buffer, sizeof(buffer));
    
    
    //read(sockfd, buffer, sizeof(buffer));
    while (true)
    {
        bzero(buffer, sizeof(buffer));
        printf("Enter the command, 'QUIT' to quit : ");

        string keyboardInput;
        getline(cin, keyboardInput);

        int sent = send(sockfd, keyboardInput.c_str(), strlen(keyboardInput.c_str()), 0);
        // cout << buffer;
        if (sent == -1)
        {
            cout << "sending failed";
        }

        bzero(buffer, sizeof(buffer));

        read(sockfd, buffer, sizeof(buffer));

        printf("%s\n", buffer);

        if (strncmp("205 closing connection - goodbye!", buffer, 33) == 0)
        {
            close(sockfd);
            break;
        }
        //bzero(buffer, sizeof(buffer));
    }

    close(sockfd);

    return 0;
}