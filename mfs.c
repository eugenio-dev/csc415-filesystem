/**************************************************************
* Class::  CSC-415-03 Spring 2025
* Name:: Ty Bohlander, Julia Bui, Eugenio Ramirez, Juan Ramirez
* Student IDs:: 
* GitHub-Name:: Tybo2020
* Group-Name:: The Ducklings
* Project:: Basic File System
*
* File:: mfs.c
*
* Description:: 
*
**************************************************************/
#include "mfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "freeSpace.h"

char currentWorkingPath[MAXDIR_LEN] = "/";


DirectoryEntry* rootDirectory = NULL;
DirectoryEntry* currentWorkingDirectory = NULL;

// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode){
    ppinfo* info = malloc(sizeof(ppinfo));
    
    char *pathCopy = strdup(pathname);
    if (pathCopy == NULL) {
        free(info);
        return -1;
    }

    if(parsePath(pathCopy, info) != 0 || info->index != -1){
        free(info);
        free(pathCopy);
        return -1;
    }

    // //Check if a file/directory with the same name already exists in that location
    // if(info->index != -1){
    //     free(info);
    //     free(pathCopy);
    //     return -1;
    //}

    uint64_t startBlock = createDirectory(MAX_ENTRIES, info->parent);
    if(startBlock == 0){
        free(info);
        free(pathCopy);
        return -1;  // Failed to create directory
    }

    // Find a free entry in the parent directory for the new directory
    int freeIndex = -1;
    for(int i = 0; i < MAX_ENTRIES; i++){
        if(!info->parent[i].inUse){
            freeIndex = i;
            break;
        }
    }
    
    if(freeIndex == -1){
        // No free space in parent directory
        free(info);
        free(pathCopy);
        return -1;
    }

    // Initialize the new directory entry in the parent
    time_t currentTime = time(NULL);
    strncpy(info->parent[freeIndex].fileName, info->lastElementName, sizeof(info->parent[freeIndex].fileName) - 1);
    info->parent[freeIndex].isDir = true;
    info->parent[freeIndex].fileSize = MAX_ENTRIES * sizeof(DirectoryEntry);  // Approximate size
    info->parent[freeIndex].startBlock = startBlock;
    info->parent[freeIndex].inUse = true;
    info->parent[freeIndex].createdTime = currentTime;
    info->parent[freeIndex].lastModified = currentTime;
    info->parent[freeIndex].lastAccessed = currentTime;
    
    // Write the updated parent directory back to disk
    int parentBlocks = (info->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if(LBAwrite(info->parent, parentBlocks, info->parent[0].startBlock) != parentBlocks){
        free(info);
        free(pathCopy);
        return -1;  // Failed to update parent directory
    }
    
    free(info);
    free(pathCopy);
    return 0;
}

