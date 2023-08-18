# include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fstream>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <algorithm>
//can I do this though??
#include <stdlib.h>
#include <signal.h>
#include <vector>
#include <glob.h>
#include <mutex>//mutex for critical sections ::
#include <stdio.h>
#include <openssl/md5.h>

#define SIZE_FILE 1000


std::mutex mtx;

//#pragma warning(disable:4996)
//this file establishes connectoins with immediate hosts
using namespace std;

//info about a particular host/server::
int num_neigh,num_files;
int *neigh_id,*port_num;
char **files;
int my_id,pr_port,pblc_port;
string dir_str;//we are maintaing client_dir as glob var::


vector <string> my_files;

bool is_dir(string path){
    struct stat s;
    if(stat((dir_str+path).c_str(),&s)==0){
      if(s.st_mode & S_IFDIR){
          

          return true;
      }
    }
    return false;
}

string sel(string tmp,string pattern){
    string ans="";
    for(int i=pattern.length()-1;i<tmp.length();i++){
        ans+=tmp[i];
    }
    return ans;
}



//data structure to represent search table corresponding to A::
//depth 1 is preferred over 2::
//file-name:char* , next_node_id:int , depth:int
//converts char* to string
string cvt_str(char* c){
    //neglecting the case ofempty char::
    int curr=0;
    string ans="";
    while(c[curr]!='\0'){
       ans+=c[curr];
       curr++;
    }
    return ans;
}

int len(char *c){
    int curr=0;
    while(c[curr]!='\0'){
        curr++;
        
    }
    return curr;
}
vector<string> globVector(const string& pattern){
    glob_t glob_result;
    glob(pattern.c_str(),GLOB_TILDE,NULL,&glob_result);
    vector<string> files;
    for(unsigned int i=0;i<glob_result.gl_pathc;++i){
        string tmp=string(glob_result.gl_pathv[i]);
        
        
        files.push_back(sel(tmp,pattern));
    }
    globfree(&glob_result);
    return files;
}
//converts char to int 

class forward_table{
    
    vector <string> file;
    vector <int>  next_node;
    vector <int>  depth;
    vector <int>  p_port;
    vector <int>  port;
    public:
    int tot_elem;//total elemts in the table
    int sd;//represents max_srch depth that is perfectloy accurate acc to precond.

