/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name:: Juan Ramirez & Ranjiv Jithendran 
* Student IDs::
* GitHub-Name:: Tybo2020
* Group-Name:: The Ducklings
* Project:: Basic File System
*
* File:: fsInit.c
*
* Description:: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include "fsLow.h"
#include "vcb.h"
#include "freeSpace.h"
#include "createDirectory.h"
#include "mfs.h"

VolumeControlBlock* vcb = NULL;

int initFileSystem(uint64_t numberOfBlocks, uint64_t blockSize)
{

    vcb = malloc(blockSize);
    if (!vcb) {
        fprintf(stderr, "VCB malloc failed\n");
        return -1;
    }

    if (LBAread(vcb, 1, 0) != 1) {
        fprintf(stderr, "Error reading block 0\n");
        return -1;
    }

    if (vcb->signature != MAGIC_NUMBER) {

        vcb->signature = MAGIC_NUMBER;
        vcb->volumeSize = numberOfBlocks;
        vcb->totalBlocks = numberOfBlocks;

        initFreeSpace(numberOfBlocks, blockSize);

        int rootBlock = createDirectory(50, NULL);
        if (rootBlock <= 0) {
            fprintf(stderr, "Error creating root directory\n");
            return -1;
        }

        // Update VCB with root directory location
        vcb->rootDirBlock = rootBlock;
        
        // For Debugging Purposes
        // printVolumeControlBlock(vcb);

        if (LBAwrite(vcb, 1, 0) != 1) {
            fprintf(stderr, "Error writing final VCB\n");
            return -1;
        }

        printf("Volume formatted successfully!\n");
    } else {
        printf("Volume already formatted.\n");
    }

   // Whether new or existing volume, load the root directory into memory
    int rootDirSize = MAX_ENTRIES * sizeof(DirectoryEntry);
    int rootDirBlocks = (rootDirSize + blockSize - 1) / blockSize;
    
    // Allocate memory for root directory
    rootDirectory = (DirectoryEntry*)malloc(rootDirSize);
    if (!rootDirectory) {
        fprintf(stderr, "Failed to allocate memory for root directory\n");
        return -1;
    }
    
    // Read root directory from disk
    if (LBAread(rootDirectory, rootDirBlocks, vcb->rootDirBlock) != rootDirBlocks) {
        fprintf(stderr, "Failed to read root directory\n");
        free(rootDirectory);
        rootDirectory = NULL;
        return -1;
    }
    
    // Set current working directory to root
    currentWorkingDirectory = rootDirectory;
    
    return 0;
}


void printVolumeControlBlock(VolumeControlBlock* vcb) {
    if (vcb == NULL) {
        printf("Error: VCB is NULL\n");
        return;
    }

    printf("Volume Control Block Details:\n");
    printf("-----------------------------\n");
    printf("Signature:             0x%lX\n", vcb->signature);
    printf("Volume Size:           %u blocks\n", vcb->volumeSize);
    printf("Total Blocks:          %u blocks\n", vcb->totalBlocks);
    printf("Root Directory Block:  %u\n", vcb->rootDirBlock);
    printf("Free Space Start:      %u\n", vcb->freeSpaceStartBlock);
    printf("Free Space Head:       %u\n", vcb->freeSpaceHead);
    printf("Free Space Size:       %u blocks\n", vcb->freeSpaceSize);
    printf("-----------------------------\n");
}

void exitFileSystem()
{
    printf("System exiting.\n");
    if (vcb) {
        free(vcb);
        vcb = NULL;
    }

     // prevent double-free if CWD and root are same
    if (currentWorkingDirectory && currentWorkingDirectory != rootDirectory) {
        free(currentWorkingDirectory);
        currentWorkingDirectory = NULL;
    }

    if (rootDirectory) {
        free(rootDirectory);
        rootDirectory = NULL;
    }
}
