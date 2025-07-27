/**************************************************************
* Class::  CSC-415-03 Spring 2025
* Name::
* Student IDs:
* GitHub-Name:
* Group-Name: The Ducklings
* Project:: Basic File System
*
* File:: vcb.c
*
* Description:: 
*
**************************************************************/

#include "vcb.h"
#include "freeSpace.h"
#include "createDirectory.h"

void initializeVCB(VolumeControlBlock *vcbPointer, unsigned long signature, uint16_t numBlocks, uint16_t BlockSize){
    vcbPointer->signature = signature;
    vcbPointer->volumeSize = numBlocks;
    vcbPointer->totalBlocks = 0;
    vcbPointer->rootDirBlock = createDirectory(numBlocks, NULL);
    vcbPointer->freeSpaceStartBlock = allocateBlocks(numBlocks);
    initFreeSpace(numBlocks, BlockSize);
}