    forward_table(){
        tot_elem=0;
        //initilise it with the file you need::
        for(auto i=my_files.begin();i<my_files.end();++i){
            if((*i).length()!=0){

                file.push_back(*i);
                next_node.push_back(my_id);
                depth.push_back(0);
                p_port.push_back(pr_port);
                port.push_back(pblc_port);
                tot_elem++;
            }
        }


    }
    //updates on taking the suitable values::
    //cap captures uperlimit on file distance I can achive::
    int nex_prt(char* c){
         int elem_it=0;
         for(auto i=file.begin();i!=file.end();++i){
            
            char tmp_char[(*i).length()+1];
            strcpy(tmp_char,(*i).c_str());
            if(strcmp(tmp_char,c)==0){
                return port.at(elem_it);


            }
            elem_it++;
         }
       //not found::
         return -1;
    }
    void update(char* c, int nn, int d,int prvt_port,int prt,int cap){
       if(d+1>cap){
           return;
       }
       int elem_it=0;
       for(auto i=file.begin();i!=file.end();++i){
            
            char tmp_char[(*i).length()+1];
            strcpy(tmp_char,(*i).c_str());
            if(strcmp(tmp_char,c)==0){
                if(depth[elem_it]>d+1){
                    next_node.insert(next_node.begin()+elem_it,nn);
                    depth.insert(depth.begin()+elem_it,d+1);
                    p_port.insert(p_port.begin()+elem_it,prvt_port);
                    port.insert(port.begin()+elem_it,prt);
                }
                if(depth[elem_it]==d+1 && prvt_port<next_pvt(tmp_char)){
                    next_node.insert(next_node.begin()+elem_it,nn);
                    depth.insert(depth.begin()+elem_it,d+1);
                    p_port.insert(p_port.begin()+elem_it,prvt_port);
                    port.insert(port.begin()+elem_it,prt);
                }
                return;


            }
            elem_it++;
       }
       string s=cvt_str(c);
       file.push_back(s);//push the file inside::

       next_node.push_back(nn);
       depth.push_back(d+1);
       p_port.push_back(prvt_port);
       port.push_back(prt);
       tot_elem++;
    }
    //returns node_num of the next node needed::
    int next(char* c){
       int elem_it=0;
       for(auto i=file.begin();i!=file.end();++i){
            
            char tmp_char[(*i).length()+1];
            strcpy(tmp_char,(*i).c_str());
            if(strcmp(tmp_char,c)==0){
                return next_node.at(elem_it);


            }
            elem_it++;
       }
       //not found::
       return -1;
    }
    int next_pvt(char *c){
         int elem_it=0;
       for(auto i=file.begin();i!=file.end();++i){
            
            char tmp_char[(*i).length()+1];
            strcpy(tmp_char,(*i).c_str());
            if(strcmp(tmp_char,c)==0){
                return p_port.at(elem_it);


            }
            elem_it++;
       }
       //not found::
       return -1;
    }
    //min depth we nee to go to reach a file::
    int dep(char* c){
       int elem_it=0;
       for(auto i=file.begin();i!=file.end();++i){
            
            char tmp_char[(*i).length()+1];
            strcpy(tmp_char,(*i).c_str());
            if(strcmp(tmp_char,c)==0){
                return depth.at(elem_it);


            }
            elem_it++;
       }
       //not found::
       return -1;
    }
    //process c for update function:: update will be called from here::
    void process(char* c,int nb,int cap){
       int curr=0;

       char* handler;
       int nn,d,prvt_port,prt;

       while(curr!=nb){
            
            for(int j=0;j<5;j++){
                
                if(j==0){
                    string temp_str="";
                    
                    while((c[curr]-' ')!=0 && (c[curr]-'\0')!=0){
                        //cout<<c[curr];
                        //cout<<c[curr]-' '<<endl;
                        temp_str+=c[curr];
                        curr++;
                        if(curr==nb){
                            break;
                        }
                    }
                    
                    handler=new char[temp_str.length()+1];
                    strcpy(handler,temp_str.c_str());
                    
                }
                else if(j==1){
                    string temp_str="";
                    while(c[curr]!=' ' && c[curr]!='\0' && c[curr]<='9' && c[curr]>='0'){
                        temp_str+=c[curr];
                        curr++;
                    }
                    
                    nn=stoi(temp_str);
                    
                    if(curr==nb){
                            break;
                    }
                }
                else if(j==2){
                    string temp_str="";
                    while(c[curr]!=' ' && c[curr]!='\0' && c[curr]<='9' && c[curr]>='0' && c[curr]!='\n'){
                        temp_str+=c[curr];
                        curr++;
                    }
                    d=stoi(temp_str);
                    if(curr==nb){
                            break;
                    }
                }
                else if(j==3){
                    string temp_str="";
                    while(c[curr]!=' ' && c[curr]!='\0' && c[curr]<='9' && c[curr]>='0' && c[curr]!='\n'){
                        temp_str+=c[curr];
                        curr++;
                    }
                    prvt_port=stoi(temp_str);
                    if(curr==nb){
                            break;
                    }
                }
                else{
                 string temp_str="";
                    while(c[curr]!=' ' && c[curr]!='\0' && c[curr]<='9' && c[curr]>='0' && c[curr]!='\n'){
                        temp_str+=c[curr];
                        curr++;
                    }
                    prt=stoi(temp_str);
                    if(curr==nb){
                            break;
                    }
            }
                curr++;
            }
           
            update(handler,nn,d,prvt_port,prt,cap);

            delete[] handler;
       }

    }
    //encodes table as suitable char::
    string as_char(){
        string s="";
        for(int i=0;i<tot_elem;i++){
           s+=file.at(i);
           s+=' ';
           s+=to_string(next_node.at(i));
           s+=' ';
           s+=to_string(depth.at(i));
           s+=' ';
           s+=to_string(p_port.at(i));
           s+=' ';
           s+=to_string(port.at(i));
           s+='\n';
        }
        
        return s;
    }


};

forward_table ft;
forward_table temp_ft;

