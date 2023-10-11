#include"shareData.h"

shareData::shareData(u_int inputSize){
    this->memory = malloc(inputSize);
    this->size = inputSize;
    this->id  = (shareData::maxId)++; //고유값 할당 후 다음 고유값 설정
}

int shareData::setData(void* newData, u_int inputSize){
    if(inputSize != this->size){
        fprintf(stderr, "E: changeDate size not wrong\n");
        return -1;
    }
    
    memset(this->memory, 0, this->size);
    memcpy(this->memory, newData, inputSize);
}