#include"node.h"
#include"communicate.h"

int node::handleReqLock(time_t timeStamp, const request* req){
    //다른 노드 어딘가에 우선순위가 높은 reqLock이 발생했다면
    //현재 노드가 locked상태가 되는 것은 불가능하다.
    //우선순위가 높은 reqLock을 가진 노드에서 현재 lock된 req에 대해
    //allow해줄 수 없기때문이다.
    
    //psuh이전 최우선순위 req
    reqTableNode* oldTop = reqTable.at(req->id).top().second;
    //reqTable에 추가될 노드 생성
    reqTableNode* newReq = malloc(sizeof(reqTableNode));
    newReq->fd = req->fd;
    newReq->sent = 0;

    //reqTable에 새 노드 추가
    reqTable.at(req->id).push({timeStamp, newReq});
    //해당 id의 락 상태
    int state = lockTable.at(req->id).state;

    //state==unLocked, 대기중인 req조차 없음
    if(state == unLocked){
        //state을 변경하고 reqLock메시지 전파
        lockTable.at(req->id).state = waitFor;
        newReq->sent = 1;
        lockTable.at(req->id).allowCnt = reqLock(req->id, req->fd, timeStamp);
    }
    //대기중인 req가 있지만 새로운 req가 최우선순위
    else if(state == waitFor && reqTable.at(req->id).top().second == newReq){
        //기존 req의 allow를 새로운 req가 대신 받음
        //reqLock메시지 전파
        newReq->sent = 1;
        //기존 req의 sent false, 이 req는 향후 다시 전송되어야함
        oldTop->sent = 0;
        //기존 req에게는 reqLock이 안갔으므로 reqLock전송
        reqLock_target(req->id, oldTop->fd, timeStamp);
    }

    return 0;
}

int node::handleAllowLock(const request* req){
    if(lockTable.at(req->id).state == unLocked){ //state == unLock, 아무런 req가 없는데 allow가 도착함
        fprintf(stderr, "E: got allowLock but no req\n");
        return -1;
    }

    if(lockTable.at(req->id).allowCnt > 0){ //해당 id는 기다리던 allow가 있음
        //allowCnt 업데이트
        lockTable.at(req->id).allowCnt -= 1;
    }

    if(lockTable.at(req->id).allowCnt == 0){ //모든 allow 수신됨
        //최우선 req
        reqTableNode* topReq = reqTable.at(req->id).top().second;
        if(topReq->fd == -1){ //본인이 요청한 request가 allow됨
            /*해당 노드에서 lock 취득*/
            //해당 id의 데이터에 lock
            lockTable.at(req->id).state = locked;
        }
        else{
            //해당 req를 전송한 노드에게 allow
            allowLock_target(req->id, topReq->fd);
        }
        //해당 req 제거
        free(topReq);
        reqTable.at(req->id).pop();
    }

    return 0;
}

int node::handleNotifyUnlock(const request* req){
    if(lockTable.at(req->id).state != locked){ //state이 locked가 아닌데 unlock이 왔음, 오류
        fprintf(stderr, "E: got otifyUnlock but not locked\n");
        return -1;
    }

    if(!isRoot && scm.childFds.size()){ // not root and not leaf
        notifyUnlock(req->id, req->fd); //notifyUnlock전파
    }

    //reqTable, lockTable수정
    if(!(reqTable.at(req->id).size())){ //대기중이던 reqLock이 없음
        lockTable.at(req->id).state = unLocked;
    }
    else{ //대기중이던 reqLock 존재
        reqTableNode *nextReq = reqTable.at(req->id).top; //다음 reqLock 받아오기
        reqTable.at(req->id).pop(); //reqTable pop
        lockTable.at(req->id).state = waitFor; //상태를 waitFor로 변경
        reqLock(req->id, nextReq->fd); //다음 reqLock전파
        free(nextReq);
    }

    return 0;
}

/*private*/
int node::handleReq(){
    //cpp의 priority queue에서 pop은 데이터를 리턴하지 않음
    time_t timeStamp = reqQ.top().first; //타임스탬프
    request* req = reqQ.top().second; //최우선 request
    reqQ.pop();

    switch(req.type){
        case t_reqLock: handleReqLock(timeStamp, (const request*)req); break;
        case t_allowLock: handleAllowLock((const request*)req); break;
        case t_notifyUnlock: handleNotifyUnlock((const request*)req); break;
    }

    //마무리
    return 0;
}

/*public*/
node::node(const char* inputParentIp, int parentPort, int myPort){
    this->scm=new scManage(myPort); //소켓매니저 인스턴스 생성
    this->isRoot=(!inputParentIp); //루트노드 여부 결정(inputParentIp가 NULL이라면 루트노드)
    if(!(this->isRoot)){//루트가 아니라면 부모노드에 연결
        this->scm.connectParent(inputParentIp, parentPort);
    }
}

int node::handleMsg(msgType type, const char* msg, int srcFd){
    request req;
    time_t timeStamp;
    //타입에 영향받지 않는 request값 설정
    req.fd = srcFd;
    req.type = type;

    // 타입에 따른 request값 설정
    switch(type){
        // msg값 읽기
        case t_reqLock:{
            req.id = *((msgReqLock*)msg)->id;
            timeStamp = *((msgReqLock*)msg)->t;
        }
        case t_allowLock:{
            req.id = *((msgAllowLock*)msg)->id;
        }
        break;
        case t_notifyUnlock:{
            req.id = *((msgNotifyUnlock*)msg)->id;
        }
        default:
        break;
    }

    // 해당 request를 타임스탬프와 함께 저장
    reqQ.push({timeStamp, req});
    return 0;
}

int node::reqLock_broad(u_int id, int exceptSocket, time_t timeStamp){
    int size = sizeof(msgHeader) + sizeof(msgReqLock); //패킷의 총 크기 헤더+보디
    char msg[sizeof(msgHeader) + sizeof(msgReqLock)]; //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_reqLock; //메시지 타입 설정
    
    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgReqLock*)msgPtr)->t = timeStamp;
    ((msgReqLock*)msgPtr)->id = id;

    // 메시지 뿌리기
    return scm.spreadMsg(msg, size, exceptSocket);
}

int node::reqLock_target(u_int id, int socket, time_t timeStamp){
    int size = sizeof(msgHeader) + sizeof(msgReqLock); //패킷의 총 크기 헤더+보디
    char msg[sizeof(msgHeader) + sizeof(msgReqLock)]; //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_reqLock; //메시지 타입 설정
    
    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgReqLock*)msgPtr)->t = timeStamp;
    ((msgReqLock*)msgPtr)->id = id;

    // 메시지 뿌리기
    return write(socket, msg, size);
}

int node::allowLock_target(u_int id, int socket){
    int size = sizeof(msgHeader) + sizeof(msgAllowLock); //패킷의 총 크기 헤더+보디
    char msg[sizeof(msgHeader) + sizeof(msgAllowLock)]; //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_allowLock; //메시지 타입 설정

    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgAllowLock*)msgPtr)->id = id;

    // 메시지 전송
    return write(socket, msg, size);
}

int node::notifyUnlock_broad(u_int id, int exceptSocket){
    int size = sizeof(msgHeader) + sizeof(msgNotifyUnlock); //패킷의 총 크기 헤더+보디
    char msg[sizeof(msgHeader) + sizeof(msgNotifyUnlock)]; //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_notifyUnlock; //메시지 타입 설정
    
    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgNotifyUnlock*)msgPtr)->id = id;

    // 메시지 뿌리기
    return scm.spreadMsg(msg, size, exceptSocket);
}