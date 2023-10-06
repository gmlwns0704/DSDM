#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>

#include<string>
#include<iostream>
#include<queue>
#include"scManage.h"
#include"communicate.h"

using namespace std;

typedef struct request{
    msgType type;
    int fd; //어느 소켓으로부터 온 요청인지
    u_int id; //대상이 되는 데이터 id
} request;

typedef struct cmpOnlyFirst {
    bool operator()(const std::pair<time_t, struct request>& a, const std::pair<time_t, struct request>& b) {
        return a.first > b.first;  // time_t의 오름차순
    }
} cmpOnlyFirst;

typedef struct reqTableNode{
    int fd; //reqLock을 보냈던 fd
} reqTableNode;

typedef enum state{
    unLocked = 0, //lock 없음, 대기중인 allowLock도 없음
    waitFor, //allowLock을 대기중
    locked, //locked
    waitForLock //lock을 요청한 최초의 노드
} state;

typedef struct lockTableNode{
    state state; //lock여부
    int fd; //lock메시지를 보냈던 fd
} lockTableNode;

class node{
    private:
    bool isRoot; //root노드 여부(부모없음)
    scManage scm; //소켓 관련 기능 종합
    priority_queue<pair<time_t, request>, vector<pair<time_t, request>>, cmpOnlyFirst> reqQ; //lockReq에 대한 요청 큐, <타임스탬프, request>, vector와 greater는 큐를 오름차순으로 정렬하기 위함
    deque<priority_queue<pair<time_t, reqTableNode>, vector<pair<time_t, reqTableNode>>, cmpOnlyFirst>> reqTable; //데이터에 대한 lockTable, Data_i의 reqLock정보는 reqTable[i], 향후 hash table로 변경할 것 고려
    deque<lockTableNode> lockTable; //데이터에 대한 lockTable, Data_i의 lock정보는 lockTable[i], 향후 hash table로 변경할 것 고려
    int handleReq();

    public:
    node(const char* inputParentIp, int parentPort, int myPort);
    int handleMsg(msgType type, const char* msg, int srcFd); //다른 노드로부터 온 메시지를 requset구조체로 변경하여 enque
    int reqLock(u_int id, int exceptSocket); //주어진 소켓을 제외하고 lock요청
    int allowLock(u_int id, int socket); //주어진 소켓에게 lock허용
    int notifyUnlock(u_int id, int exceptSocket); //주어진 소켓을 제외하고 unlock통보
};