int fs_rmdir(const char *pathname){
    ppinfo* info = malloc(sizeof(ppinfo));
    
    // Create a copy of pathname since parsePath modifies it
    char *pathCopy = strdup(pathname);
    if (pathCopy == NULL) {
        free(info);
        return -1;
    }

    // Check if directory exists
    if(parsePath(pathCopy, info) != 0 || info->index < 0){
        free(info);
        free(pathCopy);
        return -1;
    }
    
    // Get the directory entry
    DirectoryEntry *dirEntry = &(info->parent[info->index]);
    
    // Check if it's actually a directory
    if(!dirEntry->isDir){
        free(info);
        free(pathCopy);
        return -1;
    }
    
    // Load the directory to check if it's empty
    DirectoryEntry *dirEntries = loadDir(dirEntry);
    if (dirEntries == NULL) {
        free(info);
        free(pathCopy);
        return -1;
    }
    
    // Check if directory is empty (skip . and .. entries)
    for(int i = 2; i < MAX_ENTRIES; i++){
        if(dirEntries[i].inUse){
            free(dirEntries);
            free(info);
            free(pathCopy);
            return -1;  // Directory not empty
        }
    }
    
    // Free the directory blocks
    int firstBlock = dirEntry->startBlock;
    int totalBlocks = (dirEntry->fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    releaseBlocks(firstBlock, totalBlocks);
    
    // Clear the directory entry in parent
    memset(dirEntry, 0, sizeof(DirectoryEntry));
    dirEntry->inUse = false;
    
    // Write the updated parent directory back to disk
    int parentBlocks = (info->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if(LBAwrite(info->parent, parentBlocks, info->parent[0].startBlock) != parentBlocks){
        free(dirEntries);
        free(info);
        free(pathCopy);
        return -1;  // Failed to update parent directory
    }
    
    // Clean up
    free(dirEntries);
    free(info);
    free(pathCopy);
    return 0;
}

// Directory iteration functions
fdDir * fs_opendir(const char *pathname){
    ppinfo* info = malloc(sizeof(ppinfo));
    
    char *pathCopy = strdup(pathname);
    if (pathCopy == NULL) {
        free(info);
        return NULL;
    }

    if(parsePath(pathCopy, info) != 0){
        free(info);
        free(pathCopy);
        return NULL;
    }

    DirectoryEntry* targetDir;

    if(info->index == -2){
        targetDir = rootDirectory;
    }
    else if(info->index == -1){
        free(info);
        free(pathCopy);
        return NULL;
    } else {
        targetDir = &(info->parent[info->index]);
        if (!isDEaDir(targetDir)) {
            free(info);
            free(pathCopy);
            return NULL;
        }
    }

    DirectoryEntry* dirEntries = loadDir(targetDir);
    if (dirEntries == NULL) {
        free(info);
        free(pathCopy);
        return NULL;
    }

    fdDir* dir = malloc(sizeof(fdDir));
    if (dir == NULL) {
        free(dirEntries);
        free(info);
        free(pathCopy);
        return NULL;
    }

    // Initialize the directory structure
    dir->directory = dirEntries;
    dir->dirEntryPosition = 0;
    dir->d_reclen = sizeof(fdDir);
    dir->di = malloc(sizeof(struct fs_diriteminfo));
    if (dir->di == NULL) {
        free(dir);
        free(dirEntries);
        free(info);
        free(pathCopy);
        return NULL;
    }
    
    free(info);
    free(pathCopy);
    return dir;
}

// TO DO:
struct fs_diriteminfo *fs_readdir(fdDir *dirp){
    if (dirp == NULL || dirp->directory == NULL || dirp->di == NULL) {
        return NULL;
    }

    DirectoryEntry* dirEntries = dirp->directory;
    int position = dirp->dirEntryPosition;

    // Search for a valid entry
    while(position < MAX_ENTRIES){
        if (dirEntries[position].inUse){
            // Exit once we find the valid entry
            break;
        }
        position++;
    }

    // Check if we reached the end
    if (position >= MAX_ENTRIES) {
        return NULL;  // No more entries
    }

    dirp->di->d_reclen = sizeof(struct fs_diriteminfo);

    // Copy the filename 
    strncpy(dirp->di->d_name, dirEntries[position].fileName, 255);
    dirp->di->d_name[255] = '\0';

    // Set the file type
    if (dirEntries[position].isDir) {
        dirp->di->fileType = FT_DIRECTORY;
    } else {
        dirp->di->fileType = FT_REGFILE;
    }
    
    // Update the position counter for the next call
    dirp->dirEntryPosition = position + 1;
    
    return dirp->di;
}

int fs_closedir(fdDir *dirp){
    // check to see if we are in a proper directory 
    if (dirp == NULL){
        return -1; 
    }

    // free the directory item info struct
    if (dirp -> di != NULL){
        free(dirp -> di);
        dirp -> di = NULL; 
    }

    // free the dreictory entry array
    if (dirp -> directory != NULL){
        free(dirp -> directory);
        dirp -> directory = NULL; 
    }

    // free the main fdDir struc
    free (dirp);
    dirp = NULL;

    // muy importante to return to caller upon success 
    return 0; 
}


// Misc directory functions
char * fs_getcwd(char *pathname, size_t size) {
    if (pathname == NULL || size == 0) {
        return NULL;
    }
    
    // Get directory name
    strncpy(pathname, currentWorkingPath, size);
    
    // Ensure null-termination
    pathname[size-1] = '\0';
    
    return pathname;
}

char* normalizePathname(const char* path) {
    if (path == NULL) {
        return NULL;
    }
    
    char* normalized = malloc(MAXDIR_LEN);
    if (normalized == NULL) {
        return NULL;
    }
    
    // For absolute paths, start from root
    if (path[0] == '/') {
        strcpy(normalized, "/");
    } else {
        // For relative paths, start from current directory
        strcpy(normalized, currentWorkingPath);
    }
    
    char* pathCopy = strdup(path);
    char* token = strtok(pathCopy, "/");
    
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Stay in current directory, do nothing
        } else if (strcmp(token, "..") == 0) {
            // Go up one directory
            char* lastSlash = strrchr(normalized, '/');
            if (lastSlash != NULL && lastSlash != normalized) {
                *lastSlash = '\0';  // Remove last component
            } else if (lastSlash == normalized) {
                // At root, can't go up further
                strcpy(normalized, "/");
            }
        } else {
            // Regular directory name
            if (strcmp(normalized, "/") != 0) {
                strcat(normalized, "/");
            }
            strcat(normalized, token);
        }
        
        token = strtok(NULL, "/");
    }
    
    // If result is empty, set to root
    if (strlen(normalized) == 0) {
        strcpy(normalized, "/");
    }
    
    free(pathCopy);
    return normalized;
}

