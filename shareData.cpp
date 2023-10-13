#include"shareData.h"

shareData::shareData(){
}

int shareData::addData(u_int size){
    int i = 0;
    int maxId = memories.size();
    while(i < maxId && memories.at(i)->available){i++;}

    mem* newData = (mem*)malloc(sizeof(mem));
    memset(newData, 0, sizeof(mem));

    if(i == maxId){ //큐의 마지막에 추가
        memories.push_back(newData);
    }
    else if(i < maxId){ //큐의 중간에 대체
        memories.at(i)->available = 1;
        memories.at(i)->size = size;
        memories.at(i)->memory = malloc(size);
    }
    else{ //예상되지 못한 값
        fprintf(stderr, "E: unavailabel id (newId:%d/maxId:%d)\n", i, maxId);
        return -1;
    }

    //새로운 data의 id
    return i;
}

int shareData::rmData(u_int id){
    mem* target = memories.at(id);
    //접근 가능여부 체크
    if(target->available == 0){
        fprintf(stderr, "E: unallocated data %d\n", id);
        return -1;
    }

    free(target->memory);
    target->available = 0;

    return 0;
}

int shareData::setData(u_int id, const void* value, u_int size){
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
    mem* target = memories.at(id);
    //접근 가능여부 체크
    if(target->available == 0){
        fprintf(stderr, "E: unallocated data %d\n", id);
        return NULL;
    }

    return (const void*)(target->memory);
}