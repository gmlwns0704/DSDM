#include"scManage.h"

using namespace std;

scManage::scManage(int inputSrvPort){
    #ifdef DBG
    fprintf(stderr, "DBG: scm created\n");
    #endif
    this->srvPort=inputSrvPort;
    this->srvFd=newSrv(this->srvPort);
    this->parentFd=0; //부모를 따로 연결하지 않으면 0, root라는 의미
    /*
    startAccept를 별도 스레드로 수행
    */
}

int scManage::connectParent(const char* ip, int port){
    return this->parentFd=newClnt(ip, port);
}

/*새로운 서버 소켓 생성*/
int scManage::newSrv(int port){
    int srvFd;
    struct sockaddr_in srvAddr;

    //소켓 생성
    if((srvFd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        return -1;
    }

    //sockaddr_in 초기화
    memset(&srvAddr, 0, sizeof(srvAddr));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	srvAddr.sin_port = htons(port);

    //bind
    if(bind(srvFd, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0){
        perror("bind");
        close(srvFd);
        return -1;
    }

    //listen
    if(listen(srvFd, 5) < 0){
        perror("listen");
        close(srvFd);
        return -1;
    }

    return srvFd;
}

/*특정 tcp서버에 연결된 클라이언트 소켓 생성*/
int scManage::newClnt(const char* ip, int port){
    int clntFd;
    struct sockaddr_in clntAddr;

    if((clntFd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
        return -1;
    }

    clntAddr.sin_family = AF_INET;
	clntAddr.sin_addr.s_addr = inet_addr(ip);
	clntAddr.sin_port = htons(port);
    if(connect(clntFd, (struct sockaddr*)&clntAddr, sizeof(clntAddr)) < 0){
        perror("connect");
        return -1;
    }

    return clntFd;
}

int scManage::startAccept(){
    struct sockaddr_in clntAddr;
    int clntAddrLen;
    while(1){
        int newClientFd;
        if(newClientFd=accept(srvFd, (struct sockaddr*)&clntAddr, (socklen_t*)&clntAddrLen) < 0){
            perror("accept");
            return -1;
        }
        childFds.push_back(newClientFd); //새로운 클라이언트 소켓을 목록에 추가
    }
}

int scManage::broadcast(const char* msg, int size, deque<int> fds){
    int succNum=0;
    for(int i = 0; i < fds.size(); i++){
        if(write(fds.at(i), msg, size) < 0){
            perror("write");
        }
        else{
            succNum++;
        }
    }
    return succNum;
}

int scManage::spreadMsg(const char* msg, int size, int srcFd){
    int succNum = 0;
    int writeResult;

    if(srcFd == parentFd){ // 부모노드로부터 온 메시지
        return broadcast(msg, size, childFds); //모든 자식노드에게 전파
    }
    else{ //부모노드가 아님
        for(int i = 0; i < childFds.size(); i++){ //srcFd가 아닌 모든 자식노드에게 전파
            if(childFds.at(i) == srcFd)
                continue;
            if((write(childFds.at(i), msg, size) < 0)){
                perror("write");
                return -1;
            }
            succNum++;
        }
        if(parentFd && write(parentFd, msg, size) < 0){ //부모노드 존재 && 부모노드에게 전파
            perror("write");
            return -1;
        }

        return succNum;
    }

    return -1;
}