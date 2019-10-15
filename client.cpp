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
#include <sstream>
#include <math.h>
#include <stdio.h>

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define LEVEL SOL_SOCKET
#define SET_OPTIONS SO_REUSEADDR
#define BUFF_SIZE 512*1024
#define CONVERT_KB (512*1024)
#define MAX_PEERS 5
#define SMALL_SIZE 4096

using namespace std;

int *child_thread_fds;

struct file_tukde
{
    string ip,path;
    int port;
    int to_contact;
    vector<int> piece_present;
};

struct parameter
{
    int port;
    string ip;
};

struct param_child_piece
{
    int port,piece_no;
    string ip;
    string server_path;
    string self_path;
};

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

int connect_server(int port,string ip,int sock_fd)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family=DOMAIN;
    server_addr.sin_port=htons(port);
    if(inet_pton(DOMAIN, ip.c_str(), &server_addr.sin_addr)<0)
    {
        cout<<"Problem in converting IP"<<endl;
    }
    int connect_stat=connect(sock_fd,(struct sockaddr *)&server_addr, sizeof(server_addr));
    return connect_stat;
}

/*void create_command(string command,vector<string> &built)
{
    //cout<<"Command Received is:"<<command<<endl;
    built.clear();
    stringstream check1(command);
    string intermediate;
    // Tokenizing w.r.t. space ' '
    while(getline(check1, intermediate, ' '))
    {
        built.push_back(intermediate);
    }
    //Setting opcode
    if(strcmp(built[0].c_str(),"create_user")==0)
    {
        built[0]="0";
    }
}*/

int find_pieces_info(string response , vector<file_tukde> &peers_to , vector<int> &to_take, long long int &a,long long int &b) //file_size,file_name,file_path
{
    stringstream div_resp(response);
    string temp;
    long long int pieces_found=0;
    getline(div_resp, temp);
    long long int file_size = stoi(temp);
    a = file_size;
    long long int pieces;
    pieces = ceil((double)file_size/CONVERT_KB);
    b = pieces;
    cout<<"Size of File in KBytes is:"<<file_size<<endl;
    cout<<"Number of Pieces : "<<pieces<<endl;
    //Setting no pieces found
    for(int i=0;i<pieces;i++)
    {
        to_take.push_back(-1);
    }
    //getting all peers available
    while(getline(div_resp, temp)) {
        file_tukde temp_s;
        string part_data;
        stringstream data(temp);
        getline(data,part_data,' ');
        temp_s.path=part_data;
        //cout<<"Path is:"<<part_data<<endl;
        getline(data,part_data,' ');
        temp_s.ip=part_data;
        //cout<<"IP is:"<<part_data;
        getline(data,part_data,' ');
        temp_s.port=stoi(part_data);
        //cout<<"Port is:"<<part_data<<endl;
        peers_to.push_back(temp_s);
    }
    int sock_fd[peers_to.size()];   //opening sockets with all
    for(int i=0;i<peers_to.size();i++)
    {
        sock_fd[i] = socket(DOMAIN, TYPE, PROTOCOL);    //open socket for all possible pieces
        if(sock_fd<0)
        {
            cout<<"Error in opening pieces port"<<endl;
        }
        int connect_stat=connect_server(peers_to[i].port,peers_to[i].ip,sock_fd[i]);
        if(connect_stat<0)
        {
            cout<<"Cannot connect to server for part_fetch"<<endl;
            peers_to[i].to_contact=0;
        }
        else
        {
            peers_to[i].to_contact=1;
        }
    }
    cout<<"Number of peers to query are:"<<peers_to.size()<<endl;
    while(pieces_found<pieces)
    {
        //cout<<"Pieces Found "<<pieces_found<<endl;
        for(int i=0;i<peers_to.size();i++)
        {
            if(peers_to[i].to_contact)
            {
                int found=0;
                while(!found)       //find one piece from each peer
                {
                    //cout<<"Asking peer "<<i<<endl;
                    string msg = "which ";
                    msg += peers_to[i].path;
                    //cout<<"Sending from client"<<msg<<endl;
                    const void * comm_send = msg.c_str();
                    int send_stat=send(sock_fd[i],comm_send,msg.length(),0);
                    if(send_stat<0) //peer is inactive then go to next peer
                    {
                        cout<<"Sending Error"<<endl;
                        peers_to[i].to_contact=0;
                        break;
                    }
                    char buff[BUFF_SIZE]={0};
                    int valread=0;
                    string number;
                    int num;
                    valread = read( sock_fd[i] , buff, BUFF_SIZE);
                    if(valread<0)
                    {
                        cout<<"Read Error"<<endl;
                    }
                    number=buff;
                    //cout<<"Received "<<number<<endl;
                    num = stoi(number);
                    if(num==-1) //no pieces further with this peer
                    {
                        peers_to[i].to_contact=0;
                        break;
                    }
                    if(to_take[num]==-1)
                    {
                        found=1;
                        to_take[num]=i;
                        pieces_found++;
                    }
                    number.clear();
                }
            }
        }
    }
    //cout<<"A"<<endl;
}