//linux chdir
int fs_setcwd(char *pathname){
    if (pathname == NULL) {
        return -1;
    }
    
    // Normalize the pathname first
    char* normalizedPath = normalizePathname(pathname);
    if (normalizedPath == NULL) {
        return -1;
    }
    
    ppinfo* info = malloc(sizeof(ppinfo));
    if (info == NULL) {
        free(normalizedPath);
        return -1;
    }

    // Make a copy since parsePath modifies the string
    char* pathCopy = strdup(normalizedPath);
    if (pathCopy == NULL) {
        free(info);
        free(normalizedPath);
        return -1;
    }

    // Use copy for parsing
    if (parsePath(pathCopy, info) == -1) {
        free(info); 
        free(normalizedPath);
        free(pathCopy);
        return -1; 
    }

    // Special case for the root directory 
    if (info->index == -2){
        if (currentWorkingDirectory != rootDirectory) {
            free(currentWorkingDirectory);
        }
        currentWorkingDirectory = rootDirectory;
        strncpy(currentWorkingPath, "/", MAXDIR_LEN);
        free(info);
        free(normalizedPath);
        free(pathCopy);
        return 0;
    }

    if (info->index < 0) {
        free(info);
        free(normalizedPath);
        free(pathCopy);
        return -1;
    }

    DirectoryEntry *target = &(info->parent[info->index]);

    // Make sure it's a directory
    if (!target->isDir) {
        free(info);
        free(normalizedPath);
        free(pathCopy);
        return -1;
    }

    // Allocate space for new directory block
    int blocksToRead = (target->fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    DirectoryEntry *newDir = malloc(sizeof(DirectoryEntry) * MAX_ENTRIES);
    if (newDir == NULL) {
        free(info);
        free(normalizedPath);
        free(pathCopy);
        return -1;
    }

    // Read directory block into memory
    LBAread(newDir, blocksToRead, target->startBlock);

    // Free the old current working directory if it's not the root
    if (currentWorkingDirectory != rootDirectory) {
        free(currentWorkingDirectory);
    }

    // Set CWD to new block
    currentWorkingDirectory = newDir;

    // Update the current working path
    strncpy(currentWorkingPath, normalizedPath, MAXDIR_LEN);
    currentWorkingPath[MAXDIR_LEN - 1] = '\0';

    free(info);
    free(normalizedPath);
    free(pathCopy);
    return 0;
}

int fs_isFile(char * filename){
    ppinfo* info = malloc(sizeof(ppinfo));
    
    char *pathCopy = strdup(filename);
    if (pathCopy == NULL) {
        free(info);
        return 0;
    }
    
    if(parsePath(pathCopy, info) == -1){
        // Path does not exist
        free(info);
        free(pathCopy);
        return 0;
    }
    
    int indx = info->index;
    if (indx < 0) {
        free(info);
        free(pathCopy);
        return 0;
    }
    
    DirectoryEntry *entry = &(info->parent[indx]);
    int isFile = !entry->isDir;  // If it's not a directory, it's a file
    
    if (info->parent != rootDirectory && info->parent != currentWorkingDirectory) {
        free(info->parent);
    }

    free(info);
    free(pathCopy);
    return isFile ? 1 : 0;
}

int fs_isDir(char * pathname){
    ppinfo* info = malloc(sizeof(ppinfo));
    
    char *pathCopy = strdup(pathname);
    if (pathCopy == NULL) {
        free(info);
        return 0;
    }

    if(parsePath(pathCopy, info) == -1){
        // Path does not exist
        free(info);
        free(pathCopy);
        return 0;
    }
    
    int indx = info->index;
    if (indx < 0) {
        free(info);
        free(pathCopy);
        return 0;
    }
    
    // Checks isDir, which holds type for directory entries
    DirectoryEntry  *dir = &(info->parent[indx]);
    int result = isDEaDir(dir);

    if (info->parent != rootDirectory && info->parent != currentWorkingDirectory) {
        free(info->parent);
    }
    
    free(info);
    free(pathCopy);
    return result;
}

int fs_delete(char* filename){
    ppinfo* info = malloc(sizeof(ppinfo));
    
    char *pathCopy = strdup(filename);
    if (pathCopy == NULL) {
        free(info);
        return -1;
    }

    if(parsePath(pathCopy, info) == -1){
        // File does not exist
        free(info);
        free(pathCopy);
        return -1;
    }
    
    if(fs_isFile(filename) == 1){
        // Deletes file by freeing the memory associated with it
        int indx = info->index;
        int firstBlock = info->parent[indx].startBlock;
        int totalBlocks = (info->parent[indx].fileSize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
        releaseBlocks(firstBlock, totalBlocks);
        
        // Clear the directory entry
        strcpy(info->parent[indx].fileName, "NULL");
        info->parent[indx].permissions = 0;
        info->parent[indx].fileSize = 0;
        info->parent[indx].startBlock = 0;
        info->parent[indx].createdTime = 0;
        info->parent[indx].lastModified = 0;
        info->parent[indx].lastAccessed = 0;
        info->parent[indx].isDir = false;
        info->parent[indx].inUse = false;
        
        // Write the updated parent directory back to disk
        int parentBlocks = (info->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        if(LBAwrite(info->parent, parentBlocks, info->parent[0].startBlock) != parentBlocks){
            free(info);
            free(pathCopy);
            return -1;  // Failed to update parent directory
        }       

        if (info->parent != rootDirectory && info->parent != currentWorkingDirectory) {
            free(info->parent);
        }

        free(info);
        free(pathCopy);
        return 0;
    }else{
        free(info);
        free(pathCopy);
        return -1;
    }
}

int parsePath(char* pathname, ppinfo* ppi){
    DirectoryEntry* parent;
    DirectoryEntry* startParent;
    char* savePtr;
    char* token1;
    char* token2;

    if(pathname == NULL) return (-1);
    if(pathname[0] == '/') {
        startParent = rootDirectory;
    }else{
        startParent = currentWorkingDirectory;
    }
    parent = startParent;

    token1 = strtok_r(pathname, "/", &savePtr);

    if(token1 == NULL) {
        if(pathname[0] == '/'){
            ppi->parent = parent;
            ppi->index = -2;
            ppi->lastElementName = NULL;
            return 0;
        }else{
            return -1;
        }
    }

    while(1){
        int idx = findInDirectory(parent, token1);
        token2 = strtok_r(NULL, "/", &savePtr);

        if(token2 == NULL){
            ppi->parent = parent;
            ppi->index = idx;
            ppi->lastElementName = token1;
            return 0;
        }else{
            if(idx == -1){
                return -2;
            }
            if(!isDEaDir(&(parent[idx]))) return (-1);

        DirectoryEntry* tempParent = loadDir(&(parent[idx]));

        if(parent != startParent){
            free(parent);
        }

        parent = tempParent;
        token1 = token2;

        }
    }
}

int findInDirectory(DirectoryEntry* parent, char* token){

    // Iterate through the every possible entry 
    for(int i = 0; i < MAX_ENTRIES; i++ ){
        if(strcmp(parent[i].fileName, token) == 0){
            return i;
        }
    }

    // Directory entry does not exist;
    return -1;
}

DirectoryEntry* loadDir(DirectoryEntry* targetDir){
    // Calculate how many entries we have
    int numEntries = targetDir->fileSize / sizeof(DirectoryEntry);
    // Calculate how many blocks are needed to store the directory
    int blocksNeeded = (targetDir->fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    DirectoryEntry* dirBuffer = (DirectoryEntry*)malloc(targetDir->fileSize);
    if (dirBuffer == NULL) {
        return NULL; 
    }
    // Read the directory blocks from disk
    int startBlock = targetDir->startBlock;
    // LBAread returns the number of blocks actually read
    if (LBAread(dirBuffer, blocksNeeded, startBlock) != blocksNeeded) {
        free(dirBuffer);
        return NULL;
    }
    // Update access time for the directory
    targetDir->lastAccessed = time(NULL);
    return dirBuffer;
}

// Returns 1 if DE is a directory, 0 if false
int isDEaDir(DirectoryEntry* targetDir){
    if(targetDir->isDir == false){
        return 0;
    }
    else return 1; 
}

int fs_stat(const char *path, struct fs_stat *buf){
    printf("\n Printing metadata \n");

    if (path == NULL || buf == NULL){
		return -1; 
	}

    // need to allocate memory to parse the directory path we want
	ppinfo* info = malloc(sizeof(ppinfo));
    
    char *pathCopy = strdup(path);
    if (pathCopy == NULL) {
        free(info);
        return -1;
    }

	// parse the path to locate desire file in directory struct
    int result = parsePath(pathCopy, info);
    if(result != 0){
		free(info);
        free(pathCopy);
		return -1; 
    }  
    
    // invalid path; index of -1 --> actual file does not exist 
    if(info->index == -1){
        free(info);
        free(pathCopy);
        return -1; 
    }

    DirectoryEntry entry = info->parent[info->index];

    // populate fs_stat struct
    buf->st_size = entry.fileSize;
    buf->st_blksize = BLOCK_SIZE;  
    buf->st_blocks = (entry.fileSize + 511) / 512; 
    buf->st_accesstime = entry.lastAccessed;
    buf->st_modtime = entry.lastModified;
    buf->st_createtime = entry.createdTime;

    if (info->parent != rootDirectory && info->parent != currentWorkingDirectory) {
    free(info->parent);
}
    free(info);
    free(pathCopy);

    return 0; 
}

int fs_mv(const char *srcPath, const char *destPath) {
    ppinfo *srcInfo = malloc(sizeof(ppinfo));
    ppinfo *destInfo = malloc(sizeof(ppinfo));

    char *srcCopy = strdup(srcPath);
    char *destCopy = strdup(destPath);

    if (parsePath(srcCopy, srcInfo) != 0 || srcInfo->index == -1) {
        printf("mv error: Source does not exist.\n");
        goto fail;
    }

    if (parsePath(destCopy, destInfo) == 0 && destInfo->index != -1) {
        printf("mv error: Destination already exists.\n");
        goto fail;
    }

    DirectoryEntry *srcEntry = &(srcInfo->parent[srcInfo->index]);

    // Find a free entry in the destination directory
    int destFreeIndex = -1;
    for (int i = 0; i < MAX_ENTRIES; i++) {
        if (!destInfo->parent[i].inUse) {
            destFreeIndex = i;
            break;
        }
    }

    if (destFreeIndex == -1) {
        printf("mv error: Destination directory full.\n");
        goto fail;
    }

    // Copy entry metadata
    DirectoryEntry *destEntry = &destInfo->parent[destFreeIndex];
    memcpy(destEntry, srcEntry, sizeof(DirectoryEntry));
    strncpy(destEntry->fileName, destInfo->lastElementName, sizeof(destEntry->fileName) - 1);
    destEntry->fileName[sizeof(destEntry->fileName) - 1] = '\0';
    destEntry->inUse = true;
    destEntry->lastModified = time(NULL);

    // Remove old entry
    srcEntry->inUse = false;
    strcpy(srcEntry->fileName, "NULL");

    // Write changes
    int srcBlocks = (srcInfo->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int destBlocks = (destInfo->parent[0].fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    LBAwrite(srcInfo->parent, srcBlocks, srcInfo->parent[0].startBlock);
    LBAwrite(destInfo->parent, destBlocks, destInfo->parent[0].startBlock);

    if (srcInfo->parent != rootDirectory && srcInfo->parent != currentWorkingDirectory){
        free(srcInfo->parent);
    }
    if (destInfo->parent != rootDirectory && destInfo->parent != currentWorkingDirectory){
        free(destInfo->parent);
    }
    
    free(srcInfo); free(destInfo); free(srcCopy); free(destCopy);
    return 0;

fail:
    free(srcInfo); free(destInfo); free(srcCopy); free(destCopy);
    return -1;
}