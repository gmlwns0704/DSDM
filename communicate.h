#pragma once
#ifndef H_COMMUNICATE
#define H_COMMUNICATE

#include<sys/types.h>
/*
소켓을 통한 통신에서 표준 규약 정의
*/

typedef enum msgType{
    t_reqLock = 0, //
    t_allowLock, //
    t_update
} msgType;
    
typedef struct msgHeader{
    msgType type; //메시지 타입
    uint size; //헤더제외 본문 사이즈
} msgHeader;

typedef struct msgReqLock{ //lock충돌시 우선순위를 정해야 하므로 타임스탬프 포함
    time_t t;
    uint id;
} msgReqLock;

typedef struct msgAllowLock{ //단순한 메시지의 재전파만 이루어지므로 타임스탬프X
    uint id;
} msgAllowLock;

typedef struct msgUpdate{ //값의 업데이트
    uint id;
    uint size; //뒤에 따라붙을 update된 데이터의 크기
    /*updated 값은 별도로 이어붙임, 사실상 헤더*/
} msgUpdate;

#endif
