/**************************************************************
* Class::  CSC-415-0# Spring 2024
* Name::
* Student IDs::
* GitHub-Name::
* Group-Name::
* Project:: Basic File System
*
* File:: b_io.c
*
* Description:: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "createDirectory.h"
#include "mfs.h"
#include "freeSpace.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		//holds the open file buffer
	int index;		//holds the current position in the buffer
	int buflen;		//holds how many valid bytes are in the buffer
	int currentBlk; //holds the current block number
	int numBlocks; //hold how many blocks file occupies
	DirectoryEntry * de; //holds the low level systems file info
    int flags; 
	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
b_io_fd b_open(char * filename, int flags)
{
    b_io_fd returnFd;
    
    if (startup == 0) b_init();  //Initialize our system

    if (filename == NULL || flags < 0){
        return -1; 
    }

    returnFd = b_getFCB();              // get our own file descriptor
    if (returnFd == -1){                // check for error - all used FCB's
        printf("no free slots available \n");
        return -1; 
    }

    // Duplicate filename to avoid modifying original 
    char *currentPathname = malloc(strlen(filename) + 1); 
    if (currentPathname == NULL){
        printf("malloc failed for pathname \n"); 
        return -1; 
    }

    strcpy(currentPathname, filename);

    // Parse pathname 
    ppinfo *info = malloc(sizeof(ppinfo));
    if (info == NULL) {
        free(currentPathname);
        fprintf(stderr, "malloc failed for path info \n");
        return -1;
    }

    int parseResult = parsePath(currentPathname, info);
    DirectoryEntry *fi = NULL;
    
    if (parseResult == -1) {
        free(info);
        free(currentPathname);
        return -1;
    }
    
    if (parseResult == -2 && !(flags & O_CREAT)) {
        free(info);
        free(currentPathname);
        return -1;
    }

    // File exists
    if (info->index >= 0){
        fi = &(info->parent[info->index]);

        if (fi->isDir){
            free(info);
            free(currentPathname);
            printf("cannot open a dir \n");
            return -1;
        }

        // Handle O_TRUNC flag - reset file size if file exists and O_TRUNC is set
        if (flags & O_TRUNC) {
            fi->fileSize = 0;
            LBAwrite(info->parent, (info->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE, info->parent[0].startBlock);
        }
    }
    // File does not exist
    else if (info->index == -1 || parseResult == -2) {
        if (flags & O_CREAT) {
            // Find a free entry in the parent directory
            int freeIndex = -1;
            for (int i = 0; i < MAX_ENTRIES; i++) {
                if (!info->parent[i].inUse) {
                    freeIndex = i;
                    break;
                }
            }
            if (freeIndex == -1) {
                free(info);
                free(currentPathname);
                return -1; // No free entry in directory
            }

            // Create a new file entry
            fi = &(info->parent[freeIndex]);
            memset(fi, 0, sizeof(DirectoryEntry));
            strncpy(fi->fileName, info->lastElementName, sizeof(fi->fileName) - 1);
            fi->isDir = false;
            fi->fileSize = 0;
            fi->permissions = 0777; // Add default permissions
            fi->startBlock = allocateBlocks(1);  // allocate at least 1 block
            if (fi->startBlock == 0) {
                // Failed to allocate blocks
                free(info);
                free(currentPathname);
                return -1;
            }
            fi->inUse = true;
            fi->createdTime = fi->lastModified = fi->lastAccessed = time(NULL);

            // Update parent directory on disk
            LBAwrite(info->parent, (info->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE, info->parent[0].startBlock);
        } else {
            // Cannot open non-existent file if O_CREAT is not set
            free(info);
            free(currentPathname);
            return -1;
        }
    }

    // Allocate and set up the buffer
    fcbArray[returnFd].buf = malloc(B_CHUNK_SIZE);
    if (fcbArray[returnFd].buf == NULL) {
        free(info);
        free(currentPathname);
        fprintf(stderr, "malloc for buffer failed\n");
        return -1;
    }
    
    // Initialize buffer variables
    fcbArray[returnFd].index = 0;
    fcbArray[returnFd].currentBlk = 0;
    fcbArray[returnFd].numBlocks = (fi->fileSize + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE;  // Calculate blocks needed
    fcbArray[returnFd].de = fi;
    fcbArray[returnFd].flags = flags;
    
    // For read operations, load the first block if file is not empty
    if ((flags & O_ACCMODE) != O_WRONLY && fi->fileSize > 0) {
        LBAread(fcbArray[returnFd].buf, 1, fi->startBlock);
        // Set buflen to either block size or file size, whichever is smaller
        fcbArray[returnFd].buflen = (fi->fileSize < B_CHUNK_SIZE) ? fi->fileSize : B_CHUNK_SIZE;
    } else {
        // Empty buffer for write-only or empty files
        fcbArray[returnFd].buflen = 0;
    }
    
    // Handle O_APPEND flag - position at end of file
    if (flags & O_APPEND) {
        fcbArray[returnFd].currentBlk = fi->fileSize / B_CHUNK_SIZE;
        fcbArray[returnFd].index = fi->fileSize % B_CHUNK_SIZE;
        
        // If appending and not at the start of a block, load the current block
        if (fcbArray[returnFd].index > 0 && (flags & O_ACCMODE) != O_RDONLY) {
            LBAread(fcbArray[returnFd].buf, 1, fi->startBlock + fcbArray[returnFd].currentBlk);
        }
    }
    
    // Update access time
    fi->lastAccessed = time(NULL);
    
    // Clean up temporary allocations
    free(info);
    free(currentPathname); 
    
    return (returnFd);  // Return the file descriptor
}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
	// Check if this FCB is actually in use
    if (fcbArray[fd].buf == NULL)
    {
        return (-1);             // FCB not in use
    }
    
    // Get directory entry from FCB
    DirectoryEntry* de = fcbArray[fd].de;
    if (de == NULL)
    {
        return (-1);             // No directory entry available
    }
    
    // Calculate current absolute position and file size
    int currentPos = (fcbArray[fd].currentBlk * B_CHUNK_SIZE) + fcbArray[fd].index;
    int fileSize = de->fileSize;  // From the directory entry
    
    // Calculate new position based on whence
    int newPos = 0;
    switch (whence)
    {
        case SEEK_SET:  // From beginning of file
            newPos = offset;
            break;
            
        case SEEK_CUR:  // From current position
            newPos = currentPos + offset;
            break;
            
        case SEEK_END:  // From end of file
            newPos = fileSize + offset;  
            break;
            
        default:
            return (-1);  
    }
    
    // Validate the new position
    if (newPos < 0)
    {
        return (-1);  // Cannot seek before start of file
    }
    
    if (newPos > fileSize)
    {   
        newPos = fileSize;
    }
    
    // Calculate new block number and position within that block
    int newBlockNum = newPos / B_CHUNK_SIZE;
    int newIndex = newPos % B_CHUNK_SIZE;
    
    // If we're moving to a different block, we need to load that block
    if (newBlockNum != fcbArray[fd].currentBlk)
    {
        // Assumption: block loading/saving will be handled in b_read/b_write
        // Update the current block in the FCB
        fcbArray[fd].currentBlk = newBlockNum;
        
        // Assumption: buffer management will be handled in b_read/b_write
        // For a complete implementation, we might need to load the new block here
    }
    
    // Update the position within the buffer
    fcbArray[fd].index = newIndex;
    
    // Return the new absolute position
    return newPos;	
	
	}


int b_write(b_io_fd fd, char *buffer, int count) {
    if (startup == 0) b_init();  // Initialize our system
    
    // Check that fd is between 0 and (MAXFCBS-1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        return (-1);  // Invalid file descriptor
    }
    
    // Check if file is open
    if (fcbArray[fd].de == NULL) {
        return -1;  // File not open
    }
    
    // Check write permission using proper flag masking
    if ((fcbArray[fd].flags & O_ACCMODE) == O_RDONLY) {
        return -1;  // File not opened for writing
    }
    
    // If nothing to write, return 0
    if (count <= 0) {
        return 0;
    }
    
    int bytesWritten = 0;      // Bytes written so far
    int bytesToWrite = count;  // Bytes remaining to write
    int currentPos = 0;        // Current position in buffer
    
    // Process the data in smaller chunks to avoid buffer issues
    while (bytesToWrite > 0) {
        // If there's space in the current buffer, use it
        if (fcbArray[fd].index < B_CHUNK_SIZE) {
            int spaceInBuffer = B_CHUNK_SIZE - fcbArray[fd].index;
            int chunkSize = (bytesToWrite < spaceInBuffer) ? bytesToWrite : spaceInBuffer;
            
            // Copy data to the buffer
            memcpy(fcbArray[fd].buf + fcbArray[fd].index, buffer + currentPos, chunkSize);
            fcbArray[fd].index += chunkSize;
            bytesWritten += chunkSize;
            currentPos += chunkSize;
            bytesToWrite -= chunkSize;
            
            // If buffer is now full, write it to disk
            if (fcbArray[fd].index == B_CHUNK_SIZE) {
                
                LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].de->startBlock + fcbArray[fd].currentBlk);
                fcbArray[fd].index = 0;  // Reset buffer index
                fcbArray[fd].currentBlk++;  // Move to next block
                
            }
        }
        else {
            // This shouldn't happen if the code above is working correctly,
            // but just in case, reset the buffer index
            fcbArray[fd].index = 0;
        }
    }
    
    // Update file size if needed
    int newFileSize = fcbArray[fd].currentBlk * B_CHUNK_SIZE + fcbArray[fd].index;
    
    if (newFileSize > fcbArray[fd].de->fileSize) {
        
        fcbArray[fd].de->fileSize = newFileSize;
        fcbArray[fd].de->lastModified = time(NULL);
        
        // Update the directory entry in the parent directory
        if (fcbArray[fd].de >= rootDirectory && fcbArray[fd].de < rootDirectory + MAX_ENTRIES) {
            // File is in root directory
            int blocksNeeded = (rootDirectory[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
            LBAwrite(rootDirectory, blocksNeeded, rootDirectory[0].startBlock);
        } 
        else if (fcbArray[fd].de >= currentWorkingDirectory && 
                fcbArray[fd].de < currentWorkingDirectory + MAX_ENTRIES) {
            // File is in current directory
            int blocksNeeded = (currentWorkingDirectory[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
            LBAwrite(currentWorkingDirectory, blocksNeeded, currentWorkingDirectory[0].startBlock);
        }
        else {
            //printf("Could not determine parent directory for file\n");
        }
    }
    return bytesWritten;
}

// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
{
   if (startup == 0) b_init();  // Initialize our system

   // Check for valid file descriptor
   if ((fd < 0) || (fd >= MAXFCBS))
   {
       return (-1); // Invalid file descriptor
   }
   
   // Check if file is open
   if (fcbArray[fd].de == NULL)
   {
       return (-1); // File not open
   }
   
   // Check read permission
   if ((fcbArray[fd].flags & O_ACCMODE) == O_WRONLY)
   {
       return (-1); // File not opened for reading
   }
   
   // Calculate current file position and adjust count if it goes beyond EOF
   int currentPosition = (fcbArray[fd].currentBlk * B_CHUNK_SIZE) + fcbArray[fd].index;
   int fileSize = fcbArray[fd].de->fileSize;
   
   // EOF
   if (currentPosition >= fileSize)
   {
       return 0; 
   }
   
   if (currentPosition + count > fileSize)
   {
       count = fileSize - currentPosition; // Adjust count to not read past EOF
   }
   
   // No bytes to read
   if (count <= 0)
   {
       return 0;
   }
   
   int totalBytesRead = 0;    // Total bytes read so far
   int bytesRemaining = count; // Bytes still needed
   
   // Part 1: Read from current buffer if there's data available
   int bytesInBuffer = fcbArray[fd].buflen - fcbArray[fd].index;
   if (bytesInBuffer > 0)
   {
       int part1Size = (bytesInBuffer < bytesRemaining) ? bytesInBuffer : bytesRemaining;
       memcpy(buffer, fcbArray[fd].buf + fcbArray[fd].index, part1Size);
       
       fcbArray[fd].index += part1Size;
       totalBytesRead += part1Size;
       bytesRemaining -= part1Size;
   }
   
   // If we've satisfied the request, return
   if (bytesRemaining == 0)
   {
       return totalBytesRead;
   }
   
   // Part 2: Read whole blocks directly into user's buffer
   int wholeBlocksToRead = bytesRemaining / B_CHUNK_SIZE;
   if (wholeBlocksToRead > 0)
   {
       int blockStart = fcbArray[fd].de->startBlock + fcbArray[fd].currentBlk;
       
       // Read multiple blocks at once if possible
       int blocksRead = LBAread(buffer + totalBytesRead, wholeBlocksToRead, blockStart);
       int bytesReadInBlocks = blocksRead * B_CHUNK_SIZE;
       
       totalBytesRead += bytesReadInBlocks;
       bytesRemaining -= bytesReadInBlocks;
       fcbArray[fd].currentBlk += blocksRead;
   }
   
   // If we've satisfied the request or something went wrong, return
   if (bytesRemaining == 0 || bytesRemaining >= B_CHUNK_SIZE)
   {
       return totalBytesRead;
   }
   
   // Part 3: Read the last partial block into our buffer, then copy to user
   if (bytesRemaining > 0)
   {
       // Load the next block into our buffer
       int blockToRead = fcbArray[fd].de->startBlock + fcbArray[fd].currentBlk;
       int blocksRead = LBAread(fcbArray[fd].buf, 1, blockToRead);
       
       if (blocksRead > 0)
       {
           // Calculate how many bytes we actually read
           fcbArray[fd].buflen = B_CHUNK_SIZE;
           fcbArray[fd].index = 0;
           
           // Make sure we don't read more than what's available
           int part3Size = (bytesRemaining < fcbArray[fd].buflen) ? 
                            bytesRemaining : fcbArray[fd].buflen;
           
           // Copy from our buffer to user's buffer
           memcpy(buffer + totalBytesRead, fcbArray[fd].buf, part3Size);
           
           fcbArray[fd].index = part3Size;
           totalBytesRead += part3Size;
           fcbArray[fd].currentBlk++;
       }
   }
   
   // Update the last accessed time
   fcbArray[fd].de->lastAccessed = time(NULL);
   
   return totalBytesRead;
}
	
// Interface to Close the file	
int b_close (b_io_fd fd)
{
    if (startup == 0) b_init();  // Initialize if needed

    // Validates the file descriptor
    if (fd < 0 || fd >= MAXFCBS) {
        return -1; // Invalid file descriptor
    }

    // Checks if FCB is actually in use
    if (fcbArray[fd].buf == NULL) {
        return -1; // Not an open file
    }

    // If there are unwritten bytes left in the buffer, this will write them to disk
    if (fcbArray[fd].index > 0) {
        LBAwrite(fcbArray[fd].buf, 1, fcbArray[fd].de->startBlock + fcbArray[fd].currentBlk);
    }

    // Free the buffer
    free(fcbArray[fd].buf);
    fcbArray[fd].buf = NULL;

    // Clears the other fields to mark this FCB slot as free
    fcbArray[fd].index = 0;
    fcbArray[fd].buflen = 0;
    fcbArray[fd].currentBlk = 0;
    fcbArray[fd].numBlocks = 0;
    fcbArray[fd].de = NULL;

    return 0; // Success
}
