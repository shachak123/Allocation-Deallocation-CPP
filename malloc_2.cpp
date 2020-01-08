#include <cstddef>
#include <unistd.h>
#include <cstring>

const int FAIL = -1;
const size_t MAX_SIZE = 100000000;

// our meta-data struct, a node in a two directional linked list of the meta datas.
// contains effective size of data, is_free boolean value and the address of the start of meta-data.
struct metaDataNode{
    metaDataNode* next;
    metaDataNode* prev;
    size_t eff_size;
    bool is_free;
    void* meta_data_address;
};
// global head and tail of the meta-data linked list.
metaDataNode *list_head = NULL;
metaDataNode *list_tail = NULL;


void* malloc(size_t size) {
    if (size == 0 || size > MAX_SIZE)
        return NULL;

    metaDataNode * list_iterator = list_head;
    // iterate over whole linked list searching for freed blocks that can contain this allocation
    while (list_iterator != NULL){
        if (list_iterator->is_free && size <= list_iterator->eff_size){
            list_iterator->is_free = false;
            return ((metaDataNode*)(list_iterator->meta_data_address) + 1);
        }
        list_iterator = list_iterator->next;
    }
    //meta data and data allocation using sbrk and create the meta data node.
    void *PB_meta = sbrk(sizeof(metaDataNode) + size);
    if (PB_meta == (void*)FAIL)
        return NULL;
    metaDataNode* PB_asMeta = (metaDataNode*) PB_meta;
    PB_asMeta->next = NULL;
    PB_asMeta->prev = list_tail;
    PB_asMeta->meta_data_address = PB_meta;
    PB_asMeta->is_free = false;
    PB_asMeta->eff_size = size;
    if (list_head == NULL) { // list_head = NULL in first allocation, so we should initialize it
        list_head = PB_asMeta;
        list_tail = list_head;
        return ((metaDataNode*)PB_meta + 1);
    }
    list_tail->next = PB_asMeta;
    list_tail = PB_asMeta;
    return ((metaDataNode*)PB_meta + 1); // pointer arithmetics we need to return actual data address
}

// simply call malloc and use memset to make values zero.
void* calloc(size_t num, size_t size){
    void* res = malloc(size*num);
    if (res == NULL)
        return res;
    return memset(res, 0, size*num);
}

// mark is_free as true for given block
void free(void* p) {
    ((metaDataNode *)((metaDataNode *)p - 1))->is_free = true;
}

// check if we can reallocate without allocating extra blocks and if so do it else free old memory and allocate a new
// block with the proper size and memcpy previous data
void* realloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE)
        return NULL;
    if (oldp == NULL)
        return malloc(size);

    metaDataNode *oldpMeta = (metaDataNode *)oldp - 1;
    if (oldpMeta->eff_size >= size) {
        return oldp;
    }

    void* res = memcpy(malloc(size), oldp, oldpMeta->eff_size);
    if (res != NULL && oldp != res)
        free(oldp);
    return res;
}

// iterate to find number of free blocks (all will be inside linked list)
size_t _num_free_blocks(){
    size_t num_free = 0;
    metaDataNode * list_iterator = list_head;
    while (list_iterator != NULL) {
        if (list_iterator->is_free) {
            num_free++;
        }
        list_iterator = list_iterator->next;
    }
    return num_free;
}
// iterate to find all free blocks (all will be inside linked list) and sum up effective size of all

size_t _num_free_bytes (){
    size_t free_bytes = 0;
    metaDataNode * list_iterator = list_head;
    while (list_iterator != NULL) {
        if (list_iterator->is_free) {
            free_bytes += list_iterator->eff_size;
        }
        list_iterator = list_iterator->next;
    }
    return free_bytes;
}

// iterate to find number of blocks (all will be inside linked list)
size_t _num_allocated_blocks(){
    size_t num_free = 0;
    metaDataNode * list_iterator = list_head;
    while (list_iterator != NULL) {
        num_free++;
        list_iterator = list_iterator->next;
    }
    return num_free;
}
// iterate to find number of blocks (all will be inside linked list) and sum all bytes

size_t _num_allocated_bytes(){
    size_t num_bytes = 0;
    metaDataNode * list_iterator = list_head;
    while (list_iterator != NULL){
        num_bytes += list_iterator->eff_size;
        list_iterator = list_iterator->next;
    }
    return num_bytes;
}


size_t _num_meta_data_bytes(){
    return (_num_allocated_blocks()*sizeof(metaDataNode));
}

size_t _size_meta_data(){
    return sizeof(metaDataNode);
}

