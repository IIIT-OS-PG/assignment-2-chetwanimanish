/*Tracker*/
#include <bits/stdc++.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <vector>

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define LEVEL SOL_SOCKET
#define SET_OPTIONS SO_REUSEADDR
#define BUFF_SIZE 512*1024

using namespace std;

/*STRUCTS DECLARATION*/

struct client_params
{
    int file_fd;
    int port_to_read;
    string ip_addr;
    int tracker_no;
};

struct user_value
{
    int online_status;
    string passwd;
    vector<int> groups;
};

struct peer_file
{
    string path,ip;
    int port;
};

struct file_details
{
    int size;
    vector<peer_file> peer_having_files; //IP and Port of Peer where file is available
};

struct group_details
{
    string group_owner_user_id;
    unordered_map<string,file_details> file_data;   //key is SHA of file
};

struct group_join_req
{
    unordered_set<string> members;
};

/*STRUCTS DECLARATION*/

/*DATA STRUCTRES DECLARATION*/

unordered_map<string,user_value> user_details;  //user_id is key
unordered_map<int,group_details> group_data;    //group_no is key
unordered_map<int,group_join_req> group_join_request;    //group_id is key

/*DATA STRUCTRES DECLARATION*/

/*FUNCTIONS START*/
int syn_tracker(int sock_fd, string other_ip, int other_port)
{
    struct sockaddr_in other_track;
    int connect_stat;
    other_track.sin_family=DOMAIN;
    other_track.sin_port=htons(other_port);
    if(inet_pton(DOMAIN, other_ip.c_str(), &other_track.sin_addr)<0)
    {
        cout<<"Address Conversion Error"<<endl;
    }
    connect_stat=connect(sock_fd,(struct sockaddr *)&other_track, sizeof(other_track));
    if(connect_stat<0)
    {
        cout<<"Connection with other tracker not possible"<<endl;
    }
    else
    {
        //SYN Can be Done
        cout<<"Write SYN Process"<<endl;
        close(sock_fd);
    }
    return connect_stat;
}

int set_socket(struct sockaddr_in self_track,int option_set,int port)
{
    int sock_fd=socket(DOMAIN,TYPE,PROTOCOL);
    if(sock_fd<0)
    {
        cout<<"Cannot get FD for socket"<<endl;
        exit(0);
    }
    //Setting to reuse socket again
    //cout<<"Sock FD="<<sock_fd<<endl;
    if(setsockopt(sock_fd,LEVEL,SET_OPTIONS,(void *)&option_set,sizeof(int))<0)
    {
        cout<<"SETTING OPTION : "<<errno<<endl;
        cout<<"Error in setting option"<<endl;
        exit(0);
    }
    self_track.sin_family=DOMAIN;
    self_track.sin_port=htons(port);
    self_track.sin_addr.s_addr = INADDR_ANY;
    if(bind(sock_fd,(struct sockaddr *)&self_track,sizeof(self_track))<0)
    {
        cout<<"Binding Failed "<<errno<<endl;
    }
    return sock_fd;
}

