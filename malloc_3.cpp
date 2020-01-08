#include <cstddef>
#include <unistd.h>
#include <cstring>

const int FAIL = -1;
const size_t MAX_SIZE = 100000000;
int first_allocation = 0;
// our meta-data struct, a node in a two directional linked list of the meta datas.
// contains effective size of data, is_free boolean value and the address of the start of meta-data.
struct metaDataNode{
    metaDataNode* next;
    metaDataNode* prev;
    size_t eff_size;
    bool is_free;
    void* meta_data_address;
    int align_me;
};

// global head and tail of the meta-data linked list.
metaDataNode *list_head = NULL;
metaDataNode *list_tail = NULL;

void merge(metaDataNode* p_node){
    int prev_update = 0;
    if (p_node->prev != NULL && p_node->prev->is_free) { // check if need to merge with previous
        p_node->prev->eff_size += p_node->eff_size + sizeof(metaDataNode);
        p_node->prev->next = p_node->next;
        if (p_node->next)
            p_node->next->prev = p_node->prev;
        prev_update = 1;
    }
    if (p_node->next != NULL && p_node->next->is_free) { // check if need to merge with next
        p_node->is_free = true;
        if (prev_update == 1){
            p_node->prev->eff_size += p_node->next->eff_size + sizeof(metaDataNode);
            if (p_node->next->next)
                p_node->next->next->prev = p_node->prev;
            p_node->prev->next = p_node->next->next;
            if (p_node->next == list_tail)
                list_tail = p_node->prev;
        }
        else{
            p_node->eff_size += p_node->next->eff_size + sizeof(metaDataNode);
            if (p_node->next->next != NULL)
                p_node->next->next->prev = p_node;
            p_node->next = p_node->next->next;
            if (p_node->next == list_tail)
                list_tail = p_node;
        }
    }
}

