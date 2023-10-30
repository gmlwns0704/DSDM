#include"shareData.h"

shareData::shareData(){
    #ifdef DBG
    fprintf(stderr, "DBG: sdm created\n");
    #endif
}

int shareData::addData(u_int size){
    #ifdef DBG
    fprintf(stderr, "DBG: sdm addData(size:%u)\n", size);
    #endif
    int i = 0;
    int maxId = memories.size();
    #ifdef DBG
    fprintf(stderr, "DBG: sdm addData: memories.size: %d\n", maxId);
    #endif
    //available 인덱스 찾기
    while(i < maxId && memories.at(i)->available){i++;}

    if(i == maxId){ //큐의 마지막에 추가
        #ifdef DBG
        fprintf(stderr, "DBG: sdm addData: push new mem\n");
        #endif
        mem* newData = (mem*)malloc(sizeof(mem));
        memset(newData, 0, sizeof(mem));
        newData->size=size;
        newData->available=1;
        //size가 0이라면 malloc안함
        if(size){newData->memory = malloc(size);}
        else{newData->memory=NULL;}
        memories.push_back(newData);
    }
    else if(i < maxId){ //큐의 중간에 대체
        #ifdef DBG
        fprintf(stderr, "DBG: sdm addData: change mem at %d\n", i);
        #endif
        memories.at(i)->available = 1;
        memories.at(i)->size = size;
        //size가 0이라면 malloc안함
        if(size){memories.at(i)->memory = malloc(size);}
        else{memories.at(i)->memory=NULL;}
    }
    else{ //예상되지 못한 값
        fprintf(stderr, "E: unavailabel id (newId:%d/maxId:%d)\n", i, maxId);
        return -1;
    }

    //새로운 data의 id
    #ifdef DBG
    fprintf(stderr, "DBG: sdm addData return %d\n", i);
    #endif
    return i;
}

int shareData::rmData(u_int id){
    #ifdef DBG
    fprintf(stderr, "DBG: sdm rmData(id:%u)\n", id);
    #endif
    mem* target = memories.at(id);
    //접근 가능여부 체크
    if(target->available == 0){
        fprintf(stderr, "E: unallocated data %d\n", id);
        return -1;
    }

    //size가 0이라면 free 필요없음
    if(target->size){free(target->memory);}
    target->available = 0;

    return 0;
}

int shareData::setData(u_int id, const void* value, u_int size){
    #ifdef DBG
    fprintf(stderr, "DBG: sdm setData(id:%u, value:%p, size:%u)\n", id, value, size);
    #endif
    mem* target = memories.at(id);
    //접근 가능여부 체크
    if(target->available == 0){
        fprintf(stderr, "E: unallocated data %d\n", id);
        return -1;
    }

    // 크기 체크
    if(target->size < size){
        fprintf(stderr, "E: size is bigger than allowed, %d > %d\n", size, target->size);
        return -1;
    }

    memset(target->memory, 0, target->size);
    memcpy(target->memory, value, size);

    return 0;
}

const void* shareData::getData(u_int id){
    #ifdef DBG
    fprintf(stderr, "DBG: sdm getData(id:%u)\n", id);
    #endif
    mem* target = memories.at(id);
    //접근 가능여부 체크
    if(target->available == 0){
        fprintf(stderr, "E: unallocated data %d\n", id);
        return NULL;
    }

    return (const void*)(target->memory);
}