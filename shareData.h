#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<queue>

using namespace std;

typedef struct mem{
    void* memory; //동적할당 메모리
    u_int size; //메모리 크기
    bool available; //데이터의 사용 여부 표시
} mem;

//객체 하나는 data하나
class shareData{
    private:
    deque<mem*> memories; //메모리들의 배열

    public:
    shareData();
    int addData(u_int size);
    int rmData(u_int id);
    int setData(u_int id, const void* value, u_int size);
    const void* getData(u_int id);
};