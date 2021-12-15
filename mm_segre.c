/* 이해해 도움이 되는 정보
 * 1. prev, next는 physically 앞과 뒤; pred, succ은 링크드리스트상 앞과 뒤
 * 2. 이 구현에서의 경우 doubly linked list를 오름차순으로 정렬했고,
 * 한 노드의 다음노드를 SUCC이 아닌 PRED로 정했다.
 */


/*
 * 출처: https://github.com/mightydeveloper/Malloc-Lab
 * mm.c - malloc using segregated list
 * KAIST
 * Tony Kim
 * 
 * In this approach, 
 * Every block has a header and a footer 
 * in which header contains reallocation information, size, and allocation info
 * and footer contains size and allocation info.
 * Free list are tagged to the segregated list.
 * Therefore all free block contains pointer to the predecessor and successor.
 * The segregated list headers are organized by 2^k size.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* Double word (8) alignment */
#define ALIGNMENT 8
/* Rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define WSIZE 4
#define DSIZE 8
#define INITCHUNKSIZE (1<<6) // memory util 점수 높이기 용. 최초 extend_heap시 기존 1024(=2^12/2^2) words 만큼 키워주는 게 아니라, 64(=2^6) words만큼 키워줌
#define CHUNKSIZE (1<<12)

#define LISTLIMIT 20
#define REALLOC_BUFFER (1<<7) // ?

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x ,y) ((x) < (y) ? (x) : (y))

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a word at address hdr_or_ftr_p
#define GET(hdr_or_ftr_p) (*(unsigned int *)(hdr_or_ftr_p))
#define PUT_WITH_RATAG(hdr_or_ftr_p, val) (*(unsigned int *)(hdr_or_ftr_p) = (val) | GET_RATAG(hdr_or_ftr_p))
#define PUT_WITHOUT_RATAG(hdr_or_ftr_p, val) (*(unsigned int *)(hdr_or_ftr_p) = (val)) // RATAG의 의미는 Reallocation bit. 따라서 RATAG 없이 사이즈와 alloc bit만 넣기

// Store predecessor or successor pointer for each free block
// 어떤 free block이 overhead로 보관하는 pred pointer나 succ pointer 정보를 갱신할 때 사용
#define SET_PRED_OR_SUCC_PTR(pred_or_succ_ptr, pred_or_succ) (*(unsigned int *)(pred_or_succ_ptr) = (unsigned int)(pred_or_succ))

// Read the size and allocation bit, reallocation tag from address hdr_or_ftr_p
#define GET_SIZE(hdr_or_ftr_p) (GET(hdr_or_ftr_p) & ~0x7)
#define GET_ALLOC(hdr_or_ftr_p) (GET(hdr_or_ftr_p) & 0x1)
#define GET_RATAG(hdr_or_ftr_p) (GET(hdr_or_ftr_p) & 0x2)
#define SET_RATAG(hdr_or_ftr_p) (GET(hdr_or_ftr_p) |= 0x2)
#define REMOVE_RATAG(hdr_or_ftr_p) (GET(hdr_or_ftr_p) &= ~0x2)

// Address of block's header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) (NEXT_BLKP(bp) - DSIZE)

// Address of (physically) next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

// Address of free block's predecessor and successor entries
#define PRED_PTR(bp) ((char *)(bp)) // bp가 보관하고 있는 predecessor 포인터를 가리키는 word의 첫 바이트를 가리키기 때문에 사실 predecessor의 경우 이 매크로가 없어도 되지만, SUCC_PTR과의 통일성을 위해 존재
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)

// Address of free block's predecessor and successor on the segregated list
#define PRED(bp) (*(char **)(bp)) // 32-bit 모드여서 pointer가 WSIZE(char *이 4바이트가 됨). 그리고 free block 그림을 아래서 보면, bp는 pointer to its predecessor임
#define SUCC(bp) (*(char **)(SUCC_PTR(bp)))

/*
    Segregated Free List size class
    | list_idx | size range(hex)   | size range(dec) |
    |  0       | 0~1               | 0~1             |
    |  1       | 10~11             | 2~3             |
    |  2       | 100~111           | 4~7             |
    |  3       | 1000~1111         | 8~15            |
    ...
    | 19       | 1000...0~1111...1 | 2^19~2^20-1     |
*/
void *segregated_free_lists[LISTLIMIT];

// private helper functions
static void *extend_heap(size_t size);
static void insert_node_in_freelist(void *bp, size_t node_size);
static void delete_node_in_freelist(void *bp);
static void *coalesce(void *bp);
static void *allocate(void *bp, size_t asize);

