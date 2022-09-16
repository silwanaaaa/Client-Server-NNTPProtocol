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
#include <vector>
#include <sstream>
#include <ctime>
#include <cstring>
#include <iterator>
#include <regex>
#include <string>

#define BACKLOG 10 // how many pending connections queue will hold

using namespace std;

string message;
vector<int> GroupVector;
int NumOfFilesInDirectory = -1;
string globalGroup = "";
bool groupFail = true;
int FileNumber = 1;

bool isXEmpty(const string &x)
{
    return x.size() == 0;
}

string deleteWhitespaces(string &path)
{
    path.erase(remove_if(path.begin(), path.end(), ::isspace), path.end());
    return path;
}

string openFileForMsgID(string &path)
{
    deleteWhitespaces(path);
    ifstream file;
    string line, msgID;
    file.open(path);
    if (file.is_open())
    {
        while (getline(file, line))
        {
            if (line.find("Message-ID: ") != string::npos)
            {
                line.erase(0, 12);
                msgID = line;
            }
        }
        file.close();
    }
    return msgID;
}

string help()
{
    message = "[S] 100 Help text follows:\n";
    message += "[S] CAPABILITIES - List of supported commands.\n\tPossible Responses:\n\t\t101 Capability list\n";
    message += "[S] GROUP- Summary about the selected newsgroup.\n\tPossible Responses:\n\t\t211 Group successfully selected\n\t\t411 No such newsgroup\n";
    message += "[S] LISTGROUP - Returns the GROUP command as well as a list of article numbers in the newsgroup. If no group is specified, the currently selected newsgroup is used.\n\tPossible Responses:\n\t\t211 Article numbers follow\n\t\t411 No such newsgroup\n\t\t412 No newsgroup selected\n";
    message += "[S] ARTICLE - Returns the entire article.\n";
    message += "\tPossible Responses:\n\t\t230, 220 Article follows\n\t\t430 No article with that message ID or field\n\t\t412 No newsgroup selected\n\t\t423 No article with that number\n\t\t420 Current article number is invalid\n";
    message += "[S] NEXT - Returns the next article in the currently selected newsgroup.\n";
    message += "\tPossible Responses:\n\t\t223 Article found\n\t\t412 No newsgroup selected\n\t\t420 Current article number is invalid\n\t\t421 No next article in this group\n";
    message += "[S] HDR - Returns the specific header of an article.\n";
    message += "\tPossible Responses:\n\t\t225 Headers follow\n\t\t430 No article with that message ID\n";
    message += "[S] LIST HEADERS - Returns a list of fields, either in a specified article or in the whole db.\n";
    message += "\tPossible Responses:\n\t\t215 Field list follows\n";
    message += "[S] LIST ACTIVE - Returns a list of valid newsgroups and associated information.\n";
    message += "\tPossible Responses:\n\t\t215 List of newsgroups follows\n";
    message += "[S] LIST NEWSGROUPS - Returns the name of each newsgroup on the server along with a description.\n";
    message += "\tPossible Responses:\n\t\t215 information follows\n";
    message += "[S] DATE- Server data and time.\n\tPossible Responses:\n\t\t111 Server data and time\n";
    message += "[S] QUIT- Exits client\n\tPossible Responses:\n\t\t 205 closing connection\n";
    return message;
}

string quit()
{
    message = "205 closing connection - goodbye!";
    return message;
}

string capabilities()
{
    message = "[S] 101 Capability list: \n";
    message += "[S] VERSION 2\n";
    message += "[S] READER\n";
    message += "[S] HDR\n";
    message += "[S] LIST ACTIVE NEWSGROUPS HEADERS\n";
    message += "[S] .\n";
    return message;
}

string invalidCommand()
{
    message = "[S] 500 Invalid command\n";
    return message;
}

