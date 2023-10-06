#include"node.h"
#include"communicate.h"


/*private*/
int node::handleReq(){
    //cpp의 priority queue에서 pop은 데이터를 리턴하지 않음
    time_t timeStamp = reqQ.top().first; //타임스탬프
    request req = reqQ.top().second; //최우선 request
    reqQ.pop();

    switch(req.type){
        case t_reqLock:{
            if(lockTable.at(req.id).state == waitForLock){ //내가 요청한 reqLock이 나한테 왔음, 오류
                fprintf(stderr, "E: got reqLock but it's waitForLock\n");
                return -1;
            }
            if(lockTable.at(req.id).state != unlocked){ //해당 데이터는 현재 unlocked가 아님, 대기해야함
                /*reqTable에 pq에 노드를 추가해서 차례를 대기*/
                //reqTable에 추가될 노드 생성
                reqTableNode newNode;
                newNode.fd = req.fd;
                //reqTable에 새 노드 추가
                reqTable.at(req.id).push({timeStamp, newNode});

                //해당 request는 선행되었던 reqLock이 unlock되면서 처리될 것임
            }
            else if(isRoot || !(scm.childFds.size())){ //해당 데이터는 unlocked && root or leaf
                //allowLock응답
                allowLock(req.id, req.fd);
            }
            else{
                //reqLock전파
                reqLock(req.id, req.fd);
                //state waitFor로 변경
                lockTable.at(req.id).state = waitFor;
            }
        }
        break;
        case t_allowLock:{
            if(lockTable.at(req.id).state != waitFor){ //state이 waitFor가 아닌데 allowLock이 왔음, 오류
                fprintf(stderr, "E: got allowLock but not waitFor\n");
                return -1;
            }

            lockTable.at(req.id).state = locked; //state를 lock으로 변경
            if(lockTable.at(req.id).state == waitForLock){ //state가 waitForLock, 
                /*마침내 lock취득*/
            }
            else{
                //allowLock 전파
                allowLock(req.id, req.fd);
            }
        }
        break;
        case t_notifyUnlock:{
            if(lockTable.at(req.id).state != locked){ //state이 locked가 아닌데 unlock이 왔음, 오류
                fprintf(stderr, "E: got otifyUnlock but not locked\n");
                return -1;
            }

            if(!isRoot && scm.childFds.size()){ // not root and not leaf
                notifyUnlock(req.id, req.fd); //notifyUnlock전파
            }

            //reqTable, lockTable수정
            if(!(reqTable.at(req.id).size())){ //대기중이던 reqLock이 없음
                lockTable.at(req.id).state = unLocked;
            }
            else{ //대기중이던 reqLock 존재
                reqTableNode nextReq = reqTable.at(req.id).top; //다음 reqLock 받아오기
                reqLock(req.id, nextReq.fd); //다음 reqLock전파
                lockTable.at(req.id).state = waitFor; //상태를 waitFor로 변경
                reqTable.at(req.id).pop(); //상태를 waitFor로 변경
            }
            /*
            if(notifyUnlock 발신지) then{
                unlock 요청 수락됨
            }
            else{
                notifyUnlock 전파
            }
            */
        }
        break;
    }

    //마무리
}

/*public*/
node::node(const char* inputParentIp, int parentPort, int myPort){
    this->scm=new scManage(myPort); //소켓매니저 인스턴스 생성
    this->isRoot=(!inputParentIp); //루트노드 여부 결정(inputParentIp가 NULL이라면 루트노드)
    if(!(this->isRoot)){this->scm.connectParent(inputParentIp, parentPort);} //루트가 아니라면 부모노드에 연결
}

int node::handleMsg(msgType type, const char* msg, int srcFd){
    request req;
    time_t timeStamp;
    //타입에 영향받지 않는 request값 설정
    req.fd = srcFd;
    req.type = type;

    // 타입에 따른 request값 설정
    switch(type){
        case t_reqLock:{
            // msg값 읽기
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
}

int node::reqLock(u_int id, int exceptSocket){
    int size = sizeof(msgHeader)+sizeof(msgReqLock); //패킷의 총 크기 헤더+보디
    char* msg = malloc(size); //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_reqLock; //메시지 타입 설정
    
    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgReqLock*)msgPtr)->t = time(NULL);
    ((msgReqLock*)msgPtr)->id = id;

    // 메시지 뿌리기
    int result = scm.spreadMsg(msg, size, exceptSocket);
    free(msg);
    return result;
}

int node::allowLock(u_int id, int socket){
    int size = sizeof(msgHeader)+sizeof(msgAllowLock); //패킷의 총 크기 헤더+보디
    char* msg = malloc(size); //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_allowLock; //메시지 타입 설정

    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgAllowLock*)msgPtr)->id = id;

    // 메시지 뿌리기
    int result = write(socket, msg, size);
    free(msg);
    return result;
}

int node::notifyUnlock(u_int id, int exceptSocket){
    int size = sizeof(msgHeader)+sizeof(msgNotifyUnlock); //패킷의 총 크기 헤더+보디
    char* msg = malloc(size); //버퍼 생성
    char* msgPtr = msg;

    //헤더 설정
    ((msgHeader*)msgPtr)->size = size - sizeof(msgHeader); //메시지 크기 입력(헤더크기제외)
    ((msgHeader*)msgPtr)->type = t_notifyUnlock; //메시지 타입 설정
    
    //보디 설정
    msgPtr += sizeof(msgHeader);
    ((msgNotifyUnlock*)msgPtr)->id = id;

    // 메시지 뿌리기
    int result = scm.spreadMsg(msg, size, exceptSocket);
    free(msg);
    return result;
}