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
* Description:: 
*
**************************************************************/

#ifndef FREE_SPACE_H
#define FREE_SPACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "fsLow.h"
#include "vcb.h"

// Constants to help with managing the free space
#define BLOCK_FREE 0x00000000       
#define BLOCK_RESERVED 0xFFFFFFFE    

typedef uint32_t FATEntry; 

void initFreeSpace(int blockCount, int blockSize);
int allocateBlocks(int numOfBlocks);
int extendChain(int headOfChain, int amountToChange);
int releaseBlocks(int location, int numOfBlocks);

// Helper Functions 
bool isBlockFree(int blockNum);

FATEntry readFATEntry(int blockNum);
bool writeFATEntry(int blockNum, FATEntry entry);
uint64_t getFATEntryPos(int blockNum);


#endif