string group(string filename)
{
    // reset num files and the global vector each time or it'll keep getting added to
    GroupVector.clear();
    NumOfFilesInDirectory = -1; //-1 for .info file

    DIR *dr;
    struct dirent *en;

    string path = "db/" + filename;
    deleteWhitespaces(path);

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0)) // accounts for the . and .. for previous and next folder
            {
                string current = en->d_name;
                deleteWhitespaces(current);

                string file = current.substr(0, current.find_last_of("."));
                GroupVector.push_back(atoi(file.c_str())); // convert the string to char to int for sorting
                NumOfFilesInDirectory++;
            }
        }

        sort(GroupVector.begin(), GroupVector.end());

        message = "[S] 211 " + to_string(NumOfFilesInDirectory) + " " + to_string(GroupVector[1]) + " " + to_string(GroupVector[NumOfFilesInDirectory]) + " " + filename + "\n";
        closedir(dr);

        // set globals incase of failure
        globalGroup = filename;
        groupFail = false;
    }
    else
    {
        groupFail = true;
        message = "[S] 411 No such newsgroup\n";
        // cout << "group failed";
    }

    return message;
}

string listgroupWithRange(string fullInput, vector<string> ListgroupParts)
{

    bool validRange;
    // run group and make sure the newsgroup exists
    if (!groupFail)
    {
        // just one number, one number with hyphen, or range(with hyphen)
        if (fullInput.find("-") != string::npos) // if there is a hyphen, check to see if there is a range or just one number(return all)
        {
            string range = ListgroupParts[1];
            deleteWhitespaces(range);

            if (range.back() == '-') // max, print out everything after the number
            {
                message = group(ListgroupParts[0]);
                message += "[S] list follows\n";

                // check if ListGroupParts[1] = GroupVector[x] and once it is, print out the rest
                // check if the value exists in groupvector
                int i = 0;

                while (stoi(range) != GroupVector[i])
                {
                    if (i <= NumOfFilesInDirectory)
                    {
                        i++;
                        validRange = true;
                    }
                    else
                    {
                        validRange = false;
                        break;
                    }
                }

                // cout << i;
                if (validRange)
                {
                    for (i; i < GroupVector.size(); i++)
                    {
                        if (GroupVector[i] != 0) // 0 takes into account the .info file
                        {
                            message += "[S] " + to_string(GroupVector[i]) + " \n";
                        }
                    }
                }
            }
            else // has full range
            {
                message = group(ListgroupParts[0]);
                message += "[S] list follows\n";
                char delimiter = '-';
                istringstream stream(range);
                vector<string> ListgroupRange;
                string part;
                while (getline(stream, part, delimiter))
                {
                    ListgroupRange.push_back(part);
                }

                int low = stoi(ListgroupRange[0]);

                int lowVal = 0;

                while (low != GroupVector[lowVal])
                {
                    if (lowVal <= NumOfFilesInDirectory)
                    {
                        lowVal++;
                        validRange = true;
                    }
                    else
                    {
                        validRange = false;
                        break;
                    }
                }

                if (validRange)
                {
                    int high = stoi(ListgroupRange[1]);
                    int highVal = 0;

                    while (high != GroupVector[highVal])
                    {
                        if (highVal <= NumOfFilesInDirectory)
                        {
                            highVal++;
                            validRange = true;
                        }
                        else
                        {
                            validRange = false;
                            break;
                        }
                    }

                    for (lowVal; lowVal <= highVal; lowVal++) // makes sure that the high value is greater than low
                    {
                        if (GroupVector[lowVal] != 0)
                        {
                            message += "[S] " + to_string(GroupVector[lowVal]) + " \n";
                        }
                    }
                }
            }
        }
        else // just one number, no hyphen
        {
            message += "[S] list follows\n";
        }
        message += "[S] .\n";
    }
    return message;
}

