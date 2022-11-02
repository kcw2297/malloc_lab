#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"


team_t team = {
    /* Team name */
    "Team5",
    /* First member's full name */
    "Chae Wook Kim",
    /* First member's email address */
    "chaeugg@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};



/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
// My additional Macros
#define WSIZE     4          // word and header/footer size (bytes)
#define DSIZE     8          // double word size (bytes)
#define INITCHUNKSIZE (1<<6)
#define CHUNKSIZE (1<<12)//+(1<<7)
#define LISTLIMIT     20     
#define MAX(x, y) ((x) > (y) ? (x) : (y))
// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))
// Read and write a word at address p
#define GET(p)            (*(unsigned int *)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val))
// Store predecessor or successor pointer for free blocks
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))
// Read the size and allocation bit from address p
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)  // 1이 할당 0이 free
// Address of block's header and footer
#define HDRP(ptr) ((char *)(ptr) - WSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)
// Address of (physically) next and previous blocks
#define NEXT_BLKP(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE))
#define PREV_BLKP(ptr) ((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE))
// Address of free block's predecessor and successor entries
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)
// Address of free block's predecessor and successor on the segregated list
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))
// End of my additional macros


// Global var
void *segregated_free_lists[LISTLIMIT];
// Functions
static void *extend_heap(size_t size);
static void *coalesce(void *ptr);
static void *place(void *ptr, size_t asize);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);
static void *extend_heap(size_t size)
{
    void *ptr;                  
    size_t asize;                // Adjusted size
    asize = ALIGN(size);
    if ((ptr = mem_sbrk(asize)) == (void *)-1)
        return NULL;
    // Set headers and footer
    PUT(HDRP(ptr), PACK(asize, 0)); 
    PUT(FTRP(ptr), PACK(asize, 0));  
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));
    insert_node(ptr, asize);
    return coalesce(ptr);
}
static void insert_node(void *ptr, size_t size) {
    int list = 0;
    void *search_ptr = ptr;
    void *insert_ptr = NULL;
    // Select segregated list
    while ((list < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        list++;
    }
    // Keep size ascending order and search
    search_ptr = segregated_free_lists[list];
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr)))) {
        insert_ptr = search_ptr;
        search_ptr = PRED(search_ptr);
    }
    // Set predecessor and successor
    if (search_ptr != NULL) {
        if (insert_ptr != NULL) {
            SET_PTR(PRED_PTR(ptr), search_ptr);
            SET_PTR(SUCC_PTR(search_ptr), ptr);
            SET_PTR(SUCC_PTR(ptr), insert_ptr);
            SET_PTR(PRED_PTR(insert_ptr), ptr);
        } else {
            SET_PTR(PRED_PTR(ptr), search_ptr);
            SET_PTR(SUCC_PTR(search_ptr), ptr);
            SET_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    } else {
        if (insert_ptr != NULL) {
            SET_PTR(PRED_PTR(ptr), NULL);
            SET_PTR(SUCC_PTR(ptr), insert_ptr);
            SET_PTR(PRED_PTR(insert_ptr), ptr);
        } else {
            SET_PTR(PRED_PTR(ptr), NULL);
            SET_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    }
    return;
}
static void delete_node(void *ptr) {
    int list = 0;
    size_t size = GET_SIZE(HDRP(ptr));
    // Select segregated list
    while ((list < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        list++;
    }
    if (PRED(ptr) != NULL) {
        if (SUCC(ptr) != NULL) {
            SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
            SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
        } else {
            SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
            segregated_free_lists[list] = PRED(ptr);
        }
    } else {
        if (SUCC(ptr) != NULL) {
            SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
        } else {
            segregated_free_lists[list] = NULL;
        }
    }
    return;
}
static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));
    if (prev_alloc && next_alloc) {                         // Case 1
        return ptr;
    }
    else if (prev_alloc && !next_alloc) {                   // Case 2
        delete_node(ptr);
        delete_node(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {                 // Case 3
        delete_node(ptr);
        delete_node(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    } else {                                                // Case 4
        delete_node(ptr);
        delete_node(PREV_BLKP(ptr));
        delete_node(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    insert_node(ptr, size);
    return ptr;
}
static void *place(void *ptr, size_t asize)
{
    size_t ptr_size = GET_SIZE(HDRP(ptr));
    size_t remainder = ptr_size - asize;
    delete_node(ptr);
    if (remainder <= DSIZE * 2) {
        // Do not split block
        PUT(HDRP(ptr), PACK(ptr_size, 1));
        PUT(FTRP(ptr), PACK(ptr_size, 1));
    }
    else if (asize >= 100) {   //  새로 배치해야하는 공간크기가 realloc buffer 넘어선다면? => 다음블록에서 배치 (현재블록은 가용블록으로 유지)
        // Split block
        PUT(HDRP(ptr), PACK(remainder, 0));
        PUT(FTRP(ptr), PACK(remainder, 0));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(asize, 1));
        insert_node(ptr, remainder);
        return NEXT_BLKP(ptr);
    }
    else {  // 새로 배치해야하는 공간크기가 realloc buffer 안에 있다면? => ptr 을 place
        // Split block
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(remainder, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(remainder, 0));
        insert_node(NEXT_BLKP(ptr), remainder);
    }
    return ptr;
}
/*
 * mm_init - initialize the malloc package.
*/
int mm_init(void)
{
    int list;        
    char *heap_start; // Pointer to beginning of heap
    // Initialize segregated free lists
    for (list = 0; list < LISTLIMIT; list++) {
        segregated_free_lists[list] = NULL;
    }
    // Allocate memory for the initial empty heap
    if ((long)(heap_start = mem_sbrk(4 * WSIZE)) == -1)
        return -1;
    PUT(heap_start, 0);                            /* Alignment padding */
    PUT(heap_start + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_start + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_start + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    if (extend_heap(INITCHUNKSIZE) == NULL)
        return -1;
    return 0;
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    void *ptr = NULL;  /* Pointer */
    // Ignore size 0 cases
    if (size == 0)
        return NULL;
    // Align block size
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = ALIGN(size+DSIZE);
    }
    int list = 0;
    size_t searchsize = asize;
    // Search for free block in segregated list
    while (list < LISTLIMIT) { //0 ~ 19
        if ((list == LISTLIMIT - 1) || ((searchsize <= 1) && (segregated_free_lists[list] != NULL))) {
            // 리스트의 맨 끝이거나, (할당받고자 하는 사이즈가 1보다 작으면서 seglist의 현재인덱스 길이 클래스의 가용리스트가 있으면)
            ptr = segregated_free_lists[list];
            // ptr에 현재 길이 클래스 가용리스트를 준다.
            // Ignore blocks that are too small or marked with the reallocation bit
            while ((ptr != NULL) && (   (asize > GET_SIZE(HDRP(ptr)))))
            {
                // ptr이 NULL이 아니면서, 원하는 asize가 현재블록보다 크거나()
                // ptr이 NULL이 아니면서, 현재블록에 RATAG가 있을 때(재할당태그가 붙어있으면 그 PRED를 할당)
                ptr = PRED(ptr);  // 오름차순이므로 PRED는 현재 클래스의 선행 블록이라서 크기가 작다.
            }
            if (ptr != NULL)
                break;
        }
        searchsize >>= 1;
        list++;
    }
    // if free block is not found, extend the heap
    if (ptr == NULL) {
        extendsize = MAX(asize, CHUNKSIZE);
        if ((ptr = extend_heap(extendsize)) == NULL)
            return NULL;
    }
    // Place and divide block
    ptr = place(ptr, asize);
    // Return pointer to newly allocated block
    return ptr;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    insert_node(ptr, size);
    coalesce(ptr);
    return;
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *new_ptr = ptr;    /* Pointer to be returned */
    size_t new_size = size; /* Size of new block */
    int remainder;          /* Adequacy of block sizes */
    int extendsize;         /* Size of heap extension */
    // Ignore size 0 cases
    if (size == 0)
        return NULL;
    // Align block size
    if (new_size <= DSIZE) {
        new_size = 2 * DSIZE;
    } else {
        new_size = ALIGN(size+DSIZE);
    }
    /* Allocate more space if overhead falls below the minimum */
    /* Check if next block is a free block or the epilogue block */
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)))) {
        remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
        if (remainder < 0) {
            // 추가 공간 필요
            extendsize = MAX(-remainder, CHUNKSIZE);
            if (extend_heap(extendsize) == NULL)
                return NULL;
            remainder += extendsize;
        }
        delete_node(NEXT_BLKP(ptr));   // 스플릿된 채 가용리스트에 들어있는 next는 삭제
        // Do not split block
        PUT(HDRP(ptr), PACK(new_size + remainder, 1)); // (ptr + next) 사이즈만큼 place!
        PUT(FTRP(ptr), PACK(new_size + remainder, 1));
    } else {
        new_ptr = mm_malloc(new_size - DSIZE); 
        memcpy(new_ptr, ptr, size);
        mm_free(ptr);
    }
    // Return the reallocated block
    return new_ptr;
}