team_t team = {
    /* Team name */
    "6",
    /* First member's full name */
    "Jun-Young Choi",
    /* First member's email address */
    "cjy13753@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

///////////////////////////////// Block information /////////////////////////////////////////////////////////
/*
 
A   : Allocated? (1: true, 0:false)
RA  : Reallocation tag (1: true, 0:false)
 
 < Allocated Block >
 
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |  | A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                                                                                               |
            |                                                                                               |
            .                              Payload and padding                                              .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
 < Free block >
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |RA| A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its predecessor in Segregated list                          |
bp+WSIZE--> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its successor in Segregated list                            |
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            .                                                                                               .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
*/
///////////////////////////////// End of Block information /////////////////////////////////////////////////////////

//////////////////////////////////////// Helper functions //////////////////////////////////////////////////////////
static void *extend_heap(size_t size)
{
    void *bp;
    size_t asize;

    asize = ALIGN(size);

    if ((bp = mem_sbrk(asize)) == (void *)-1)
        return NULL;
    
    // Set headers and footer
    PUT_WITHOUT_RATAG(HDRP(bp), PACK(asize, 0));
    PUT_WITHOUT_RATAG(FTRP(bp), PACK(asize, 0));
    PUT_WITHOUT_RATAG(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* update epilogue header */
    insert_node_in_freelist(bp, asize); /* 새로 생성된 free block을 segregated_free_lists 중 적당한 list에 노드로 넣어준다. */

    return coalesce(bp);
}

static void insert_node_in_freelist(void *bp, size_t node_size)
{
    int list_idx = 0;
    void *pred_bp = NULL;
    void *succ_bp = NULL;

    // Select segregated free list
    while ((list_idx < LISTLIMIT - 1) && (node_size > 1)) {
        node_size >>= 1;
        list_idx++;
    }

    // Keep size ascending order and search
    pred_bp = segregated_free_lists[list_idx]; // free list의 처음 노드부터 search 시작
    while ((pred_bp != NULL) && (node_size > GET_SIZE(HDRP(pred_bp)))) {
        succ_bp = pred_bp;
        pred_bp = PRED(pred_bp);
    }

    // Set predecessor and successor
    if (pred_bp != NULL) {
        // Case 1: 이미 존재하는 block 사이에 삽입해야 하는 경우
        if (succ_bp != NULL) {
            SET_PRED_OR_SUCC_PTR(PRED_PTR(bp), pred_bp);
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(pred_bp), bp);
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(bp), succ_bp);
            SET_PRED_OR_SUCC_PTR(PRED_PTR(succ_bp), bp);
        }
        // Case 2: 비어 있지 않은 segregated free list의 맨 앞에 삽입하는 경우
        else {
            SET_PRED_OR_SUCC_PTR(PRED_PTR(bp), pred_bp);
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(pred_bp), bp);
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(bp), NULL);
            segregated_free_lists[list_idx] = bp;
        }
    } 
    else {
        // Case 3: 비어 있지 않은 segregated free list의 맨 뒤에 삽입하는 경우
        if (succ_bp != NULL) {
            SET_PRED_OR_SUCC_PTR(PRED_PTR(bp), NULL);
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(bp), succ_bp);
            SET_PRED_OR_SUCC_PTR(PRED_PTR(succ_bp), bp);
        } 
        // Case 4: segregated_free_list가 비어있는 경우
        else {
            SET_PRED_OR_SUCC_PTR(PRED_PTR(bp), NULL);
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(bp), NULL);
            segregated_free_lists[list_idx] = bp;
        }
    }
}

static void delete_node_in_freelist(void *bp)
{
    int list_idx = 0;
    size_t size = GET_SIZE(HDRP(bp));

    // Select segregated list
    // bp가 어느 segregated free list에 속하는지 찾아주기
    while ((list_idx < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        list_idx++;
    }

    if (PRED(bp) != NULL) {
        // Case 1: pred block과 succ block이 모두 존재하는 경우
        if (SUCC(bp) != NULL) {
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(PRED(bp)), SUCC(bp));
            SET_PRED_OR_SUCC_PTR(PRED_PTR(SUCC(bp)), PRED(bp));
        }
        // Case 2: pred block 존재하고, succ block 존재하지 않는 경우
        else {
            SET_PRED_OR_SUCC_PTR(SUCC_PTR(PRED(bp)), NULL);
            segregated_free_lists[list_idx] = PRED(bp);            
        }
    }
    else {
        // Case 3: pred block이 존재하지 않고, succ block이 존재하는 경우
        if (SUCC(bp) != NULL) {
            SET_PRED_OR_SUCC_PTR(PRED_PTR(SUCC(bp)), NULL); // 해석하면, bp의 successor가 가지고 있는 predecessor pointer 정보를 NULL로 갱신한다.
        }
        // Case 4:prev block과 succ block이 모두 존재하지 않는 경우
        else {
            segregated_free_lists[list_idx] = NULL;
        }

    }
}