int execute_command(string &command, string &response,client_params* p,int &login_stat,string &user_id_syn,int &sync) {
    response="Default Executed";
    stringstream div_comm(command);
    if(command.compare(0,11,"create_user")==0)
    {
        //first finding if already exist
        string user_id,passwd;
        getline(div_comm, user_id, ' ');
        getline(div_comm, user_id, ' ');
        getline(div_comm, passwd, ' ');
        //cout<<"Creating user with user_id "<<user_id<<" Password = "<<passwd<<endl;
        if(user_details.find(user_id) == user_details.end())    //NOT FOUND
        {
            user_details[user_id].passwd = passwd;
            user_details[user_id].online_status = 1;
            sync = 1;
            response = "Added User";
        }
        else
        {
            response = "UserID Already Taken";
        }
    }
    else if(command.compare(0,5,"login")==0)
    {
        string user_id,passwd;
        getline(div_comm, user_id, ' ');
        getline(div_comm, user_id, ' ');
        getline(div_comm, passwd, ' ');
        if(user_details.find(user_id) == user_details.end())	//check if usr exist
        {
            response = "UserID Does Not Exist";
        }
        else
        {
            if(strcmp(user_details[user_id].passwd.c_str(),passwd.c_str())==0)
            {
                user_id_syn = user_id;
                login_stat = 1;
                response = "Logged In Successfully";
            }
            else
            {
                response = "Check Password";
            }
        }
    }
    else if(command.compare(0,12,"create_group")==0 && login_stat==1)
    {
        int group_no;
        string temp;
        getline(div_comm, temp, ' ');
        getline(div_comm, temp, ' ');
        group_no = stoi(temp);
        if(group_data.find(group_no) == group_data.end())   //Group Not Found
        {
            group_data[group_no].group_owner_user_id = user_id_syn;
            sync = 1;
            user_details[user_id_syn].groups.push_back(group_no);
            response = "Group Successfully Added";
            sync=1;
        }
        else
        {
            response = "Group with this group ID Already Exist";    //38
        }
    }
    else if(command.compare(0,10,"join_group")==0 && login_stat==1 )
    {
        sync=1;
        int group_no;
        string temp;
        getline(div_comm, temp, ' ');
        getline(div_comm, temp, ' ');
        group_no = stoi(temp);
        group_join_request[group_no].members.insert(user_id_syn);
        response = "Request Added Successfully!";
    }
    else if(command.compare(0,11,"leave_group")==0 && login_stat==1 )
    {
        int group_no;
        string temp;
        getline(div_comm, temp, ' ');
        getline(div_comm, temp, ' ');
        group_no = stoi(temp);
        vector<int>::iterator itr = find(user_details[user_id_syn].groups.begin(),user_details[user_id_syn].groups.end(),group_no);
        if(itr == user_details[user_id_syn].groups.end() )
        {
            response = "User Already Not in Group";
        }
        else
        {
            user_details[user_id_syn].groups.erase(itr);
            response = "User Removed Successfully";
            sync=1;
        }
    }
    else if(command.compare(0,13,"list_requests")==0 && login_stat==1 )
    {
        int group_no;
        string temp;
        getline(div_comm, temp, ' ');
        getline(div_comm, temp, ' ');
        group_no = stoi(temp);
        vector<int>::iterator itr = find(user_details[user_id_syn].groups.begin(),user_details[user_id_syn].groups.end(),group_no);
        if(itr == user_details[user_id_syn].groups.end())
        {
            response = "Cannot Access Other Groups Details";
        }
        else
        {
            cout<<" UserId Requested are : "<<endl;
            response = " UserId Requested are : \n";
            unordered_set<string> :: iterator itr;
            for (itr = group_join_request[group_no].members.begin(); itr != group_join_request[group_no].members.end(); itr++){
                response += *itr;
                response += "\n";
            }
        }
    }
    else if(command.compare(0,14,"accept_request")==0 && login_stat==1 )
    {
        string user_id;
        int group_id;
        getline(div_comm, user_id, ' ');
        getline(div_comm, user_id, ' ');
        group_id = stoi(user_id);
        getline(div_comm, user_id, ' ');
        if(strcmp(group_data[group_id].group_owner_user_id.c_str(),user_id_syn.c_str())==0)
        {
            //Group Owner has issued command
            user_details[user_id].groups.push_back(group_id);
            response = "User Added Successfully";
            sync=1;
        }
        else
        {
            response = "You are not Group Owner";
        }
    }
    else if(command.compare(0,11,"list_groups")==0 && login_stat==1 )
    {
        unordered_map<int,group_details>::iterator itr;
        response = "Group Nos Present are:\n";
        for(itr = group_data.begin(); itr!=group_data.end();itr++)
        {
            response += to_string(itr->first);
            response += "\n";
        }
    }
    else if(command.compare(0,10,"list_files")==0 && login_stat==1 )
    {
        int group_no;
        string temp;
        getline(div_comm, temp, ' ');
        getline(div_comm, temp, ' ');
        group_no = stoi(temp);
        unordered_map<string,file_details>::iterator itr;
        response = "Name of all Shareable files in Group are:\n\n";
        for( itr = group_data[group_no].file_data.begin(); itr != group_data[group_no].file_data.end();itr++ )
        {
            response += (itr->first);
            response += "\n";
        }
    }
    else if(command.compare(0,11,"upload_file")==0 && login_stat==1 )
    {
        string path,file_name,ip;
        int port,group_id,size;
        getline(div_comm, path , ' ');
        getline(div_comm, path , ' ');
        getline(div_comm, file_name , ' ');
        group_id = stoi(file_name);
        getline(div_comm, file_name , ' ');
        size = stoi(file_name);
        getline(div_comm, ip , ' ');
        getline(div_comm, file_name , ' ');
        port = stoi(file_name);
        /*Extracting File Name*/
        stringstream file_name_ext(path);
        while(getline(file_name_ext, file_name , '/'));
        //cout<<"File Name is"<<file_name;
        /*Extracting File Name*/
        group_data[group_id].file_data[file_name].size=size;
        peer_file temp;
        temp.path=path;
        temp.ip=ip;
        temp.port=port;
        group_data[group_id].file_data[file_name].peer_having_files.push_back(temp);
        response ="File Added Successfully!\n";
    }
    else if(command.compare(0,13,"download_file")==0 && login_stat==1 )
    {
        string file_name;
        int group_id;
        getline(div_comm, file_name, ' ');
        getline(div_comm, file_name, ' ');
        group_id = stoi(file_name);
        getline(div_comm, file_name, ' ');
        response = to_string(group_data[group_id].file_data[file_name].size);
        response += "\n";
        for(int i=0;i<group_data[group_id].file_data[file_name].peer_having_files.size();i++)
        {
            response += group_data[group_id].file_data[file_name].peer_having_files[i].path;
            response += " ";
            response += group_data[group_id].file_data[file_name].peer_having_files[i].ip;
            response += " ";
            response += to_string(group_data[group_id].file_data[file_name].peer_having_files[i].port);
            response += "\n";
        }
    }
    else if(command.compare(0,14,"show_downloads")==0 && login_stat==1 )
    {

    }
    else if(command.compare(0,10,"stop_share")==0 && login_stat==1 )
    {

    }
    else
    {
        response="Enter Proper Command or not logged in!!\n";
    }
    return 1;
}

