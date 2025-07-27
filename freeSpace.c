/**************************************************************
* Class::  CSC-415-03 Spring 2025
* Name:: Ty Bohlander
* Student IDs::
* GitHub-Name:: Tybo2020
* Group-Name:: The Ducklings
* Project:: Basic File System
*
* File:: free_space.c
*
* Description:: Interface for free space management
*
**************************************************************/
#include "freeSpace.h"
#include <string.h>
#include <stdio.h>


void initFreeSpace(int blockCount, int blockSize) {
    int entriesPerBlock = blockSize / sizeof(FATEntry);
    int blocksNeeded = 6;

    // Allocate buffer for one FAT entry block
    FATEntry* fatBuffer = (FATEntry*)malloc(blockSize);
    if (!fatBuffer) {
        fprintf(stderr, "Error: Failed to allocate memory for FAT buffer\n");
        return;
    }

    vcb->freeSpaceStartBlock = 1; 
    vcb->freeSpaceSize = blocksNeeded;
    
    // Find first block after VCB and FAT
    int firstFreeBlock = vcb->freeSpaceStartBlock + blocksNeeded;

    // Mark blocks as either reserved or free
    for (int fatBlockIndex = 0; fatBlockIndex < blocksNeeded; fatBlockIndex++) {
        // Clear the buffer
        memset(fatBuffer, 0, blockSize);
        
        // For each entry in this FAT block
        for (int i = 0; i < entriesPerBlock; i++) {
            int blockNum = fatBlockIndex * entriesPerBlock + i;
            
            if (blockNum >= blockCount) {
                break; // Beyond the end of the volume
            }
            
            if (blockNum < firstFreeBlock) {
                fatBuffer[i] = BLOCK_RESERVED; // System blocks are reserved
            } else {
                fatBuffer[i] = BLOCK_FREE; // All other blocks are free
            }
        }
        
        // Write FAT block to disk
        uint64_t lbaPosition = vcb->freeSpaceStartBlock + fatBlockIndex;
        if (LBAwrite(fatBuffer, 1, lbaPosition) != 1) {
            fprintf(stderr, "Error: Failed to write FAT block %d\n", fatBlockIndex);
            free(fatBuffer);
            return;
        }
    }
    
    // Set the head of the free list to the first free block
    vcb->freeSpaceHead = firstFreeBlock;
    
    free(fatBuffer);
    
    // Write the updated VCB to disk
    LBAwrite(vcb, 1, 0);
    printf("Free space initialized. First free block: %d\n", firstFreeBlock);
}


int allocateBlocks(int numOfBlocks) {
    if (numOfBlocks <= 0) {
        printf("Error: Invalid number of blocks requested: %d\n", numOfBlocks);
        return -1;
    }
    
    printf("Allocating %d blocks, starting search at block %d\n", 
           numOfBlocks, vcb->freeSpaceHead);
    
    // Initialize tracking variables
    int startBlock = -1;
    int prevBlock = -1;
    int blocksAllocated = 0;
    
    // Start searching from the free space head
    int currentBlock = vcb->freeSpaceHead;
    
    // Scan through all blocks looking for free ones
    while (blocksAllocated < numOfBlocks && currentBlock < vcb->totalBlocks) {
        // Check if this block is free
        FATEntry entry = readFATEntry(currentBlock);
        if (entry == BLOCK_FREE) {
            //printf("Allocating block %d\n", currentBlock);
            
            // If this is our first block, save it as the start
            if (startBlock == -1) {
                startBlock = currentBlock;
            }
            
            // If we have a previous block, link it to this one
            if (prevBlock != -1) {
                if (!writeFATEntry(prevBlock, currentBlock)) {
                    printf("Error: Failed to link block %d to %d\n", prevBlock, currentBlock);
                    // Clean up and return error
                    releaseBlocks(startBlock, blocksAllocated);
                    return -1;
                }
            }
            
            prevBlock = currentBlock;
            blocksAllocated++;
        }
        
        // Move to the next block
        currentBlock++;
    }
    
    // Check if we got all the blocks we needed
    if (blocksAllocated < numOfBlocks) {
        printf("Error: Could only allocate %d of %d blocks\n", 
               blocksAllocated, numOfBlocks);
        if (startBlock != -1) {
            releaseBlocks(startBlock, blocksAllocated);
        }
        return -1;
    }
    
    // Mark the last block as end of chain
    if (!writeFATEntry(prevBlock, BLOCK_RESERVED)) {
        printf("Error: Failed to mark end of chain at block %d\n", prevBlock);
        releaseBlocks(startBlock, blocksAllocated);
        return -1;
    }
    
    // Update the free space head to speed up future allocations
    vcb->freeSpaceHead = currentBlock; 
    
    // Update VCB on disk
    if (LBAwrite(vcb, 1, 0) != 1) {
        printf("Error: Failed to update VCB after allocation\n");
    }
    
    printf("Successfully allocated %d blocks starting at block %d\n", 
           numOfBlocks, startBlock);
    return startBlock;
}

