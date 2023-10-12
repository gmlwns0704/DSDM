#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>

#include<iostream>
#include<queue>
#include<algorithm>

#include"communicate.h"

using namespace std;

class scManage{
    private:
    int srvFd; //서버 바인드 소켓
    int srvPort; //서버 바인드 포트
    int parentFd; //부모노드 소켓
    deque<int> childFds; //자식소켓 리스트

    int newSrv(int port); //새로운 서버 소켓 생성
    int newClnt(const char* ip, int port); //해당 ip로 connect하는 새로운 소켓 생성

    public:
    scManage(int inputSrvPort);

    int connectParent(const char* ip, int port); //부모 노드에게 연결
    int startAccept(); //child노드 accept시작
    int broadcast(const char* msg, int size, deque<int> fds); //해당 소켓 리스트에게 메시지 뿌리기
    int spreadMsg(const char* msg, int size, int srcFd); //특정 fd로부터 온 메시지를 적절하게 전파
    
};