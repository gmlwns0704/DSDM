#include"node.h"
#include"communicate.h"

/*private*/

int node::discountAllowCnt(int id){
    if(lockTable.at(id)->allowCnt <= 0){ //allowCnt는 이미 0이거나 그보다 작음
        return -1;
    }
    //allowCnt 업데이트
    lockTable.at(id)->allowCnt -= 1;
    
    if(lockTable.at(id)->allowCnt > 0){ //해당 id는 기다리던 allow가 있음
        return lockTable.at(id)->allowCnt;
    }
    else if(lockTable.at(id)->allowCnt == 0){ //모든 allow 수신됨
        //최우선 req
        reqTableNode* topReq = reqTable.at(id).top().second;
        if(topReq->fd == -1){ //본인이 요청한 request가 allow됨
            /*해당 노드에서 lock 취득*/
            //해당 id의 데이터에 lock
            lockTable.at(id)->state = locked;
            //mtx를 unlock해서 사용자가 마침내 데이터에 접근 가능
            pthread_mutex_unlock(lockTable.at(id)->mtx);
        }
        else{
            //해당 req를 전송한 노드에게 allow
            allowLock_target(id, topReq->fd);
        }
        //해당 req 제거
        reqTable.at(id).pop();
        free(topReq);
        return 0;
    }

    return -1; //일반적으로 도달 불가
}

int node::handleReqLock(time_t timeStamp, const request* req){
    //psuh이전 최우선순위 req*, timeStamp
    reqTableNode* oldTop = reqTable.at(req->id).top().second;
    time_t oldTimeStamp = reqTable.at(req->id).top().first;
    //reqTable에 추가될 노드 생성
    reqTableNode* newReq = (reqTableNode*)malloc(sizeof(reqTableNode));
    newReq->fd = req->fd;

    //reqTable에 새 노드 추가
    reqTable.at(req->id).push({timeStamp, newReq});
    //해당 id의 락 상태
    int state = lockTable.at(req->id)->state;

    //다른 노드 어딘가에 우선순위가 높은 reqLock이 발생했다면
    //현재 노드가 locked상태가 되는 것은 불가능하다.
    //우선순위가 높은 reqLock을 가진 노드에서 현재 lock된 req에 대해
    //allow해줄 수 없기때문이다.
    if(state == locked){
        fprintf(stderr, "E: got reqLock but this node is locked\n");
        return -1;
    }
    //state==unLocked, 대기중인 req조차 없음
    else if(state == unLocked){
        //state을 변경하고 reqLock메시지 전파
        lockTable.at(req->id)->state = waitFor;
        //reqLock을 전파받은 노드의 갯수
        int spreadCnt = reqLock_broad(req->id, req->fd, timeStamp);
        if(spreadCnt){
            //allowCnt설정
            lockTable.at(req->id)->allowCnt = spreadCnt;
        }
        //reqLock을 전파받은 노드가 없음
        else{
            //reqLock을 전송한 노드에게 즉시 allow
            allowLock_target(req->id, req->fd);
        }
    }
    else if(state == waitFor){
        //allow를 대기중인 req가 있지만 새로운 req가 최우선순위
        if(reqTable.at(req->id).top().second == newReq){
            //기존 req의 allow를 새로운 req가 대신 받음
            //다른 req의 자리를 빼았았음을 표시
            newReq->steal = 1;
            //top을 빼았긴 req의 timeStamo를 newReq에 저장
            newReq->stolenTimeStamp = oldTimeStamp;
            //기존 req를 전송한 노드에게는 새로운 reqLock이 안갔으므로 reqLock전송
            reqLock_target(req->id, oldTop->fd, timeStamp);
        }
        //현재 topReq는 다른 req의 자리를 빼았은 자리임 && 새로운 req가 top을 빼았긴 req보다 우선순위 높음
        //이경우 oldTop은 현재의 top이기도 함
        //top이 newReq가 아니라는 것으로 현재 top인 req보다 우선순위가 낮다는 것이 보장
        else if(oldTop->steal && oldTop->stolenTimeStamp > timeStamp){
            discountAllowCnt(req->id);
        }
    }

    return 0;
}

int node::handleAllowLock(const request* req){
    if(lockTable.at(req->id)->state == unLocked){ //state == unLock, 아무런 req가 없는데 allow가 도착함
        fprintf(stderr, "E: got allowLock but no req\n");
        return -1;
    }

    discountAllowCnt(req->id);
    
    return 0;
}

int node::handleReq(){
    //cpp의 priority queue에서 pop은 데이터를 리턴하지 않음
    time_t timeStamp = reqQ.top().first; //타임스탬프
    request* req = reqQ.top().second; //최우선 request

    switch(req->type){
        case t_reqLock: handleReqLock(timeStamp, (const request*)req); break;
        case t_allowLock: handleAllowLock((const request*)req); break;
    }

    //마무리
    reqQ.pop();
    free(req);
    return 0;
}

int node::handleMsg(msgType type, const char* msg, int srcFd){
    request* req = (request*)malloc(sizeof(request));
    time_t timeStamp;
    //타입에 영향받지 않는 request값 설정
    req->fd = srcFd;
    req->type = type;

    // 타입에 따른 request값 설정
    switch(type){
        // msg값 읽기
        case t_reqLock:{
            req->id = ((msgReqLock*)msg)->id;
            timeStamp = ((msgReqLock*)msg)->t;
            // 해당 request를 타임스탬프와 함께 저장
            reqQ.push(pair<time_t, request*>(timeStamp, req));
        }
        break;
        case t_allowLock:{
            req->id = ((msgAllowLock*)msg)->id;
            // 해당 request를 타임스탬프와 함께 저장
            reqQ.push({timeStamp, req});
        }
        break;
        case t_update:{
            //값 업데이트
        }
        break;
        default:
            fprintf(stderr, "E: undefined type of message %d\n", type);
        break;
    }
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
    return scm->spreadMsg(msg, size, exceptSocket);
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

lockTableNode* node::allocLTN(){
    lockTableNode* newNode = (lockTableNode*)malloc(sizeof(lockTableNode));
    pthread_mutex_t* mtx = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mtx, NULL);
    newNode->allowCnt = 0;
    newNode->mtx = mtx;
    newNode->state = unLocked;

    return newNode;
}

