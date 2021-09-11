#ifndef CVECTOR_H_
#define CVECTOR_H_

#include <stdlib.h>

#define CVECTOR_INIT_CAPACITY 4

typedef struct{
    void** data;
    int size;
    int capacity;
    size_t element_size;
} cvector;

void cvector_init(cvector* __v, size_t __dataSize);

int cvector_size(cvector* __v);

void cvector_push(cvector* __v, void* __data);

int cvector_set(cvector* __v, int __index, void* __data);

int cvector_delete(cvector* __v, int __index);

void* cvector_get(cvector* __v, int __index);

void cvector_clear(cvector* __v);

void cvector_resize(cvector* __v, int __newCap);

#endif /* CVECTOR_H_ */