string listgroup(string filename)
{
    char delimiter = ' ';
    istringstream stream(filename);
    vector<string> ListgroupParts;
    string part;
    while (getline(stream, part, delimiter))
    {
        ListgroupParts.push_back(part);
    }
    // cout << ListgroupParts.size();

    if (ListgroupParts.size() <= 1) // empty (so  using the global group) or has a filename
    {
        message = group(filename);
        // cout << message;
        // cout << "x";
        if (!groupFail)
        {
            // cout << "y";
            message += "[S] list follows\n";
            for (int i = 0; i < GroupVector.size(); i++)
            {
                if (GroupVector[i] != 0) // 0 takes into account the .info file
                {
                    message += "[S] " + to_string(GroupVector[i]) + " \n";
                }
            }
            message += "[S] .\n";
        }
    }
    else
    {
        listgroupWithRange(filename, ListgroupParts);
    }
    return message;
}

string next(string globalGroupName)
{
    string folderPath = "db/" + globalGroupName;
    int filename = GroupVector[FileNumber];
    string path = folderPath + "/" + to_string(filename) + ".txt";
    // deleteWhitespaces(path);
    // cout << path;

    string msgID = openFileForMsgID(path);
    FileNumber++;
    message += "[S] 223 " + to_string(filename) + " " + msgID + " retrieved\n";

    return message;
}

string hdr(string field, string messageID)
{
    message = "";
    DIR *dr;
    struct dirent *en;
    DIR *dr2;
    struct dirent *en2;

    vector<string> folderVector;

    ifstream infile;
    ifstream infile2;

    string path = "db/";
    deleteWhitespaces(path);

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0))
            {
                string currentFolder = en->d_name; // db/wired
                deleteWhitespaces(currentFolder);

                string folderPath = "db/" + currentFolder; // db/wired

                folderVector.push_back(folderPath); // first part of path
            }
        }
    }

    string fileToSearchThrough;
    for (int i = 0; i < folderVector.size(); i++)
    {
        string folderPath = folderVector[i]; // db/wired
        deleteWhitespaces(folderPath);
        dr2 = opendir(folderPath.c_str());
        if (dr2)
        {
            while ((en2 = readdir(dr2)) != NULL)
            {
                if ((strcmp(en2->d_name, "..") != 0) && (strcmp(en2->d_name, ".") != 0))
                {
                    string filename = en2->d_name;                 // 2116, etc.
                    string filepath = folderPath + "/" + filename; // db/wired/2116.txt

                    deleteWhitespaces(filepath);

                    infile.open(filepath);
                    string line;

                    if (infile.is_open())
                    {
                        while (getline(infile, line))
                        {
                            if (line.find("Message-ID: ") != string::npos)
                            {
                                line.erase(0, 12);
                                deleteWhitespaces(line);
                                deleteWhitespaces(messageID);
                                // cout<< messageID << line << endl;
                                if (strcmp(line.c_str(), messageID.c_str()) == 0)
                                {
                                    fileToSearchThrough = filepath;
                                    // cout << fileToSearchThrough;
                                    FileNumber = 1;
                                    break;
                                }
                            }
                        }
                        infile.close();
                    }
                }
            }
        }
    }

    deleteWhitespaces(fileToSearchThrough);
    infile2.open(fileToSearchThrough);
    string line2;
    deleteWhitespaces(field);

    if (infile2.is_open())
    {
        while (getline(infile2, line2))
        {
            if (line2.find(field) != string::npos)
            {
                message = "[S] 225 HEADER information follows\n";
                message += line2;
                break;
            }
            else
            {
                if (isXEmpty(message))
                {
                    message = "[S] 430 No article with that field";
                }
            }
        }
        message += "\n";
    }
    infile2.close();

    if (isXEmpty(message))
    {
        message = "[S] 430 No article with that message-id\n";
    }

    return message;
}