/* Free block에 대해서만 coalesce 진행 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // Do not coalesce with previous block if the previous block is tagged with Reallocation tag
    if (GET_RATAG(HDRP(PREV_BLKP(bp))))
        prev_alloc = 1;

    // Case 1: prev와 next 모두 allocated인 경우(예: mem_init에서 extend_heap한 경우)
    if (prev_alloc && next_alloc) {
        return bp;
    }
    // Case 2: prev는 allocated, next는 free인 경우
    else if (prev_alloc && !next_alloc) {
        delete_node_in_freelist(bp);
        delete_node_in_freelist(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT_WITH_RATAG(HDRP(bp), PACK(size, 0));
        PUT_WITH_RATAG(FTRP(bp), PACK(size, 0));
    }
    // Case 3: prev는 free, next는 allocated인 경우
    else if (!prev_alloc && next_alloc) {
        delete_node_in_freelist(bp);
        delete_node_in_freelist(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT_WITH_RATAG(FTRP(bp), PACK(size, 0));
        PUT_WITH_RATAG(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 
    // Case 4: prev와 next 모두 free인 경우
    else {
        delete_node_in_freelist(bp);
        delete_node_in_freelist(PREV_BLKP(bp));
        delete_node_in_freelist(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE((HDRP(NEXT_BLKP(bp))));
        PUT_WITH_RATAG(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT_WITH_RATAG(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_node_in_freelist(bp, size);

    return bp;
}

static void *allocate(void *bp, size_t asize)
{
    size_t block_size = GET_SIZE(HDRP(bp));
    size_t remainder = block_size - asize;

    delete_node_in_freelist(bp);

    // split 하기에는 free block 사이즈가 넉넉하지 않은 경우
    if (remainder <= DSIZE * 2) {
        PUT_WITH_RATAG(HDRP(bp), PACK(block_size, 1));
        PUT_WITH_RATAG(FTRP(bp), PACK(block_size, 1));
    }
    else {
        if (asize < 100) {
            PUT_WITH_RATAG(HDRP(bp), PACK(asize, 1));
            PUT_WITH_RATAG(FTRP(bp), PACK(asize, 1));
            PUT_WITHOUT_RATAG(HDRP(NEXT_BLKP(bp)), PACK(remainder, 0));
            PUT_WITHOUT_RATAG(FTRP(NEXT_BLKP(bp)), PACK(remainder, 0));
            insert_node_in_freelist(NEXT_BLKP(bp), remainder);
        }
        // allocate request block 사이즈가 100 byte 이상인 경우(특수 케이스)
        // 왜 이렇게 나눈 것일까?
        else {
            PUT_WITH_RATAG(HDRP(bp), PACK(remainder, 0));
            PUT_WITH_RATAG(FTRP(bp), PACK(remainder, 0));
            PUT_WITHOUT_RATAG(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
            PUT_WITHOUT_RATAG(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
            insert_node_in_freelist(bp, remainder);
            return NEXT_BLKP(bp);
        }
    }
    return bp;
}
//////////////////////////////////////// End of Helper functions ////////////////////////////////////////

/*
 * mm_init - initialize the malloc package.
 * Before calling mm_malloc, mm_realloc, or mm_free, 
 * the application program calls mm_init to perform any necessary initializations,
 * such as allocating the initial heap area.
 *
 * Return value : -1 if there was a problem, 0 otherwise.
 */
