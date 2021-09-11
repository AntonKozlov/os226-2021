#include "cvector.h"
#include <string.h>

void cvector_init(cvector* __v, size_t __dataSize){
    __v->capacity = CVECTOR_INIT_CAPACITY;
    __v->size = 0;
    __v->element_size = __dataSize;
    __v->data = (void **) malloc(CVECTOR_INIT_CAPACITY * sizeof(void*));
}

int cvector_size(cvector* __v){
    return __v->size;
}

void cvector_push(cvector* __v, void* __data){
    if(__v->size >= __v->capacity)
        cvector_resize(__v, __v->capacity + __v->capacity / 2);

    __v->data[__v->size] = (void*) malloc(__v->element_size);
    memcpy(__v->data[__v->size], __data, __v->element_size);

    __v->size++;
}

int cvector_set(cvector* __v, int __index, void* __data){
    if(__index >= 0 && __index < __v->size){
        memcpy(__v->data[__index], __data, __v->element_size);
        return 1;
    }
    return 0;
}

int cvector_delete(cvector* __v, int __index){
    if(__index < 0 || __index > __v->size - 1 || __v->size <= 0)
        return 0;

    free(cvector_get(__v, __index));

    for(int i = __index; i < __v->size - 1; i++)
        __v->data[i] = __v->data[i + 1];

    cvector_resize(__v, __v->size - 1);
    __v->size--;
    return 1;
}

void* cvector_get(cvector* __v, int __index){
    if(__index < 0 || __index > __v->size - 1 || __v->size <= 0)
        return NULL;

    return __v->data[__index];
}

void cvector_clear(cvector* __v){
    for(int i = 0; i < __v->size; i++)
        free(__v->data[i]);
    __v->size = 0;
    free(__v->data);
}

void cvector_resize(cvector* __v, int __newCap){
    __v->capacity = __newCap;
    __v->data = realloc(__v->data, __newCap * sizeof(void *));
}