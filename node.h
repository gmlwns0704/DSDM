#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>

#include<string>
#include<iostream>
#include<queue>

#include"scManage.h"
#include"communicate.h"
#include"shareData.h"

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
    //reqLock을 보냈던 fd, -1이라면 이 노드가 근원지
    int fd;
    //해당 req에게 top을 빼았긴 이전 req의 timeStamp
    time_t stolenTimeStamp;
    //기존 req의 자리를 빼았았다면 1
    //해당 값이 1이라면 이 req는 자신에 대한 allow가 아닌 다른 req에 대한 allow를 처리해야함
    bool steal;
} reqTableNode;

typedef enum state{
    unLocked = 0, //lock 없음, 대기중인 allowLock도 없음
    waitFor, //allowLock을 대기중
    locked, //locked
    empty //할당된 데이터 없음
} state;

typedef struct lockTableNode{ //(LTN)
    state state; //lock여부
    // int removeTHIS; //lock메시지를 보냈던 fd
    int allowCnt; //lock이 성립되기 위한 allow의 필요 갯수
    pthread_mutex_t* mtx; //이 노드에서 해당 데이터의 lock을 관리하기 위한 노드
} lockTableNode;

class node{
    private:
    bool isRoot; //root노드 여부(부모없음)
    scManage* scm; //소켓 관련 기능 종합
    shareData* sdm; //데이터 관리 기능 종합
    //lockReq에 대한 요청 큐, <타임스탬프, request>
    //vector와 greater는 큐를 오름차순으로 정렬하기 위함
    priority_queue<pair<time_t, request*>, vector<pair<time_t, request*>>, cmpOnlyFirst> reqQ;
    //데이터에 대한 lockTable, Data_i의 reqLock정보는 reqTable[i]
    //향후 hash table로 변경할 것 고려
    deque<priority_queue<pair<time_t, reqTableNode*>, vector<pair<time_t, reqTableNode*>>, cmpOnlyFirst>> reqTable;
    //데이터에 대한 lockTable, Data_i의 lock정보는 lockTable[i]
    deque<lockTableNode*> lockTable;

    int discountAllowCnt(int id); //allowCnt를 1 감소시키고 결과값에 따른 적절한 대응까지
    
    int handleReq(); //reqQ에서 다음 req를 처리
    int handleReqLock(time_t timeStamp, const request* req); //reqLock req에 대해서 적절한 대응
    int handleAllowLock(const request* req); //allowLock req에 대해서 적절한 대응
    int handleMsg(msgType type, const char* msg, int srcFd); //다른 노드로부터 온 메시지를 requset구조체로 변경하여 enque
    
    int reqLock_broad(u_int id, int exceptSocket, time_t timeStamp); //주어진 소켓을 제외하고 lock요청, exceptSocket = -1 이라면 broadcast
    int reqLock_target(u_int id, int socket, time_t timeStamp); //reqLock을 특정 소켓에게만
    int allowLock_target(u_int id, int socket); //주어진 소켓에게 lock허용

    lockTableNode* allocLTN();
    int freeLTN(lockTableNode* node);

    public:
    node(const char* inputParentIp, int parentPort, int myPort);

    int acquireLock(u_int id); //실제 사용자가 lock을 요구
    int releaseLock(u_int id); //얻은 lock을 해제

    int lockData(u_int id);
    int unlockData(u_int id);
    int addData(u_int size);
    int rmData(u_int id);
};