int char_to_int(char* data){
    int curr=0;
    int curr_pos=0;
    //assuming null term chars
    while(data[curr_pos]!='\0'){
        curr=10*curr+data[curr_pos]-'0';
        curr_pos++;
    }
    //indicating empty text::
    if(curr_pos==0){
        return -1;
    }
    return curr;
}
void sigchld_handler(int s){
    while(wait(NULL)>0);
}
//client functionality::
void request_client(int id,int sockfd){
    string s=to_string(id);
    char* msg_chr=new char[s.length()+1];
    strcpy(msg_chr,s.c_str());
    if (send(sockfd,msg_chr,s.length(),0)==-1){
        perror("send");
    }
   




}
int id=0;
void client(){
    //int socket_arr[num_neigh];
    
    bool conn[num_neigh];
    for(int i=0;i<num_neigh;i++){
        conn[i]=0;
    }
    while(true){
        for(int i=0;i<num_neigh;i++){
            if(conn[i]==1){
                continue;
            }
            
            int sockfd,numbytes=0;
            char* buf1=new char[400];
            struct sockaddr_in dest_addr;

            sockfd=socket(AF_INET,SOCK_STREAM,0);

            dest_addr.sin_family=AF_INET;//HOST ORDER
            dest_addr.sin_port=htons(port_num[i]);
            dest_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
            memset(&(dest_addr.sin_zero),'\0',8);
            int x_count=0;
            if(connect(sockfd,(struct sockaddr*)&dest_addr,sizeof(struct sockaddr))<0){
                //perror("conection");
                //cout<<i<<endl;
                x_count++;
                //exit(1);
                close(sockfd);
                continue;
            }
            std::thread t1(request_client,id,sockfd);
            t1.join();
            
            //cout<<i<<":"<<x_count<<endl;
            string ans="";
            int numbytes1;
            if((numbytes1=recv(sockfd,buf1,399,0))<=0){
                close(sockfd);
                continue;

            }
            conn[i]=1;
            int fs=stoi(cvt_str(buf1));
            send(sockfd,"ack",3,0);
            while(fs!=0){
               if((numbytes1=recv(sockfd,buf1,399,0))==-1){
                continue;

               } 
               buf1[numbytes1]='\0';
               fs-=numbytes1;

               ans+=buf1;
               bzero(buf1,400);
               numbytes+=numbytes1;
            }
            
            
            //cout<<numbytes<<endl;
            if(numbytes==0){
                conn[i]=0;
                delete[] buf1;
                close(sockfd);
                continue;
            }
            if(numbytes==1){
               conn[i]=0;
               delete[] buf1;
               close(sockfd);
               continue;
            }
            char *buf=new char[ans.length()+1];
            strcpy(buf,ans.c_str());

            buf[ans.length()]='\0';
            
            //cout<<"Connected to "<<neigh_id[i]<<" with unique-ID "<<buf<<" on port "<<port_num[i]<<endl;
            //updates corresponding entries in depth one:: 
          
            temp_ft.process(buf,ans.length(),1);
            
            
            delete[] buf;
            //breaking loop if all conn are made
           

            close(sockfd);
            //break;
        }
        bool v=false;
        for(int j=0;j<num_neigh;j++){
                if(conn[j]==0){
                    //close(sockfd);
                    //cout<<j<<endl;
                    v=true;
                }
        }
        if(v){
            
            continue;
        }
        mtx.lock();
        ft=temp_ft;
        id++;
        mtx.unlock();
        //cout<<"breaking loop"<<endl;
        if(id==1){
           //this is where we start our work:: we request files to it::
           //we are going to use temp_ft which is same as ft insted of using loosing mutex lock::

           for(int i=0;i<num_files;i++){
               int nxt=temp_ft.next(files[i]);
               
               if(nxt!=-1){
                   if(temp_ft.dep(files[i])==1){
                        int sockfd,num1ytes;
                        char* buf=new char[SIZE_FILE];
                        struct sockaddr_in dest_addr;

                        sockfd=socket(AF_INET,SOCK_STREAM,0);

                        dest_addr.sin_family=AF_INET;//HOST ORDER
                        //host port??
                        
                        dest_addr.sin_port=htons(temp_ft.nex_prt(files[i]));
                          
                        dest_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
                        memset(&(dest_addr.sin_zero),'\0',8);
                        int x_count=0;
                        
                        if(connect(sockfd,(struct sockaddr*)&dest_addr,sizeof(struct sockaddr))<0){
                            //perror("conection");
                            //cout<<i<<endl;
                            x_count++;
                            //exit(1);
                            close(sockfd);
                            continue;
                        }
                        
                        
                        
                        
                        string s=cvt_str(files[i]);
                        char* msg_chr=new char[s.length()+1];
                        strcpy(msg_chr,s.c_str());
                        if (send(sockfd,msg_chr,s.length(),0)==-1){
                            perror("send");
                        }
                        //cout<<"send for: "<<msg_chr<<endl;

                        //asked for msg::
                       
                        ofstream save_file((dir_str+"Downloaded/"+s),std::ofstream::binary);
                        
                        
                        //bool eod=false;.c_str
                        
                        int numbytes1;
                        char buf1[100];
                        while(numbytes1<=0){
                        numbytes1=recv(sockfd,buf1,99,0);
                        }
                        buf1[numbytes1]='\0';
                        
                        int f_s=stoi(cvt_str(buf1));
                       
                        if (send(sockfd,"ack",3,0)==-1){
                            perror("send");
                        }
                        
                        while(f_s>0){
               
                           int numbytes=recv(sockfd,buf,SIZE_FILE,0);
                           if(numbytes==-1){
                              break;
                           }
                           if(numbytes==0){
                               continue;
                           }
                           f_s-=(numbytes);
                           buf[numbytes]='\0';
                          
                           
                           save_file.write(buf,numbytes);
                           bzero(buf,SIZE_FILE);
                            

                        }
                        save_file.flush();
                        save_file.close();
                        close(sockfd);

                        

                        //we have sent the request:: waiting to receive the file::




                   }
               }

           }


           break;
        }
        for(int i=0;i<num_neigh;i++){
           conn[i]=0;
        }
    }
}
//server functionlaity::
void approve_client(int sockfd,int *decision,string *clien_rep){
    int numbytes;
    char buf[100];
    //cout<<'a'<<endl;
    while((numbytes=recv(sockfd,buf,99,0))==-1){
                perror("recv");
                continue;
    }

    
    
    if(numbytes==1){
        buf[1]='\0';
        int temp=buf[0]-'0';
        mtx.lock();
        //cout<<temp<<" "<<id<<endl;
        if(temp>id){
            *decision=0;
            mtx.unlock();
            return;
        }
        mtx.unlock();
    }
    
    if(numbytes>1){
        buf[numbytes]='\0';
        *clien_rep=cvt_str(buf);
        *decision=2;
    }
}
void send_file(int sockfd,string file){
   FILE* fp1;
   //char file_size[100];
   int file_size=0;
   char data[SIZE_FILE]={0};
   fp1=fopen((dir_str+file).c_str(),"r");
   while(fgets(data,SIZE_FILE,fp1)!=NULL){
       file_size+=len(data);
       bzero(data,SIZE_FILE);
   }
   string f_s=to_string(file_size);
  
   
   if(send(sockfd,f_s.c_str(),f_s.length(),0)==-1){
           perror("send");
           exit(1);
   }
   int numbytes;
   char buf[100];
   while((numbytes=recv(sockfd,buf,99,0))==-1){
                perror("recv");
                continue;
   }


   FILE* fp;
   fp=fopen((dir_str+file).c_str(),"r");
   
   size_t offset=0;
   int sent_bytes;
   while((( sent_bytes=sendfile(sock_to_neighbours[i],fd, &offset,1024))>0) && (numbytes>0)){
						num_bytes-=sent_bytes;
						//cout<<"sending side : "<<remain_data<<endl;
   }
}
void server(){
      int sockfd,new_fd;
      struct sockaddr_in my_addr;
      struct sockaddr_in their_addr;//connecters'info 
      socklen_t sin_size;
      struct sigaction sa;
      int yes=1;

      if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
          perror("socket");
          exit(1);
      }

      if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1){
          perror("setsocket");
          exit(1);
      }

      my_addr.sin_family=AF_INET;
      my_addr.sin_port=htons(pblc_port);

      my_addr.sin_addr.s_addr=INADDR_ANY;//my ip

      bzero(&(my_addr.sin_zero),8);

      if(::bind(sockfd, (struct sockaddr *)&my_addr,sizeof(struct sockaddr))==-1){
          perror("bind");
          exit(1);
      }

      if(listen(sockfd,10)==-1){
          perror("listen");
          exit(1);
      }

      sa.sa_handler=sigchld_handler;//REAP ALL DEAD PROCESS
      sigemptyset(&sa.sa_mask);
      sa.sa_flags=SA_RESTART;

      if(sigaction(SIGCHLD,&sa,NULL)==-1){
          perror("sigaction");
          exit(1);
      }
      int y_count=0;
      while(true){
          y_count++;
          //    cout<<y_count<<endl;
          sin_size=sizeof(struct sockaddr_in);
          if((new_fd=accept(sockfd,(struct sockaddr *)&their_addr,&sin_size))==-1){
              //perror("accpet");
              continue;
          }
          //cout<<"Connection accepted with "<<ntohs(their_addr.sin_port)<<endl;

          if(!fork()){
              close(sockfd);
              string msg=ft.as_char();
              //cout<<"msg:"<<msg<<endl;
              char msg_chr[msg.length()+1];
              strcpy(msg_chr,msg.c_str());

              //sprintf(msg,"%d",pr_port);
              
              int decision=1;
              
              //*decision=0;
              //cout<<"what the hll"<<endl;
              //int id=0;
              string clien_rep;
              std::thread t2(approve_client,new_fd,&decision,&clien_rep);
              t2.join();
              if(decision==0){
                 msg="1";
              }
              if(decision==2){
                
                 send_file(new_fd,clien_rep);
                 close(new_fd);
                 
              }
              //cout<<msg_chr<<":"<<msg<<endl;
              //cout<<"+++++"<<endl;
              else {
                char buf[4];
              send(new_fd,(to_string(msg.length())).c_str(),(to_string(msg.length())).length(),0);
              while(recv(new_fd,buf,3,0)==-1){
                  continue;
              }
              
              if (send(new_fd,msg_chr,msg.length(),0)==-1){
                  
                  perror("send");
              }
              close(new_fd);
              }
              
          }
          close(new_fd);
          //cout<<y_count<<endl;
      }
      //server should run infinitely::
      cout<<"server ends??"<<endl;

}

