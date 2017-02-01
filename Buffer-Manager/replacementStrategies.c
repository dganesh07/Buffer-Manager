

#include "dberror.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "storage_mgr.h"
#include "buffer_mgr.h"

#define MAX_CAPACITY 200;

struct DLnode {
    int pageNumber;
    int frameNum;
    int fixcount;
    int dirty;
    char *data;
    struct DLnode *next, *prev;
};

struct queuePool {
    int occupiedFrames;
    int totalNumFrames;
    struct DLnode *front, *rear;
    char *data;
    int numRead;
    int numWrite;
    bool *dirtyBitArray;
    int *fixCountArray;

};

struct hash {
    int capacity;
    struct DLnode * *pageTable;
};

/**********************************************************************************
 * Function Name: checkSpaceAvailable
 *
 * Description:
 *      checkSpaceAvailable function is used to check the space in the buffer
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Devika beniwal
 *
 *
 *
 *
 ***********************************************************************************/

int checkSpaceAvailable(struct queuePool *queuePool) {
    if (queuePool->occupiedFrames == queuePool->totalNumFrames) {
        return 0;
    } else {
        return 1;
    }
}

/**********************************************************************************
 * Function Name: createnode
 *
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Devika beniwal
 *
 *
 *
 *
 ***********************************************************************************/
struct DLnode * createnode(const PageNumber pageNum) {

    struct DLnode *newnode = (struct DLnode *) malloc(sizeof (struct DLnode *));
    newnode->next = NULL;
    newnode->prev = NULL;
    newnode->pageNumber = pageNum;
    newnode->frameNum = 0;
    newnode->fixcount = 1;
    newnode->dirty = 0;
    newnode->data = calloc(PAGE_SIZE, sizeof (SM_PageHandle));
    return newnode;
};

/**********************************************************************************
 * Function Name: findTheNode
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Devika beniwal
 *
 *
 *
 *
 ***********************************************************************************/

struct DLnode * findTheNode(struct queuePool *queuePool, const PageNumber pageNum) {

    struct DLnode *temp = queuePool->front;