void* malloc(size_t size) {
    if (size == 0 || size > MAX_SIZE)
        return NULL;
    if (first_allocation == 0){ // make sure heap is aligned in the first allocation
        if ((int)sbrk(0) % 4 != 0) {
            if(sbrk(4 - (size % 4)) == (void*)FAIL)
                return NULL;
        }
        first_allocation++;
    }
    void *PB_meta;

    // align address
    if (size % 4 != 0)
        size = size + (4 - (size % 4));


    metaDataNode * list_iterator = list_head;
    while (list_iterator != NULL){
        // iterate over whole linked list searching for freed blocks that can contain this allocation
        list_iterator->meta_data_address;
        if (list_iterator->is_free && size <= list_iterator->eff_size){
            list_iterator->is_free = false;
            if (list_iterator->eff_size - size >= (size_t)(128 + sizeof(metaDataNode))){ // split the two blocks
                metaDataNode* new_meta = (metaDataNode*)
                        ((size_t*)((metaDataNode*)(list_iterator->meta_data_address) + 1) + (size/sizeof(size_t)));
                new_meta->next = list_iterator->next;
                new_meta->prev = list_iterator;
                new_meta->meta_data_address = (size_t*)(((metaDataNode*)(list_iterator->meta_data_address) + 1) +
                        (size/ sizeof(size_t)));
                new_meta->eff_size = list_iterator->eff_size - size - sizeof(metaDataNode);
                list_iterator->eff_size = size;
                if (list_iterator->next)
                    list_iterator->next->prev = new_meta;
                list_iterator->next = new_meta;
                new_meta->is_free = true;
                merge(new_meta);
            }

            return ((metaDataNode*)(list_iterator->meta_data_address) + 1);
        }
        list_iterator = list_iterator->next;
    }
    // wilderness block
    if(list_tail && list_tail->is_free && size > list_tail->eff_size){
        PB_meta = sbrk(size - list_tail->eff_size);
        if (PB_meta == (void*)FAIL)
            return NULL;
        list_tail->is_free = false;
        list_tail->eff_size = size;
        return (metaDataNode*)list_tail->meta_data_address + 1;
    }
    //meta data and data allocation using sbrk and create the meta data node.
    PB_meta = sbrk(sizeof(metaDataNode) + size);
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

// mark is_free as true for given block also merge it with its neighbours if they are not NULL and free as well.
void free(void* p) {
    metaDataNode* p_node = ((metaDataNode *)p - 1);
    merge(p_node);
    p_node->is_free = true;
}


// check if we can reallocate without allocating extra blocks and if so do it else free old memory and allocate a new
// block with the proper size and memcpy previous data.
//
void* realloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_SIZE)
        return NULL;

    if (oldp == NULL)
        return malloc(size);

    metaDataNode *oldpMeta = (metaDataNode *)oldp - 1;
    // align address
    if (size % 4 != 0){
        size = size + (4 - (size % 4));
    }

    if (oldpMeta->eff_size >= size) { // here it means we can reuse the old block for this allocation
        if (oldpMeta->eff_size - size >= (size_t)(128 + sizeof(metaDataNode))) { // here we need to split just as we do
                                                                                 // when reusing free block in malloc
            metaDataNode* new_meta = (metaDataNode*)
                    ((size_t*)((metaDataNode*)oldpMeta->meta_data_address + 1) + (size/sizeof(size_t)));
            new_meta->next = oldpMeta->next;
            new_meta->prev = oldpMeta;
            new_meta->meta_data_address = (size_t*)(((metaDataNode*)oldpMeta->meta_data_address + 1) +
                    (size/sizeof(size_t)));
            new_meta->eff_size = oldpMeta->eff_size - size - sizeof(metaDataNode);
            oldpMeta->eff_size = size;
            if (oldpMeta->next)
                oldpMeta->next->prev = new_meta;
            if (oldpMeta->next == list_tail)
                list_tail = new_meta;
            oldpMeta->next = new_meta;
            new_meta->is_free = true;
            merge(new_meta);
            return (metaDataNode *)oldpMeta->meta_data_address + 1;
        }
        return oldp; // so here start address of data doesn't change
    }
    // check if realloc is on wilderness and if so, just add the remaining size
    if ((metaDataNode*)list_tail->meta_data_address + 1 == oldp){
        void* PB_meta = sbrk(size - list_tail->eff_size);
        if (PB_meta == (void*)FAIL) {
            if (!list_tail->prev->is_free)
                return NULL;
            size_t orig_oldp_size = oldpMeta->eff_size;
            free(list_tail);
            return memcpy(malloc(size), oldp, orig_oldp_size);
        }
        list_tail->is_free = false;
        list_tail->eff_size = size;
        return (metaDataNode*)list_tail->meta_data_address + 1;
    }
    //  here check if current and adjacent block can contain size if its free
    if (oldpMeta->next != NULL && oldpMeta->next->is_free && oldpMeta->next->eff_size + oldpMeta->eff_size +
                                                                     sizeof(metaDataNode) >= size) {
        if ((int)(oldpMeta->next->eff_size) + (int)(oldpMeta->eff_size) - (int)size >= (int)128) {
            // so split after merge
            metaDataNode* new_meta = (metaDataNode*)
                    ((size_t*)((metaDataNode*)(oldpMeta->meta_data_address) + 1) + (size/sizeof(size_t)));
            if (oldpMeta->next) {
                new_meta->next = oldpMeta->next->next;
                if (oldpMeta->next->next)
                    oldpMeta->next->next->prev = new_meta;
            }
            new_meta->prev = oldpMeta;
            new_meta->meta_data_address = (size_t*)(((metaDataNode*)(oldpMeta->meta_data_address) + 1) +
                                                    (size/ sizeof(size_t)));
            new_meta->eff_size = oldpMeta->eff_size + oldpMeta->next->eff_size - size;
            oldpMeta->eff_size = size;
            oldpMeta->next = new_meta;
            oldpMeta->is_free = false;
            new_meta->is_free = true;
            merge(new_meta); // we split so merge
            return oldp;
        }
        // here just merge them

        oldpMeta->eff_size += oldpMeta->next->eff_size + sizeof(metaDataNode);
        if (oldpMeta->next->next != NULL)
            oldpMeta->next->next->prev = oldpMeta;
        oldpMeta->next = oldpMeta->next->next;
        return oldp;
    }

    // if we do allocate a new block, free old one and return the new one's address
    size_t orig_oldp_size = oldpMeta->eff_size;
    void* res = memcpy(malloc(size), oldp, orig_oldp_size);
    if (res != NULL && oldp != res)
        free(oldp);
    return res;
}

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

size_t _num_allocated_blocks(){
    size_t num_free = 0;
    metaDataNode * list_iterator = list_head;
    while (list_iterator != NULL) {
        num_free++;
        list_iterator = list_iterator->next;
    }
    return num_free;
}

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