int extendChain(int headOfChain, int amountToChange){
    int currentBlock = headOfChain; 
    int lastBlock = -1; 

    while(currentBlock != BLOCK_RESERVED){
        lastBlock = currentBlock;
        // Read next block in chain 
        currentBlock = readFATEntry(currentBlock);

        // Prevent infinite loop
        if(currentBlock == lastBlock){
            break;
        }
    }

    int newBlocksHead = allocateBlocks(amountToChange);

    if (newBlocksHead == -1){
        // Failed to allocate new blocks 
        return -1; 
    }   

    // Link the last block of existing chain to the head of the new chain
    if (!writeFATEntry(lastBlock, newBlocksHead)) {

        // If linking fails, release the newly allocated blocks back to the free list
        releaseBlocks(newBlocksHead, amountToChange);
        return -1; 
    }
    return headOfChain;
}
int releaseBlocks(int location, int numOfBlocks){
    int blocksReleased = 0;
    int currentBlock = location;
    int nextBlock; 

    int currentFreeSpaceHead = vcb->freeSpaceHead;

    while(currentBlock != BLOCK_RESERVED && blocksReleased < numOfBlocks){
        // Find next block in chain
        nextBlock = readFATEntry(currentBlock);

        if(!writeFATEntry(currentBlock, currentFreeSpaceHead)){
            vcb->freeSpaceHead = location;
            LBAwrite(vcb, 1, 0);
            return blocksReleased;
        }

        // This block becomes the new head of the free list
        currentFreeSpaceHead = currentBlock;
        
        // Move to the next block
        currentBlock = nextBlock;
        blocksReleased++;
    }
    
    // Update the free space head in the VCB
    vcb->freeSpaceHead = location;
    
    // Write the updated VCB to disk
    LBAwrite(vcb, 1, 0);
    
    return blocksReleased;
}

bool isBlockFree(int blockNum) {
    // Ensure the block number is valid
    if (blockNum <= 0 || blockNum >= vcb->totalBlocks) {
        return false;
    }
    
    FATEntry entry = readFATEntry(blockNum);
    
    return (entry == BLOCK_FREE);
}

// Retrieves the next block in the chain
FATEntry readFATEntry(int blockNum) {
    // Validate block number
    if (blockNum < 0 || blockNum >= vcb->totalBlocks) {
        return BLOCK_RESERVED; // Return reserved as an error indicator
    }
    
    // Calculate which FAT block contains this entry
    uint64_t fatPosition = getFATEntryPos(blockNum);
    int fatBlockNum = fatPosition / BLOCK_SIZE;
    int fatOffset = fatPosition % BLOCK_SIZE;
    
    // Allocate buffer for reading the FAT block
    FATEntry* fatBuffer = (FATEntry*)malloc(BLOCK_SIZE);
    if (fatBuffer == NULL) {
        return BLOCK_RESERVED;
    }
    
    // Read the FAT block from disk
    uint64_t lbaPosition = vcb->freeSpaceStartBlock + fatBlockNum;
    if (LBAread(fatBuffer, 1, lbaPosition) != 1) {
        free(fatBuffer);
        return BLOCK_RESERVED;
    }
    
    // Calculate the index within the buffer
    int entryIndex = fatOffset / sizeof(FATEntry);
    
    FATEntry entry = fatBuffer[entryIndex];
    free(fatBuffer);
    
    return entry;
}  

// Used to update block pointers when allocating, freeing, or extending a chain
bool writeFATEntry(int blockNum, FATEntry entry) {
    // Validate block number
    if (blockNum < 0 || blockNum >= vcb->totalBlocks) {
        return false;
    }
    
    // Calculate which FAT block contains this entry
    uint64_t fatPosition = getFATEntryPos(blockNum);
    int fatBlockNum = fatPosition / BLOCK_SIZE;
    int fatOffset = fatPosition % BLOCK_SIZE;
    
    // Allocate buffer for reading/writing the FAT block
    FATEntry* fatBuffer = (FATEntry*)malloc(BLOCK_SIZE);
    if (fatBuffer == NULL) {
        return false;
    }
    
    // Read the FAT block from disk
    uint64_t lbaPosition = vcb->freeSpaceStartBlock + fatBlockNum;
    if (LBAread(fatBuffer, 1, lbaPosition) != 1) {
        free(fatBuffer);
        return false;
    }
    
    // Calculate the index within the buffer
    int entryIndex = fatOffset / sizeof(FATEntry);
    
    // Update the entry
    fatBuffer[entryIndex] = entry;
    
    // Write the updated block back to disk
    if (LBAwrite(fatBuffer, 1, lbaPosition) != 1) {
        free(fatBuffer);
        return false;
    }
    
    // Free the buffer
    free(fatBuffer);
    
    return true;
}
uint64_t getFATEntryPos(int blockNum){

    // Calculate the byte position for this entry in the FAT
    return blockNum * sizeof(FATEntry);
}