string articleMsgID(string messageID)
{
    DIR *dr;
    struct dirent *en;
    DIR *dr2;
    struct dirent *en2;

    vector<string> folderVector;
    vector<string> filenameVector;
    ifstream infile;

    string path = "db/";
    deleteWhitespaces(path);

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0))
            {
                string currentFolder = en->d_name; // db/wired
                deleteWhitespaces(currentFolder);

                string folderPath = "db/" + currentFolder; // db/wired

                folderVector.push_back(folderPath); // first part of path
            }
        }
    }

    string fileToSearchThrough;
    for (int i = 0; i < folderVector.size(); i++)
    {
        string folderPath = folderVector[i]; // db/wired
        deleteWhitespaces(folderPath);
        dr2 = opendir(folderPath.c_str());
        if (dr2)
        {
            while ((en2 = readdir(dr2)) != NULL)
            {
                if ((strcmp(en2->d_name, "..") != 0) && (strcmp(en2->d_name, ".") != 0))
                {
                    string filename = en2->d_name;                 // 2116, etc.
                    string filepath = folderPath + "/" + filename; // db/wired/2116.txt

                    deleteWhitespaces(filepath);
                    filenameVector.push_back(filepath); // db/wired/2116.txt

                    infile.open(filepath);
                    string line;

                    if (infile.is_open())
                    {
                        while (getline(infile, line))
                        {
                            if (line.find("Message-ID: ") != string::npos)
                            {
                                line.erase(0, 12);
                                deleteWhitespaces(line);
                                deleteWhitespaces(messageID);
                                // cout<< messageID << line << endl;
                                if (strcmp(line.c_str(), messageID.c_str()) == 0)
                                {
                                    fileToSearchThrough = filenameVector[i];
                                    FileNumber = 1;

                                    break;
                                }
                            }
                        }
                        infile.close();
                    }
                }
            }
        }
    }

    infile.open(fileToSearchThrough);
    string line;

    if (infile.is_open())
    {
        message = "[S] 220 0 " + messageID + " \n";
        while (getline(infile, line))
        {
            message += line;
            message += "\n";
        }
    }

    if (isXEmpty(message))
    {
        message = "[S] 430 No article with that message-id\n";
    }

    return message;
}

string article(string filename)
{
    string file = "";
    // group has already run, filename is the file you're looking for
    // cout << FileNumber;
    // cout << GroupVector[FileNumber];

    if (isXEmpty(filename))
    {
        // cout << FileNumber << NumOfFilesInDirectory << endl;
        if (FileNumber == NumOfFilesInDirectory + 1)
        {
            message = "[S] 420 Current article number is invalid\n";
        }
        else
        {
            // cout << FileNumber;
            // cout << GroupVector[FileNumber];

            file = to_string(GroupVector[FileNumber]);
        }
    }
    else
    {
        // cout << "2";
        file = filename;
    }

    string filepath = "db/" + globalGroup + "/" + file + ".txt";
    deleteWhitespaces(filepath);
    // cout << filepath;

    ifstream infile;
    infile.open(filepath);
    string line, Article, messageID;

    if (infile.is_open())
    {
        while (getline(infile, line))
        {
            Article += line + "\n";
            if (line.find("Message-ID") != string::npos)
            {
                messageID = line;
                messageID.erase(0, 12);
            }
        }
        message = "[S] 220 " + file + " " + messageID + " \n";
        message += Article;
    }
    else
    {
        if (FileNumber != NumOfFilesInDirectory + 1)
        {
            message = "[S] 423 No article with that number\n";
        }
    }
    return message;
}

