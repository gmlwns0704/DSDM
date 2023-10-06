#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<list>

using namespace std;

class shareData{
    private:
    u_int id;

    static u_int uniqValue;
    void* memory; //실제 데이터가 저장되는 메모리
    u_int size; //데이터 사이즈

    public:
    shareData(u_int inputSize);
    int changeData(void* newData, u_int inputSize); //데이터 값 변경
};