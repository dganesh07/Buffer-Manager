#include "dberror.h"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.c"
#include "buffer_mgr_stat.h"
#include "storage_mgr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "replacementStrategies.c"
#include "dt.h"


struct hash * createHashTable(int totalFrames) { 
    struct hash *temp = (struct hash *) malloc(sizeof (struct hash *));
    temp->capacity = MAX_CAPACITY;
    temp->pageTable = (struct DLnode **) malloc(temp->capacity * sizeof (struct DLnode*));
    int i;
    for (i = 0; i < temp->capacity; ++i) {
        temp->pageTable[i] = NULL;
    }

    return temp;
}

struct DLnode * createNewNode() {
    struct DLnode *node = malloc(sizeof (struct DLnode));
    node->pageNumber = NO_PAGE;
    node->frameNum = 0;
    node->dirty = 0;
    node->fixcount = 0;
    node->data = calloc(PAGE_SIZE, sizeof (SM_PageHandle));
    node->next = NULL;
    node->prev = NULL;
    return node;
}


RC initBufferPool(BM_BufferPool * const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData) {

    SM_FileHandle fHandle;
    if (openPageFile((char*) pageFileName, &fHandle) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    struct DLnode *new, *temp;
    new = createNewNode(); // create doublylinked list node and assign values 
    struct queuePool *queuePool = malloc(sizeof (struct queuePool)); //allocate memory to buffer pool
    queuePool->occupiedFrames = 0;
    queuePool->totalNumFrames = numPages;
    queuePool->numRead = 0;
    queuePool->numWrite = 0;
    queuePool->front = queuePool->rear = new;
    int i;
    for (i = 1; i < numPages; i++) {  //creating frames inside buffer pool
        temp = createNewNode();
        queuePool->rear->next = temp;
        queuePool->rear->next->prev = queuePool->rear;
        queuePool->rear = queuePool->rear->next;
        queuePool->rear->frameNum = i;
    }

    struct hash *hashTable = createHashTable(numPages); //creating hash table for storing the page numbers and the address of frames

    bm->mgmtData = queuePool;
    bm->pageTableData = hashTable;
    bm->strategy = strategy;
    bm->numPages = numPages;
    bm->pageFile = (char*) pageFileName;
    closePageFile(&fHandle);
    return RC_OK;
}


RC pinPage(BM_BufferPool * const bm, BM_PageHandle * const page, const PageNumber pageNum) {

    if (bm->strategy == RS_LRU) {
        return pinPageWithLRU(bm, page, pageNum);
    } else if (bm->strategy == RS_FIFO) {
        return pinPageWithFIFO(bm, page, pageNum);
    } else {
        return NO_SUCH_METHOD;
    }

}


RC markDirty(BM_BufferPool * const bm, BM_PageHandle * const page) {

    struct queuePool *queuePool = bm->mgmtData;
    struct DLnode *temp = findTheNode(queuePool, page->pageNum); //finding the address of the node in the buffer pool
    if (temp != NULL) {
        temp->dirty = 1;  //marking page as dirty of a frame node
        bm->mgmtData = queuePool;
        return RC_OK;
    }
    return PAGE_NODE_NOT_FOUND;
}


RC unpinPage(BM_BufferPool * const bm, BM_PageHandle * const page) {

    struct queuePool *queuePool = bm->mgmtData;

    struct DLnode *temp = findTheNode(queuePool, page->pageNum);
    if (temp != NULL && temp->fixcount > 0) {
        temp->fixcount = temp->fixcount - 1; //once the page is unpinned we are decrementing the fix count
        bm->mgmtData = queuePool;
        return RC_OK;
    }
    return PAGE_NODE_NOT_FOUND;
}

//forcePage should write the current content of the page back to the page file on disk.
RC forcePage(BM_BufferPool * const bm, BM_PageHandle * const page) {

    struct queuePool *queuePool = bm->mgmtData;
    SM_FileHandle fhandle;
    if (openPageFile((char *) (bm->pageFile), &fhandle) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    struct DLnode *temp = findTheNode(queuePool, page->pageNum);
    if (temp != NULL) {
        if (writeBlock(temp->pageNumber, &fhandle, temp->data) != RC_OK) { //writing the data of the node back to the disk
            return RC_WRITE_FAILED; 
        }
    } else {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }
    queuePool->numWrite = queuePool->numWrite + 1; 
    bm->mgmtData = queuePool;
    closePageFile(&fhandle);
    return RC_OK;
}



RC shutdownBufferPool(BM_BufferPool * const bm) { 
    forceFlushPool(bm);
    struct queuePool *queuePool = bm->mgmtData;
    struct DLnode *curr = queuePool->front;
    while (curr != NULL) {
        curr = curr->next;
        free(queuePool->front->data);
        free(queuePool->front); //freeing the node and its data from the buffer pool
        queuePool->front = curr;
    }
    queuePool->front = queuePool->rear = NULL;
    free(queuePool);
    bm->numPages = 0; //setting the no of pages of a buffer to zero
    return RC_OK;
}


RC forceFlushPool(BM_BufferPool * const bm) { //forcing the data to be written on the disk
    struct queuePool * queuePool = bm->mgmtData;
    struct DLnode *curr = queuePool->front;
    SM_FileHandle fhandle;
    int success = openPageFile((char *) (bm->pageFile), &fhandle);
    if (success != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    while (curr != NULL) {
        if (curr->dirty == 1) {
            writeBlock(curr->pageNumber, &fhandle, curr->data); //if page is dirty its writing to the disk
            curr->dirty = 0;
            queuePool->numWrite++;
        }
        curr = curr->next; //does the forceflushpool for all the data in the buffer
    }
    closePageFile(&fhandle);
    return RC_OK;
}


PageNumber *getFrameContents(BM_BufferPool * const bm) {
    return 0;
}


bool *getDirtyFlags(BM_BufferPool *const bm) { //will return an array which will give the dirty bit value of each page
    struct queuePool *queue = bm->mgmtData;
    struct DLnode *curr = queue->front;
    while (curr != NULL) {
        queue->dirtyBitArray[curr->frameNum] = curr->dirty;
        curr = curr->next;
    }
    return queue->dirtyBitArray;
}


int *getFixCounts(BM_BufferPool * const bm) { //will return an array which will give the fix count of each page
    struct queuePool *queue = bm->mgmtData;
    struct DLnode *temp = queue->front;
    while (temp != NULL) {
        queue->fixCountArray[temp->frameNum] = temp->fixcount;
        temp = temp->next;
    }
    return queue->fixCountArray;
}


int getNumReadIO(BM_BufferPool * const bm) { //will return the number of reads done
    struct queuePool *queue = bm->mgmtData;
    return queue->numRead;
}


int getNumWriteIO(BM_BufferPool * const bm) { //will return the number of writes done
    struct queuePool *queue = bm->mgmtData;
    return queue->numWrite;
}