string listActive()
{
    // get all folders and group for each folder
    DIR *dr;
    struct dirent *en;
    DIR *dr2;
    struct dirent *en2;

    vector<string> folderVector;
    vector<int> filenameVector;

    string path = "db/";
    deleteWhitespaces(path);

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0))
            {
                string currentFolder = en->d_name; // db/wired
                deleteWhitespaces(currentFolder);

                string folderPath = "db/" + currentFolder; // db/wired

                folderVector.push_back(folderPath); // first part of path
            }
        }
        closedir(dr);
    }

    // want to open each folder ang get the group info from there
    // cout << folderVector[1] <<endl;
    message = "[S] 215 list of newsgroups follows\n";
    for (int i = 0; i < folderVector.size(); i++)
    {
        filenameVector.clear();
        string folderToOpen = folderVector[i];
        deleteWhitespaces(folderToOpen);

        dr2 = opendir(folderToOpen.c_str());
        if (dr2)
        {
            while ((en2 = readdir(dr2)) != NULL)
            {
                if ((strcmp(en2->d_name, "..") != 0) && (strcmp(en2->d_name, ".") != 0))
                {
                    string current = en2->d_name;
                    deleteWhitespaces(current);
                    // cout <<  current <<endl;
                    string file = current.substr(0, current.find_last_of("."));
                    filenameVector.push_back(atoi(file.c_str()));
                }
            }

            sort(filenameVector.begin(), filenameVector.end());

            string nameOfCurrentFolder = folderToOpen.erase(0, 3);

            message += "[S] " + nameOfCurrentFolder + " " + to_string(filenameVector[filenameVector.size() - 1]) + " " + to_string(filenameVector[1]) + " n\n";
            closedir(dr2);
        }
        /*else
        {
            cout << "error";
        }*/
    }
    return message;
}

string listHeaders()
{
    DIR *dr;
    struct dirent *en;
    DIR *dr2;
    struct dirent *en2;

    vector<string> folderVector;
    vector<string> headers;
    ifstream infile;

    string path = "db/";
    deleteWhitespaces(path);

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0))
            {
                string currentFolder = en->d_name; // db/wired
                deleteWhitespaces(currentFolder);

                string folderPath = "db/" + currentFolder; // db/wired

                folderVector.push_back(folderPath); // first part of path
            }
        }
    }

    for (int i = 0; i < folderVector.size(); i++)
    {
        string folderPath = folderVector[i]; // db/wired
        deleteWhitespaces(folderPath);
        dr2 = opendir(folderPath.c_str());
        if (dr2)
        {
            while ((en2 = readdir(dr2)) != NULL)
            {
                if ((strcmp(en2->d_name, "..") != 0) && (strcmp(en2->d_name, ".") != 0))
                {
                    string filename = en2->d_name;                 // 2116.txt, etc.
                    string filepath = folderPath + "/" + filename; // db/wired/2116.txt

                    deleteWhitespaces(filepath);

                    if (filename != ".info")
                    {
                        infile.open(filepath);
                        string line;

                        if (infile.is_open())
                        {
                            while (getline(infile, line))
                            {
                                if (line.find(":") != string::npos)
                                {
                                    string header = line.substr(0, line.find_first_of(":"));
                                    if (find(headers.begin(), headers.end(), header) == headers.end()) // if not already in vector add
                                    {
                                        headers.push_back(header);
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                            infile.close();
                        }
                    }
                }
            }
        }
    }
    message = "[S] 215 List Headers supported:\n";
    for (int i = 0; i < headers.size(); i++)
    {
        message += "[S] " + headers[i] + "\n";
    }

    return message;
}

string listNewsgroups()
{
    // open .info file and read for all
    DIR *dr;
    struct dirent *en;
    DIR *dr2;
    struct dirent *en2;

    ifstream infile;

    string path = "db/";
    deleteWhitespaces(path);
    string infoFileVal = "";

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0))
            {
                string currentFolder = en->d_name; // wired
                deleteWhitespaces(currentFolder);
                // cout << currentFolder;
                string folderPath = path + currentFolder + "/.info"; // db/wired/.info
                // cout << folderPath;

                deleteWhitespaces(folderPath);
                // cout << folderPath;
                infile.open(folderPath);
                string line;

                if (infile.is_open())
                {
                    infoFileVal += "[S] " + currentFolder + " ";
                    while (getline(infile, line))
                    {
                        infoFileVal += line;
                    }

                    infile.close();
                    infoFileVal += "\n";
                    // cout << infoFileVal;
                }
            }
        }
    }
    closedir(dr);
    // cout << message;

    message = "[S] 215 information follows\n";
    message += infoFileVal;

    return message;
}

