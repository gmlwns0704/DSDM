#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<queue>

using namespace std;

//객체 하나는 data하나
class shareData{
    private:
    u_int id;

    static u_int maxId = 0;
    void* memory; //실제 데이터가 저장되는 메모리
    u_int size; //데이터 사이즈

    public:
    u_int getMaxId(){
        return shareData::maxId;
    }
    shareData(u_int inputSize);
    int setData(void* newData, u_int inputSize); //데이터 값 변경
};