    while (temp != NULL) {
        if (temp->pageNumber == pageNum) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

/**********************************************************************************
 * Function Name: moveNodeToFront
 *
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Devika beniwal
 *
 *
 *
 *
 ***********************************************************************************/
void moveNodeToFront(struct DLnode *reqPage, struct queuePool **queuePool) {
    struct DLnode* prevnode = reqPage->prev;
    struct DLnode* nextnode = reqPage->next;

    if (reqPage == (*queuePool)->front) {
        return;
    }

    reqPage->prev->next = nextnode;
    if (reqPage->next) {
        reqPage->next->prev = prevnode;
    }

    if (reqPage == (*queuePool)->rear) {
        (*queuePool)->rear = prevnode;
        (*queuePool)->rear->next = NULL;
    }
    reqPage->next = (*queuePool)->front;
    reqPage->prev = NULL;
    reqPage->next->prev = reqPage;
    (*queuePool)->front = reqPage;

}

/**********************************************************************************
 * Function Name: changeDLnodeContent
 *
 * Description:
 *      will update the node with the new requested data 
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Devika beniwal
 *
 *
 *
 *
 ***********************************************************************************/

RC changeDLnodeContent(BM_BufferPool * const bm, struct DLnode *reqPage, BM_PageHandle * const page, const PageNumber pageNum) { //will update the content of each node 

    struct queuePool *queuePool = bm->mgmtData;
    struct hash *hash = bm->pageTableData;
    SM_FileHandle fhandle;

    if (openPageFile((char *) (bm->pageFile), &fhandle) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }
    if (reqPage->dirty == 1) {
        if ((ensureCapacity(pageNum, &fhandle)) != RC_OK) {
            return RC_ENSURE_CAP_ERROR;
        }
        if ((writeBlock(reqPage->pageNumber, &fhandle, reqPage->data)) != RC_OK) { //when page is dirty writing the contents back to the disk
            return RC_WRITE_FAILED;
        }
        queuePool->numWrite++;
    }

    hash->pageTable[pageNum] = reqPage; //updating the address of the page number in the hash table

    if ((ensureCapacity(pageNum, &fhandle)) != RC_OK) {
        return RC_ENSURE_CAP_ERROR;
    }

    if ((readBlock(pageNum, &fhandle, reqPage->data)) != RC_OK) { //read the block from disk
        return RC_READ_NON_EXISTING_PAGE;
    }

    queuePool->numRead++;
    reqPage->dirty = 0;
    reqPage->fixcount++;
    reqPage->pageNumber = pageNum;
    page->pageNum = pageNum;
    page->data = reqPage->data;

    closePageFile(&fhandle);
    return RC_OK;

}

/**********************************************************************************
 * Function Name: pinPageWithFIFO
 *
 * Description:
 *      will execute the first in first out page replacement method
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Damini Ganesh
 *
 *
 *
 *
 ***********************************************************************************/

RC pinPageWithFIFO(BM_BufferPool * const bm, BM_PageHandle * const page, const PageNumber pageNum) {
    struct DLnode *reqPage;
    struct queuePool *queuePool = bm->mgmtData;
    struct hash *hash = bm->pageTableData;
    reqPage = hash->pageTable[pageNum]; //check if the page is in the hash table or not!

    if (reqPage != NULL) {
        reqPage = findTheNode(queuePool, pageNum); //if exist find the address of the node
        if (reqPage != NULL) {
            page->pageNum = pageNum;
            page->data = reqPage->data;
            reqPage->fixcount++;
            return RC_OK;
        }
    } else {
        if (checkSpaceAvailable(queuePool)) { //if occupied frames are less than the total number of frames ->space exist
            reqPage = queuePool->front;
            int i = 0;
            while (i < queuePool->occupiedFrames) {
                reqPage = reqPage->next;
                i++;
            }
            queuePool->occupiedFrames++;
        } else {
            reqPage = queuePool->rear;
            while (reqPage != NULL && reqPage->fixcount != 0) {
                reqPage = reqPage->prev;
            }
        }
        moveNodeToFront(reqPage, &queuePool);
        if (changeDLnodeContent(bm, reqPage, page, pageNum) != RC_OK) { //update the content of the node
            return UPDATE_FRAME_ISSUE;
        }
        bm->mgmtData = queuePool;
        bm->pageTableData = hash;
        return RC_OK;
    }
    return RC_OK;
}

/**********************************************************************************
 * Function Name: pinPageWithLRU
 *
 * Description:
 *      will execute the least recently used page replacement method
 *
 * Return:
 *      RC Name                      Value                   Comment:
 *      RC_OK                               0                        Process successful
 *
 * Author: Devika beniwal
 *
 *
 *
 *
 ***********************************************************************************/

int pinPageWithLRU(BM_BufferPool * const bm, BM_PageHandle * const page, const PageNumber pageNum) {

    struct DLnode* reqPage;
    struct queuePool *queuePool = bm->mgmtData;
    struct hash *hash = bm->pageTableData;
    reqPage = hash->pageTable[pageNum]; //check if the page is in the hash table or not

    if (reqPage != NULL) {
        reqPage = findTheNode(queuePool, pageNum); //if exist find the address of the node
        if (reqPage != NULL) {
            page->pageNum = pageNum;
            page->data = reqPage->data;
            reqPage->fixcount++;
            moveNodeToFront(reqPage, &queuePool); //the node whose contents have been changed is moved to the front
            return RC_OK;
        } else {
            return PAGE_NODE_NOT_FOUND;
        }
    } else {
        if (checkSpaceAvailable(queuePool)) { //if occupied frames are less than the total number of frames ->space exist
            reqPage = queuePool->front;
            int i = 0;
            while (i < queuePool->occupiedFrames) {
                reqPage = reqPage->next;
                i++;
            }
            queuePool->occupiedFrames++;
        } else { //replace the least recently used page 

            int selectedPage;
            reqPage = queuePool->rear;
            while (reqPage != NULL && reqPage->fixcount != 0) {
                reqPage = reqPage->prev;
            }
            selectedPage = reqPage->pageNumber;
            hash->pageTable[selectedPage] = NULL;
            if (reqPage == NULL) {
                return PAGE_NODE_NOT_FOUND;
            }
        }

        if (changeDLnodeContent(bm, reqPage, page, pageNum) != RC_OK) { //update the content of the node 
            return UPDATE_FRAME_ISSUE;
        }

        if (reqPage != queuePool->front) {
            moveNodeToFront(reqPage, &queuePool); //the node whose contents have been changed is moved to the front
        } else {
            queuePool->front = queuePool->rear = reqPage;

        }
        bm->mgmtData = queuePool;
        bm->pageTableData = hash;

        return RC_OK;
    }
    return RC_OK;
}