string getDate()
{
    time_t currentTime = time(0);
    tm *localTime = localtime(&currentTime);

    int year = localTime->tm_year;
    int month = localTime->tm_mon;
    int day = localTime->tm_mday;
    int hour = localTime->tm_hour;
    int min = localTime->tm_min;
    int sec = localTime->tm_sec;
    message = "[S] 111 " + to_string(year + 1900) + to_string(month + 1) + to_string(day) + to_string(5 + hour) + to_string(min + 5) + to_string(sec + 30) + "\n";
    return message;
}

string openInfoFile(string path, string currentFolder)
{
    string infoFileVal;
    string folderPath = path + currentFolder + "/.info"; // db/wired/.info
    // cout << folderPath << endl;

    ifstream infile;
    deleteWhitespaces(folderPath);
    // cout << folderPath;
    infile.open(folderPath);

    string line;

    if (infile.is_open())
    {
        infoFileVal += "[S] " + currentFolder + " ";
        while (getline(infile, line))
        {
            infoFileVal += line;
        }

        infile.close();
        infoFileVal += "\n";
        // cout << infoFileVal;
    }
    return infoFileVal;
}

string ListNewsgroupWildmats(string wildmatInput)
{

    regex wildmatSymbol(wildmatInput + ".*");
    regex wildmatSymbol2("!." + wildmatInput);
    

    DIR *dr;
    struct dirent *en;
    DIR *dr2;
    struct dirent *en2;

    ifstream infile;

    string path = "db/";
    deleteWhitespaces(path);
    string infoFileVal = "";
    int count = 0;

    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            if ((strcmp(en->d_name, "..") != 0) && (strcmp(en->d_name, ".") != 0))
            {
                string currentFolder = en->d_name; // wired

                deleteWhitespaces(currentFolder);

                // search to see if the wildmat stuff is a part of the current folder

                if (wildmatInput.find("*") != string::npos && regex_match(currentFolder, wildmatSymbol)) // if the wildmat matches *, dont have !
                {

                    infoFileVal += openInfoFile(path, currentFolder);
                }
                else if (wildmatInput.find("!") != string::npos)
                {
                    string x;
                    //cout << "here";
                    if (count == 0)
                    {
                        x =  wildmatInput.erase(0, 1);
                        count++;
                    }
                    //cout << x;
                    if (currentFolder != x)
                    {
                        infoFileVal += openInfoFile(path, currentFolder);
                    }
                    else
                    {
                        cout << currentFolder << x;
                    }


                }



                else
                {
                    cout << currentFolder << " didnt match\n";
                }

                // regex_search(currentFolder, matchFlag, wildmatSymbol);

                // cout << currentFolder;
            }
        }
    }
    closedir(dr);
    // cout << message;

    message = "[S] 215 information follows\n";
    message += infoFileVal;

    return message;

    // open .info file and read for all

    return message;
}