void sort_char(char** c,int n){
     for(int i=0;i<n;i++){
         for(int j=0;j<n-1;j++){
             if(strcmp(files[j],files[j+1])>0){
                 swap(files[j],files[j+1]);
             }
         }
     }
}

void sort_id(int* id,int* neigh,int num_neigh){
    for(int i=0;i<num_neigh;i++){
        for(int j=0;j<num_neigh-1;j++){
            if(id[j]>id[j+1]){
                swap(id[j],id[j+1]);
                swap(neigh[j],neigh[j+1]);
            }
        }
    }
}


int main(int argv,char * argc[]){
    if(argv!=3){
        cout<<"not enough args"<<endl;
        exit(0);
    }

    //reading config file name and dir
    char* config_file=argc[1];
    char* dir=argc[2];
    dir_str=cvt_str(dir);

    mkdir((dir_str+"/Downloaded").c_str(),0777);
    my_files = globVector(dir_str+"/*");
    remove_if(my_files.begin(),my_files.end(),is_dir);
    
    //opening the file
    ifstream conf;
    conf.open(config_file);
    char data[100];
    int word_num=0;
    //reading the first line

    //data to be remembered::
    


    //end of config data_points
    int curr_pos=0;
    while(conf>>data){
        //cout<<data<<endl;
        
        int elem=char_to_int(data);
        if(word_num==0){
            my_id=elem;
        }
        else if(word_num==1){
            pblc_port=elem;
        }
        else if(word_num==2){
            pr_port=elem;
        }
        else if(word_num==3){
            num_neigh=elem;
            neigh_id=new int[num_neigh];
            port_num=new int[num_neigh];
        }
        //int curr_pos=0;
        else if(word_num<=3+2*num_neigh){
            if(word_num%2==0){
                neigh_id[curr_pos]=elem;
            
            }
            else{
                port_num[curr_pos]=elem;
                curr_pos++;
            }
        }
        else if(word_num==4+2*num_neigh){
            curr_pos=0;
            num_files=elem;
            files=new char*[num_files];
        }
        else if(word_num<=4+2*num_neigh+num_files){
            //cout<<data<<endl;
            files[curr_pos]=new char[100];
            strcpy(files[curr_pos],data);
            //cout<<"he"<<files[curr_pos]<<curr_pos<<endl;
            curr_pos++;
        }


        //cout<<elem<<endl;

     
        //elem now has required informtio
        word_num++;
       
    }
    //print files i have 


    //end printing::
    //cout<<"1"<<endl;
    ft=forward_table();
    temp_ft=forward_table();
    sort_id(neigh_id,port_num,num_neigh);
    
    std::thread t_server(server);
    std::thread t_client(client);
    
    t_client.join();
    sort_char(files,num_files);
    
    //sort(my_files.begin(),my_files.end());
    for(int i =0;i<num_files;i++){
        
        int nxt=ft.next(files[i]);
        int dpt=ft.dep(files[i]);

        if(nxt==-1){
            cout<<"Found "<<files[i]<<" at 0 with MD5 0 at depth 0"<<endl;
        }
        else{
            if(dpt!=0){
               
               FILE* t_n=popen(("md5sum "+dir_str+"Downloaded/"+cvt_str(files[i])).c_str(),"r");
               
               char hash[300];
               
               
               fgets(hash,33,t_n);
               hash[33]='\0';
               
               
               
              

               cout<<"Found "<<files[i]<<" at "<<ft.next_pvt(files[i])<<" with MD5 "<<hash<<" at depth "<<dpt<<endl;
               fclose(t_n);
            }
            else{
               cout<<"Found "<<files[i]<<" at 0 with MD5 0 at depth 0"<<endl;
            }
        }

    }

    t_server.join();

    
    


    
}