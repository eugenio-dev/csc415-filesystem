/**************************************************************
* Class::  CSC-415-03 Spring 2025
* Name:: Juan Ramirez & Ranjiv Jithendran 
* Student IDs::
* GitHub-Name:: Tybo2020
* Group-Name:: The Ducklings
* Project:: Basic File System
*
* File:: vcb.h
*
* Description:: Interface for the volume control block
*
**************************************************************/

#ifndef VCB_H
#define VCB_H

#include <stdint.h>

#define BLOCK_SIZE 512
#define MAGIC_NUMBER 0xDEADBEEF

typedef struct VolumeControlBlock {
    unsigned long signature;         // Used to check formatting
    uint32_t volumeSize;             // Total number of blocks
    uint32_t totalBlocks;            // Total usable blocks
    uint32_t rootDirBlock;           // Starting block of root directory
    uint32_t freeSpaceStartBlock;    // Start of free space/FAT (optional)
    uint32_t freeSpaceHead;          // Head of free space linked list
    uint32_t freeSpaceSize;          // Size of the free space table
} VolumeControlBlock;

extern VolumeControlBlock* vcb;

#endif