string command(char buffer[])
{
    message = "";
    string bufferString = string(buffer);
    for (int i = 0; bufferString[i]; i++)
    {
        bufferString[i] = toupper(bufferString[i]);
    }
    char bufferUppercase[bufferString.length() + 1];
    strcpy(bufferUppercase, bufferString.c_str());
    // cout << bufferUppercase;

    string bufferForCommand = (string)bufferUppercase;
    char delimiter = ' ';
    istringstream stream(bufferForCommand);
    vector<string> bufferParts;
    string part;
    while (getline(stream, part, delimiter))
    {
        bufferParts.push_back(part);
    }
    string commandName = bufferParts[0];

    deleteWhitespaces(commandName);

    if (commandName == "HELP")
    {
        help();
        // cout << message;
    }
    else if (commandName == "QUIT")
    {
        quit();
    }
    else if (commandName == "CAPABILITIES")
    {
        capabilities();
    }
    else if (commandName == "GROUP")
    {
        string filename = (string)buffer;
        filename.erase(0, 6);
        deleteWhitespaces(filename);
        if (isXEmpty(filename))
        {
            message = "[S] 411 No such newsgroup\n";
        }
        else
        {
            group(filename);
        }
    }
    else if (commandName == "LISTGROUP")
    {
        string filename = (string)buffer;
        filename.erase(0, 10);
        string x = filename;
        deleteWhitespaces(x);

        if (isXEmpty(x))
        {
            if (isXEmpty(globalGroup))
            {
                message = "[S] 412  No newsgroup selected\n";
            }
            else
            {
                listgroup(globalGroup);
            }
        }
        else
        {

            listgroup(filename);
        }
    }
    else if (commandName == "NEXT")
    {
        // group has to have run before running next

        if (isXEmpty(globalGroup))
        {
            message = "[S] 412 No newsgroup selected\n";
        }
        else
        {
            if (NumOfFilesInDirectory == -1) // empty folder
            {
                message = "[S] 420 No current article selected\n";
            }
            else if (GroupVector.size() == FileNumber) // last article
            {
                message = "[S] 421 No next article in this group\n";
            }
            else
            {
                next(globalGroup);
            }
        }
    }
    else if (commandName == "HDR")
    {
        // hdr message id
        string filename = (string)buffer;
        char delimiter = ' ';
        istringstream stream(filename);
        vector<string> bufferParts;
        string part;
        while (getline(stream, part, delimiter))
        {
            bufferParts.push_back(part);
        }
        // bufferParts[0] //HDR
        string field = bufferParts[1];
        string messageID = bufferParts[2];

        hdr(field, messageID);
    }
    else if (commandName == "ARTICLE")
    {
        string filename = (string)buffer;
        filename.erase(0, 8);
        deleteWhitespaces(filename);
        if (filename.find("<") != string::npos) // message-id
        {
            articleMsgID(filename);
        }
        else
        {
            if (isXEmpty(globalGroup)) // group hasnt run
            {
                message = "[S] 412 No newsgroup selected\n";
            }
            else
            {
                article(filename);
                if (isXEmpty(filename))
                {
                    FileNumber++;
                }
            }
        }
    }
    else if (commandName == "DATE")
    {
        getDate();
    }
    else if (commandName == "LIST")
    {
        commandName += " " + bufferParts[1];
        if (commandName == "LIST HEADERS")
        {
            listHeaders();
        }
        else if (commandName == "LIST ACTIVE") // TODO: REGEX
        {
            listActive();
        }
        else if (commandName == "LIST NEWSGROUPS") // TODO: REGEX
        {
            // listNewsgroups();
            if (bufferParts[2] != "")
            {
                cout << "doing wildmats";
                ListNewsgroupWildmats(bufferParts[2]);
            }
            else
            {
                listNewsgroups();
            }
        }
        else
        {
            message = "[S] 500 Invalid command\n";
        }
    }
    else
    {
        message = "[S] 500 Invalid command\n";
    }

    bzero(bufferUppercase, sizeof(bufferUppercase));
    return message;
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
        }
        linecount++;
    }
    return toReturn;
}

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

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
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buffer[100000];

    if (argc != 2)
    {
        printf("Argument Error");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    string configFile = argv[1];

    if ((rv = getaddrinfo(NULL, getInfoFromFile(configFile, 1).c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "[S] getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("[S] Bind Failed\n");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "[S] Bind Failed\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("[S] Listen Error\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("Sigaction error");
        exit(1);
    }

    printf("[S] Waiting for connection\n");

    while (true)
    {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("[S] Accept Failed\n");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        cout << "[S] Client Accepted\n";

        // send(new_fd, buffer, sizeof(buffer), 0);
        if (!fork())
        {
            close(sockfd);
            while (true)
            {
                bzero(buffer, sizeof(buffer));

                int received = read(new_fd, buffer, sizeof(buffer));
                if (received == -1)
                {
                    cout << "receiving failed";
                }

                printf("Client: %s\n", buffer);

                command(buffer);

                bzero(buffer, sizeof(buffer));

                send(new_fd, message.c_str(), strlen(message.c_str()), 0);

                bzero(buffer, sizeof(buffer));
            }
        }
        // close(sockfd);
        close(new_fd); // parent doesn't need this
    }

    return 0;
}