int node::freeLTN(lockTableNode* node){
    if(pthread_mutex_destroy(node->mtx) < 0)
        return -1;
    free(node->mtx);
    free(node);

    return 0;
}

int node::lockData(u_int id){
    pthread_mutex_t* mtxPtr = lockTable.at(id)->mtx;
    int tryLockResult = pthread_mutex_trylock(mtxPtr);
    if(tryLockResult == 0){
        //이미 mtx가 unlock상태임, 이 노드가 해당 데이터에 lock을 지님
        pthread_mutex_unlock(mtxPtr);
        return -1;
    }
    else if(tryLockResult == EBUSY){
        //이미 mtx가 lock상태임, lockTable이 mtx를 lock하고있음, unlock까지 대기
        pthread_mutex_lock(mtxPtr); //lockTable의 mtx의 unlock대기
        pthread_mutex_unlock(mtxPtr); //lock을 취득하면 일단 다시 unlock, mtx의 unlock상태는 노드가 해당 id데이터를 lock했다는 의미이므로
        return 0;
    }
    else{
        return -1;
    }
}

int node::unlockData(u_int id){
    lockTableNode* targetNode = lockTable.at(id);
    if(targetNode->state != locked){
        fprintf(stderr, "E: %d is not locked\n", id);
        return -1;
    }

    //이미 mtx가 unlock상태임, 이 노드가 해당 데이터에 lock을 지님, 노드 unlock
    if(reqTable.at(id).size() == 0){ //대기중인 다음 req없음
        lockTable.at(id)->state = unLocked;
    }
    else{ //다음 req존재, 해당 req 처리
        int targetFd = reqTable.at(id).top().second->fd;
        time_t timeStamp = reqTable.at(id).top().first;
        lockTable.at(id)->state = waitFor;
        //reqLock을 전파받은 노드의 갯수
        int spreadCnt = reqLock_broad(id, targetFd, timeStamp);
        if(spreadCnt){
            //allowCnt설정
            lockTable.at(id)->allowCnt = spreadCnt;
        }
        //reqLock을 전파받은 노드가 없음
        else{
            //reqLock을 전송한 노드에게 즉시 allow
            allowLock_target(id, targetFd);
            reqTable.at(id).pop();
        }
    }

    // data unlock이후 mtx lock
    pthread_mutex_lock(targetNode->mtx);
    return 0;
}

/*public*/
node::node(const char* inputParentIp, int parentPort, int myPort){
    this->scm = new scManage(myPort); //소켓매니저 인스턴스 생성
    
    this->isRoot=(!inputParentIp); //루트노드 여부 결정(inputParentIp가 NULL이라면 루트노드)
    if(!(this->isRoot)){//루트가 아니라면 부모노드에 연결
        this->scm->connectParent(inputParentIp, parentPort);
    }

    this->sdm = new shareData(); //데이터매니저 인스턴스
    this->lockTableLock = lockTable.at(this->sdm->addData(0)); //lockTable을 위한 lock 설정
}

int node::acquireLock(u_int id){
    if(lockTable.at(id)->state == locked){
        fprintf(stderr, "E: this id %d is already locked\n", id);
        return -1;
    }

    time_t timsStamp = time(NULL); //현재 시간
    reqTableNode* newReq = (reqTableNode*)malloc(sizeof(reqTableNode)); //새로 push될 req
    newReq->fd = -1; //이 노드에서 시작됐음을 알림

    reqTable.at(id).push({timsStamp, newReq}); //reqTable에 push

    //lockTable이 locked state이 되면서 mtx를 unlock할때까지 대기
    return lockData(id);
}

int node::releaseLock(u_int id){
    if(lockTable.at(id)->state != locked){
        fprintf(stderr, "E: this id %d is not locked\n", id);
        return -1;
    }
    return unlockData(id);
}

int node::addData(u_int size){
    int resultId = sdm->addData(size);
    int maxId = sdm->getDataNum();
    if(resultId < 0){
        fprintf(stderr, "E: addData failed\n");
        return -1;
    }
    if(lockTable.size()+1 == maxId){
        //새로운 id의 데이터
        lockTable.push_back(allocLTN());
    }
    else if(lockTable.size() == maxId){
        //maxId가 아닌 id 할당됨resultId의 lockTable에 대하여 검사
        lockTableNode* targetNode = lockTable.at(resultId);
        if(targetNode->state != empty){
            fprintf(stderr, "E: addData failed, %d is not empty id\n", resultId);
            return -1;
        }
        targetNode->state = unLocked;
    }
    return resultId;
}

int node::rmData(u_int id){
    lockTableNode* targetNode = lockTable.at(id);
    //데이터 제거
    if(sdm->rmData(id) < 0){
        fprintf(stderr, "E: rmData failed\n");
        return -1;
    }

    if(id >= lockTable.size()){
        fprintf(stderr, "E: invalid id, (input:%d, number of Id:%d)\n", id, lockTable.size());
        return -1;
    }
    else if(targetNode->state == empty){
        fprintf(stderr, "E: id %d is empty\n", id);
        return -1;
    }

    //maxId데이터라면 pop
    if(id == lockTable.size()-1){
        lockTable.pop_back();
    }
    
    //free
    return freeLTN(targetNode);
}