int mm_init(void)
{
    int list_idx;
    char *heap_start; // Pointer to beginning of heap

    // Initialize segregated free lists
    for (list_idx = 0; list_idx < LISTLIMIT; list_idx++) {
        segregated_free_lists[list_idx] = NULL;
    }

    // Allocate memory for the initial empty heap
    if ((heap_start = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT_WITHOUT_RATAG(heap_start, 0); /* Alignment padding */
    PUT_WITHOUT_RATAG(heap_start + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT_WITHOUT_RATAG(heap_start + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT_WITHOUT_RATAG(heap_start + (3 * WSIZE), PACK(0, 1)); /* Epilogue header */

    if (extend_heap(INITCHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 * 
 * Role:
 * 1. The mm_malloc routine returns a pointer to an allocated block payload.
 * 2. The entire allocated block should lie within the heap region.
 * 3. The entire allocated block should not overlap with any other chunk.
 * 
 * Return value: Always return a pointer to payload that is aligned to 8 bytes boundary.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit is found */
    void *bp = NULL;

    // Ignore size 0 cases
    if (size == 0)
        return NULL;
    
    // Align block size
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = ALIGN(size + DSIZE);
    }

    int list_idx = 0;
    size_t searchsize = asize;
    // Search for free block in segregated list
    while (list_idx < LISTLIMIT) {
        if ((list_idx == LISTLIMIT - 1) || ((searchsize <= 1) && (segregated_free_lists[list_idx] != NULL))) {
            bp = segregated_free_lists[list_idx];
            // Ignore blocks that are too small or marked with the reallocation bit
            while ((bp != NULL) && ((asize > GET_SIZE(HDRP(bp))) || (GET_RATAG(HDRP(bp))))) {
                bp = PRED(bp);
            }
            if (bp != NULL)
                break;

        }
    }

    searchsize >>= 1;
    list_idx++;

    // if free block is not found, extend the heap
    if (bp == NULL) {
        extendsize = MAX(asize, CHUNKSIZE);

        if ((bp = extend_heap(extendsize)) == NULL)
            return NULL;
    }

    // allocate the free block and split it if the free block is too big for allocate request
    bp = allocate(bp, asize);

    // Return pointer to newly allocated block
    return bp;
}

/* 
 * mm_free - freeing a block does nothing.
 * Role: The mm_free routine frees the block pointed to by bp
 * Return value: returns nothing
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    REMOVE_RATAG(HDRP(NEXT_BLKP(bp)));
    PUT_WITH_RATAG(HDRP(bp), PACK(size, 0));
    PUT_WITH_RATAG(FTRP(bp), PACK(size, 0));

    insert_node_in_freelist(bp, size);
    coalesce(bp); // coalesce 함수는 free가 돼서 segregated free list에 들어간 block pointer를 인자로 받기 때문에 앞에 insert 함수가 먼저 실행된다.
}

/* 
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * 
 * Role : The mm_realloc routine returns a pointer to an allocated
 *        region of at least size bytes with constraints.
 * 
 */
void *mm_realloc(void *bp, size_t size)
{
    void *new_bp = bp; /* Pointer to be returned */
    size_t new_size = size; /* Size of new block */
    int remainder; /* Adequacy of block sizes */
    int extendsize; /* Size of heap extension */
    int block_buffer; /* Size of block buffer */

    /* Ignore size 0 cases */
    if (size == 0)
        return NULL;
    
    // Align block size
    if (new_size <= DSIZE) {
        new_size = 2 * DSIZE;
    } else {
        new_size = ALIGN(size + DSIZE);
    }

    /* Add overhead requirements to block size */
    new_size += REALLOC_BUFFER;

    /* Calculate block buffer */
    block_buffer = GET_SIZE(HDRP(bp)) - new_size;

    /* Allocate more space if overhead falls below the minimum */
    if (block_buffer < 0) {
        /* Check if next block is a free block or the epilogue block */
        if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))) || !GET_SIZE(HDRP(NEXT_BLKP(bp)))) {
            remainder = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLKP(bp))) - new_size;
            if (remainder < 0) {
                extendsize = MAX(-remainder, CHUNKSIZE);
                if (extend_heap(extendsize) == NULL)
                    return NULL;
                remainder += extendsize;
            }

            delete_node_in_freelist(NEXT_BLKP(bp));

            // Do not split block
            PUT_WITHOUT_RATAG(HDRP(bp), PACK(new_size + remainder, 1));
            PUT_WITHOUT_RATAG(FTRP(bp), PACK(new_size + remainder, 1));
        } else {
            new_bp = mm_malloc(new_size - DSIZE);
            memcpy(new_bp, bp, MIN(size, new_size));
            mm_free(bp);
        }
        block_buffer = GET_SIZE(HDRP(new_bp)) - new_size;
    }

    // Tag the next block if block overhead drops below twice the overhead
    if (block_buffer < 2 * REALLOC_BUFFER)
        SET_RATAG(HDRP(NEXT_BLKP(new_bp)));

    // Return the reallocated block
    return new_bp;
}