void *client_func(void* params)
{
    client_params* p=(client_params*)params;
    int login_stat=0;
    int sync=0;
    string user_id;
    //cout<<"New Child Created Port is:"<<p->port_to_read<<endl;
    char buff[BUFF_SIZE]={0};
    string command,response;
    int valread;
    while(valread = read( p->file_fd , buff, BUFF_SIZE))
    {
        sync=0;
        if(valread<0)
        {
            cout<<"Read Error"<<endl;
        }
        command=buff;
        //cout<<"Command Received is:"<<command<<endl;
        if(execute_command(command,response,p,login_stat,user_id,sync)<0)
        {
            cout<<"Error in executing command"<<endl;
        }
        if(sync==1 && p->tracker_no==0)
        {
            //send command to other tracker
        }
        const void * comm_send = response.c_str();
        //cout<<"Response to send is:"<<response.c_str()<<endl;
        int send_stat=send(p->file_fd,comm_send,response.length(),0);
        if(send_stat<0)
        {
            cout<<"Sending Error"<<endl;
        }
        memset(&buff[0], 0, sizeof(buff));
    }
}

/*FUNCTIONS START*/

int main(int argc, char *argv[])
{
    if(argc!=3)
    {
        cout<<"Enter Proper Arguments"<<endl;
        exit(0);
    }
    unordered_map<int,pair<string,int>> tracker_data;
    client_params params_to_pass[SOMAXCONN];
    int params_pass_count=0;
    string in,quit="quit";
    int self=stoi(argv[2],NULL,10),other=1-self;
    int sock_fd;
    int option_set=1,accept_stat=0;
    struct sockaddr_in self_track;
    struct sockaddr_in new_track;
    socklen_t new_track_size;
    ifstream tracker_details(argv[1]);
    if(!tracker_details.is_open())		//check if file describe is opened properly
    {
        cout<<"File Open Error"<<endl;
        exit(0);
    }
    int temp,tmp;
    string temp2;
    while(tracker_details>>temp)		//reading tracker_info
    {
        tracker_details>>temp2;
        tracker_details>>tmp;
        //cout<<"Track_no="<<temp<<" IP="<<temp2<<" Port="<<tmp<<endl;
        tracker_data[temp]=make_pair(temp2,tmp);
    }
    /*Creating socket*/
    sock_fd=set_socket(self_track,option_set,tracker_data[self].second);   //returns sock_fd after creation and also bind the socket
    /*Creating socket*/
    /*SYN with the other tracker if present else this is single tracker so no SYN req*/
    if(syn_tracker(sock_fd, tracker_data[other].first, tracker_data[other].second) < 0)
    {
        cout<<"Synchronization Failed!\nCheck Other Tracker!"<<endl;
    }
    else    //SET SOCKET AGAIN
    {
        cout<<"Sync Done"<<endl;
        sock_fd=set_socket(self_track,option_set,tracker_data[self].second);
    }
    /*SYN with the other tracker if present else this is single tracker so no SYN req*/
    /*Start with Listening for Tracker*/
    if(listen(sock_fd,SOMAXCONN)<0) //SOMAXCONN is max connections to queue
    {
        cout<<"Listening Error"<<endl;
    }
    cout<<"Give Input"<<endl;
    cin>>in;
    while((strcmp(in.c_str(),quit.c_str())!=0) && (accept_stat=accept(sock_fd, (struct sockaddr*)&new_track,&new_track_size)))
    {
        new_track_size=sizeof(new_track);
        if(accept_stat<0)
        {
            cout<<"Problem in Accepting connection error no="<<errno<<endl;
        }
        //cout<<"Creating New Thread"<<endl;
        char input_ip[INET_ADDRSTRLEN];
        cout<<"Client connected IP:"<<inet_ntop(DOMAIN,&new_track.sin_addr,input_ip,sizeof(input_ip))<<" Port Number="<<ntohs(new_track.sin_port)<<endl;
        /*Receive From Child the Command*/
        pthread_t child;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
        params_to_pass[params_pass_count].file_fd=accept_stat;
        params_to_pass->tracker_no=self;
        int client_fd = pthread_create(&child,&attr,client_func,(void *)&(params_to_pass[params_pass_count++]));
        if(client_fd)
        {
            cout<<"Error in Creating Client Thread"<<endl;
        }
        if(pthread_detach(child)!=0)
        {
            cout<<"Problem in Detach"<<endl;
        }
        /*Receive From Child the Command*/
        cout<<"Give Input"<<endl;
        cin>>in;
    }
    /*Start with Listening for Tracker*/
    close(sock_fd);
    return 0;
}

/*Tracker*/