long long int find_file_size(string path)
{
    FILE *file_name = fopen(path.c_str(),"rb");
    fseek(file_name,0,SEEK_END);
    long long int temp = ftell(file_name);
    fclose(file_name);
    return temp;
}

int create_blank_file(long long int length,string &name,string &path)
{
    FILE *pFile = NULL;
    string temp = path+name;
    pFile=fopen(temp.c_str(), "wb+" );
    char nulla[]="";
    if(fseek(pFile, length, SEEK_SET)!=0)
        cout<<"Seek Error in creating new file"<<endl;
    fwrite(nulla,1,1,pFile);
    //cout<<"Blank File created as: "<<temp<<" with size= "<<length<<endl;
    fclose( pFile );
}

void *get_piece(void* params)
{
    param_child_piece *p = (param_child_piece *)params;
    //cout<<"Getting piece no "<<p->piece_no<<" from IP: "<<p->ip<<" Port "<<p->port<<endl;
    cout<<"Reading Piece No : "<<p->piece_no<<endl;
    /*Generate Start Msg*/
    string msg = "send ";
    string exit = "exit";
    string ack = "1";
    msg += p->server_path;
    msg += " ";
    msg += to_string(p->piece_no);
    /*Generate Start Msg*/
    /*Open Connection to send*/
    int connect_stat;
    int sock_fd=socket(DOMAIN, TYPE, PROTOCOL);
    if(sock_fd<0)
    {
        cout<<"Error in opening client port"<<endl;
    }
    connect_stat=connect_server(p->port,p->ip,sock_fd);
    if(connect_stat<0)
    {
        cout<<"Cannot connect to peer to get piece"<<endl;
    }
    /*Open Connection to send*/
    /*Send First Msg*/
    const void * msg_send = msg.c_str();
    int send_stat=send(sock_fd,msg_send,msg.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    /*Send First Msg*/
    /*Reading File Size*/
    char buff[BUFF_SIZE]={0};
    //cout<<"Reading Chunck Size"<<endl;
    int valread = read( sock_fd , buff, BUFF_SIZE);
    if(valread<0)
    {
        cout<<"Reading Chunck Size Error in piece "<<p->piece_no<<endl;
    }
    string response_recv=buff;
    int chuck_size = stoi(response_recv);
    //cout<<"Chuck size to receive is: "<<chuck_size<<endl;
    /*Reading File Size*/
    /*SENDING ACK TO SEND FILE*/
    msg_send = ack.c_str();
    send_stat=send(sock_fd,msg_send,ack.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    /*SENDING ACK TO SEND FILE*/
    /*RECEIVE FILE STARTS*/
    memset(&buff[0], 0, sizeof(buff));
    FILE *file_name = fopen(p->self_path.c_str(),"r+");
    fseek(file_name,CONVERT_KB*p->piece_no,SEEK_SET);
    int no_time_recv = ceil((double)chuck_size/SMALL_SIZE);
    for(int i=0;i<no_time_recv;i++)
    {
        //cout<<"Receiving "<<i<<"th subpart of piece "<<p->piece_no<<endl;
        valread = read( sock_fd , buff, SMALL_SIZE);
        if(valread<0)
        {
            cout<<"Client Piece Read Error"<<endl;
        }
        if(chuck_size < SMALL_SIZE)
            fwrite(buff,1,chuck_size,file_name);
        else
            fwrite(buff,1,SMALL_SIZE,file_name);
        chuck_size -= SMALL_SIZE;
        send_stat=send(sock_fd,msg_send,ack.length(),0);
        if(send_stat<0)
        {
            cout<<"Sending Error"<<endl;
        }
        memset(&buff[0], 0, sizeof(buff));
    }
    /*RECEIVE FILE STARTS*/
    cout<<"Done fetching piece_no "<<p->piece_no<<endl;
    string piece_no = to_string(p->piece_no);
    piece_no += "\n";
    string temp_file_nm = p->self_path + "_temp";
    fstream temp_of_file(temp_file_nm.c_str(),fstream::in);
    if(!temp_of_file.is_open())
    {
        temp_of_file.open(temp_file_nm.c_str(), fstream::in | std::ios_base::out | fstream::trunc);
    }
    temp_of_file<<piece_no;
    temp_of_file.close();   //Add -1 at end of file after all done
    msg_send = exit.c_str();
    send_stat=send(sock_fd,msg_send,exit.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    fclose(file_name);
    close(sock_fd);
}

void *client_func(void* params)
{
    void* status;
    parameter *p=(parameter *)params;
    //cout<<"Client Created"<<endl;
    //cout<<"Client IP:"<<p[1].ip<<endl;
    //cout<<"Client Port:"<<p[1].port<<endl;
    string command,logout="logout";
    vector<string> built;
    int connect_stat;
    int sock_fd=socket(DOMAIN, TYPE, PROTOCOL);
    if(sock_fd<0)
    {
        cout<<"Error in opening client port"<<endl;
    }
    connect_stat=connect_server(p[0].port,p[0].ip,sock_fd);
    if(connect_stat<0)
    {
        connect_stat=connect_server(p[1].port,p[1].ip,sock_fd);
        if(connect_stat<0)
        {
            cout<<"Cannot connect to any tracker"<<endl;
        }
    }
    //cout<<"Connection With Tracker Done"<<endl;
    /*Actual Command Processing Starts*/
    cout<<"Enter Command:"<<endl;
    getline(cin,command);
    while(strcmp(command.c_str(),logout.c_str())!=0)
    {
        /*Add extra details for upload command*/
        if(command.compare(0,11,"upload_file")==0)
        {
            /*Appending File Size*/
            string file_path;
            stringstream div_comm(command);
            getline(div_comm, file_path, ' ');
            getline(div_comm, file_path, ' ');
            long long int size_of_file = find_file_size(file_path);
            /*Appending File Size*/
            command += " ";
            command += to_string(size_of_file);
            command += " ";
            command += p[2].ip;
            command += " ";
            command += to_string(p[2].port);
        }
        const void * comm_send = command.c_str();
        //cout<<"Entered Command:"<<command.c_str()<<endl;
        /*Add extra details for upload command*/
        int send_stat=send(sock_fd,comm_send,command.length(),0);
        if(send_stat<0)
        {
            cout<<"Sending Error"<<endl;
        }
        char buff[BUFF_SIZE]={0};
        int valread = read( sock_fd , buff, BUFF_SIZE);
        if(valread<0)
        {
            cout<<"Command Respone Read Error"<<endl;
        }
        string response_recv=buff;
        if(command.compare(0,13,"download_file")==0)
        {
            //cout<<"Response received is:"<<response_recv<<endl;
            vector<file_tukde> peers_to;
            vector<int> to_take;
            long long int file_size;
            long long int pieces_to_take;
            long long int piece_no=0;
            string file_name="",file_path="";
            find_pieces_info(response_recv,peers_to,to_take,file_size,pieces_to_take);   //gives info of pieces all the clients have in peers_to's vector
            //for(int i=0;i<to_take.size();i++)
            //    cout<<i<<"th Piece from "<<to_take[i]<<" peer"<<endl;
            //finding file name and dest_path
            stringstream dest_data(command);
            getline(dest_data , file_name ,' ');
            getline(dest_data , file_name ,' ');
            getline(dest_data , file_name ,' ');
            getline(dest_data , file_path ,' ');
            //create_blank_file of required size
            create_blank_file(file_size-1,file_name,file_path);
            //cout<<"Blank File Created"<<endl;
            param_child_piece para[MAX_PEERS];
            pthread_t get_piece_child[MAX_PEERS];
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
            while(pieces_to_take > MAX_PEERS)
            {
                //CREATE CHILD CLIENT THREADS EQUAL TO NUMBER OF MAX_CONNECTIONS
                for(int i=piece_no;i<piece_no+MAX_PEERS;i++)
                {
                    para[i-piece_no].piece_no = i;
                    para[i-piece_no].port = peers_to[to_take[i]].port;
                    para[i-piece_no].ip = peers_to[to_take[i]].ip;
                    para[i-piece_no].server_path = peers_to[to_take[i]].path;
                    para[i-piece_no].self_path = file_path+file_name;
                    if(pthread_create(&get_piece_child[i-piece_no],&attr,get_piece,(void *)&(para[i-piece_no])))
                    {
                        cout<<"Error in Creating GetPiece Thread"<<endl;
                    }
                }
                //Waiting for this to complete
                for(int i=piece_no;i<piece_no+MAX_PEERS;i++)
                {
                    pthread_join(get_piece_child[i-piece_no],&status) ;
                }
                //cout<<"Join Done"<<endl;
                pieces_to_take -= MAX_PEERS;
                piece_no += MAX_PEERS;
                //cout<<"Itearting Again"<<endl;
            }
            //DO FOR REMAINING PIECES
            //cout<<"Getting Remaining Pieces"<<endl;
            for(int i=piece_no;i<piece_no+pieces_to_take;i++)
            {
                para[i-piece_no].piece_no = i;
                para[i-piece_no].port = peers_to[to_take[i]].port;
                para[i-piece_no].ip = peers_to[to_take[i]].ip;
                para[i-piece_no].server_path = peers_to[to_take[i]].path;
                para[i-piece_no].self_path = file_path+file_name;
                if(pthread_create(&get_piece_child[i-piece_no],&attr,get_piece,(void *)&(para[i-piece_no])))
                {
                    cout<<"Error in Creating GetPiece Thread"<<endl;
                }
            }
            //Waiting for this to complete
            for(int i=piece_no;i<piece_no+pieces_to_take;i++)
            {
                pthread_join(get_piece_child[i-piece_no],&status) ;
            }
            //cout<<"Remaining Pieces Done"<<endl;
            //DO FOR REMAINING PIECES
            string temp_file_nm = file_path + file_name + "_temp";
            fstream temp_of_file(temp_file_nm.c_str(),fstream::in);
            if(!temp_of_file.is_open())
            {
                temp_of_file.open(temp_file_nm.c_str(), fstream::in | std::ios_base::out | fstream::trunc);
            }
            temp_of_file<<to_string(-1);
            temp_of_file.close();
        }
        cout<<response_recv<<endl;
        cout<<"Enter Command:"<<endl;
        getline(cin,command);
    }
    /*Actual Command Processing Starts*/
}

void send_pieces(string file_path, int client_fd, fstream &tukde_info)
{
    //cout<<"Sending Pieces info for file "<<file_path<<endl;
    string temp_file_path=file_path;
    temp_file_path += "_temp";
    //cout<<"Sending Pieces info for file "<<temp_file_path<<endl;
    if(!tukde_info.is_open())
    {
        tukde_info.open(temp_file_path.c_str(),fstream::in);
        if(!tukde_info.is_open()) {
            cout << "File Does not exist" << endl;
            fstream create_aux(temp_file_path.c_str(), fstream::in | std::ios_base::out | fstream::trunc);
            long long int file_size = find_file_size(file_path);
            long long int no_of_pieces = ceil((double) file_size / CONVERT_KB);
            for (long long int i = 0; i < no_of_pieces; i++) {
                create_aux << to_string(i);
                create_aux << "\n";
            }
            create_aux << to_string(-1);
            create_aux.close();
            tukde_info.open(temp_file_path.c_str(),fstream::in);
        }
    }
    /*if(!tukde_info.is_open())		//check if file describe is opened properly
    {
        tukde_info.open(temp_file_path.c_str(), fstream::in | std::ios_base::out | fstream::trunc);
        //File Does not exist so all pieces are present
        long long int file_size = find_file_size(file_path);
        long long int no_of_pieces = ceil((double)file_size/CONVERT_KB);
        for(long long int i=0;i<no_of_pieces;i++)
        {
            //send all i's
            /*string part_no = to_string(i);
            errno=0;
            const void * part_no_to_send = part_no.c_str();
            int send_stat=send(client_fd , part_no_to_send ,part_no.length(),0);
            if(send_stat<0)
            {
                cout<<"Sending Error in piece info "<<errno<<endl;
            }
            sleep(1);
            tukde_info<<i;
            tukde_info<<"\n";
        }
        int i=-1;
        tukde_info<<i;
    }*/
    string temp;
    tukde_info>>temp;
    //cout<<"Sending "<<temp<<endl;
    const void * part_no_to_send = temp.c_str();
    int send_stat=send(client_fd , part_no_to_send ,temp.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Error"<<endl;
    }
    //read file and send all the parts present
    /*Read and send when required*/
    /*string part_no;
    tukde_info>>part_no;
    while(!tukde_info.eof())
    {
        const void * part_no_to_send = part_no.c_str();
        int send_stat=send(client_fd , part_no_to_send ,part_no.length(),0);
        if(send_stat<0)
        {
            cout<<"Sending Error"<<endl;
        }
        tukde_info>>part_no;
    }*/
}

long long int find_size_remain(string path,int piece_no)
{
    FILE *file_name = fopen(path.c_str(),"rb");
    long long int skip_blocks = piece_no ;
    skip_blocks *= CONVERT_KB;
    fseek(file_name,0,SEEK_END);
    long long int temp = ftell(file_name);
    fclose(file_name);
    //cout<<"Remaning Size is:"<<temp-skip_blocks<<endl;
    return temp-skip_blocks;
}

int send_chunck_no(int sock_fd , string path, int piece_no )
{
    cout<<"Sending piece no "<<piece_no<<endl;
    long long int file_size = find_size_remain(path,piece_no);
    if(file_size >= CONVERT_KB )
        file_size = CONVERT_KB;
    /*FILE SIZE TO SEND*/
    string msg = to_string(file_size);
    //cout<<"Sending file size "<<msg<<endl;
    const void * msg_send = msg.c_str();
    int send_stat=send(sock_fd,msg_send,msg.length(),0);
    if(send_stat<0)
    {
        cout<<"Sending Piece Size Error"<<endl;
    }
    /*FILE SIZE TO SEND*/
    char buff[SMALL_SIZE]={0};
    int valread = read( sock_fd , buff, BUFF_SIZE);
    if(valread<0)
    {
        cout<<"Read File Size Error"<<endl;
    }
    memset(&buff[0], 0, sizeof(buff));
    /*SETTING UP THE FILE*/
    FILE* ftp = fopen(path.c_str(),"r+");
    fseek(ftp,CONVERT_KB*piece_no,SEEK_SET);
    /*SETTING UP THE FILE*/
    /*READ AND SEND DATA REPEATEDLY*/
    while(file_size>0)
    {
        if(file_size <= SMALL_SIZE && file_size>0)
        {
            fread(buff,file_size,1,ftp);
            send_stat=send(sock_fd,buff,file_size,0);
            if(send_stat<0)
            {
                cout<<"Sending Piece Size Error"<<endl;
            }
        }
        else
        {
            fread(buff,SMALL_SIZE,1,ftp);
            send_stat=send(sock_fd,buff,SMALL_SIZE,0);
            if(send_stat<0)
            {
                cout<<"Sending Piece Size Error"<<endl;
            }
        }
        //cout<<"Piece sent"<<endl;
        valread = read( sock_fd , buff, BUFF_SIZE);
        //cout<<"ACK Read done"<<endl;
        file_size -= SMALL_SIZE;
        memset(&buff[0], 0, sizeof(buff));
    }
    fclose(ftp);
    /*READ AND SEND DATA REPEATEDLY*/
}

void *server_child(void* params)
{
    //cout<<"\n\nNew Server Child Created\n\n"<<endl;
    string command="";
    fstream chunck_data;
    int* ptr_fd = (int *) params;
    int fd = *ptr_fd;
    char buff[BUFF_SIZE]={0};
    int valread;
    //cout<<"\n\nReading from peer client with fd = "<<fd<<"\n\n";
    while(valread = read( fd , buff, BUFF_SIZE))
    {
        //cout<<"\n\nReading from peer client\n\n";
        if(valread<0)
        {
            cout<<"Read Error"<<endl;
        }
        //cout<<"\n\nReading DONE from peer client\n\n";
        command=buff;
        //cout<<"Command Received is:"<<command<<endl;
        if(command.compare(0,5,"which")==0)
        {
            stringstream path_find(command);
            string file_path;
            getline(path_find,file_path,' ');
            getline(path_find,file_path,' ');
            send_pieces(file_path,fd,chunck_data);
        }
        else if(command.compare(0,4,"send")==0)
        {
            stringstream find_data(command);
            string temp,path;
            int piece_no_to_send;
            getline(find_data,temp,' ');
            getline(find_data,temp,' ');
            path = temp;
            getline(find_data,temp,' ');
            piece_no_to_send = stoi(temp);
            //cout<<"Sending chunck data"<<endl;
            send_chunck_no(fd , path, piece_no_to_send);
        }
        else if(command.compare(0,4,"exit")==0)
        {
            cout<<"Exiting server_child"<<endl;
            break;
        }
        memset(&buff[0], 0, sizeof(buff));
        command.clear();
    }
    close(fd);
    //free(params);
}

void *server_func(void* params)
{
    int accept_stat;
    socklen_t new_track_size;
    parameter *p=(parameter *)params;
    //cout<<"Server Created"<<endl;
    //cout<<"Server IP: "<<p->ip<<" Port:"<<p->port<<endl;
    struct sockaddr_in server;
    struct sockaddr_in new_track;
    int sock_fd;
    sock_fd=set_socket(server,1,p->port);
    if(listen(sock_fd,SOMAXCONN)<0) //SOMAXCONN is max connections to queue
    {
        cout<<"Listening Error"<<endl;
    }
    while((accept_stat=accept(sock_fd, (struct sockaddr*)&new_track,&new_track_size)) )
    {
        new_track_size=sizeof(new_track);
        if(accept_stat<0)
        {
            cout<<"Problem in Accepting connection error no="<<errno<<endl;
        }
        pthread_t child;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
        child_thread_fds = (int *)malloc(sizeof(int));
        *child_thread_fds = accept_stat;
        //cout<<"New Server Child Created with FD - "<<accept_stat<<endl;
        int client_fd = pthread_create(&child,&attr,server_child,(void *)(child_thread_fds));
        if(client_fd)
        {
            cout<<"Error in Creating Client Thread"<<endl;
        }
        if(pthread_detach(child)!=0)
        {
            cout<<"Problem in Detach"<<endl;
        }
    }
}

int main(int argc, char *argv[])
{
    if(argc!=3)
    {
        cout<<"Enter Proper Arguments"<<endl;
        exit(0);
    }
    string ip_server;
    int ip_port,dump;
    void* status;
    int server_fd,client_fd;
    parameter client_p[3],server_p;
    ip_server=strtok(argv[1],":");
    ip_port=stoi(strtok(NULL,":"));
    cout<<"IP Port Comb:"<<ip_server<<" Port="<<ip_port<<endl;
    pthread_t server,client;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
    fstream tracker_detail(argv[2]);
    tracker_detail>>dump;
    tracker_detail>>client_p[0].ip;	//0 contains tracker details and 1 contains self and 2 contains peers server part addr
    tracker_detail>>client_p[0].port;
    tracker_detail>>dump;
    tracker_detail>>client_p[1].ip;
    tracker_detail>>client_p[1].port;
    client_p[2].ip = ip_server;
    client_p[2].port = ip_port;
    client_fd = pthread_create(&client,&attr,client_func,(void *)&client_p);
    if(client_fd)
    {
        cout<<"Error in Creating Client Thread"<<endl;
    }
    server_p.port=ip_port;
    server_p.ip=ip_server;
    server_fd = pthread_create(&server,&attr,server_func,(void *)&server_p);
    if(server_fd)
    {
        cout<<"Error in creating Server Thread"<<endl;
    }
    pthread_join(client,&status) ;
    close(server_fd);
    //pthread_join(server,&status) ;
    return 0;
}
