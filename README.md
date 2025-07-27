# CSC415 File System Implementation

A custom file system implementation in C that simulates essential Unix-like file operations including directory navigation, file manipulation, and buffered I/O operations.

## Overview

This project implements a complete file system from scratch, featuring:
- **Volume Control Block (VCB)** for metadata management
- **FAT-based free space management** for efficient block allocation
- **Directory system** with hierarchical navigation
- **Buffered file I/O** for optimized read/write operations
- **Unix-like shell commands** (ls, cd, mkdir, rm, cp, mv, etc.)

## Architecture

The file system consists of three core components:

### 1. File System Shell (`fsshell.c`)
- Handles user input and command dispatching
- Provides interactive shell interface
- Supports standard Unix commands

### 2. Main File System (`mfs.c`)
- Implements core file system logic
- Directory operations (mkdir, rmdir, opendir, closedir)
- File management (create, delete, move, stat)
- Path resolution and navigation

### 3. Buffered I/O (`b_io.c`)
- Low-level file I/O with buffering
- Optimized read/write operations
- File descriptor management
- Seek operations for file positioning

## Key Features

### Directory Operations
- **Create directories** (`mkdir`) with proper parent-child relationships
- **Navigate filesystem** (`cd`, `pwd`) with absolute and relative paths
- **List contents** (`ls`) showing files and directories
- **Remove directories** (`rmdir`) with empty directory validation

### File Operations
- **Create files** (`touch`) with automatic block allocation
- **Copy files** (`cp`) preserving metadata and content
- **Move/rename** (`mv`) files between directories
- **Delete files** (`rm`) with proper cleanup
- **Display content** (`cat`) for file viewing

### Memory Management
- **FAT table** for tracking free and allocated blocks
- **Dynamic block allocation** and deallocation
- **Chain management** for large files spanning multiple blocks
- **Efficient space utilization** with block linking

### Path Resolution
- **Absolute and relative path** support
- **Path normalization** handling `.` and `..` components
- **Robust tokenization** preserving original path strings
- **Multi-level directory** navigation

## Technical Implementation

### Volume Control Block
- Stores filesystem metadata (volume size, root directory, free space info)
- Prevents re-initialization of formatted volumes
- Maintains free space head pointer for efficient allocation

### Free Space Management
- FAT (File Allocation Table) implementation
- Each block has corresponding FAT entry indicating status
- Supports block chaining for files larger than single blocks
- Automatic cleanup when files are deleted

### Directory Structure
```c
typedef struct DirectoryEntry {
    char name[MAX_FILENAME_LEN];
    uint32_t size;
    uint32_t startBlock;
    time_t created;
    time_t lastAccessed;
    time_t lastModified;
    bool isDirectory;
    bool inUse;
    mode_t permissions;
} DirectoryEntry;
```

## Supported Commands

| Command | Description |
|---------|-------------|
| `ls` | List directory contents |
| `cd` | Change directory |
| `pwd` | Print working directory |
| `mkdir` | Create directory |
| `rmdir` | Remove empty directory |
| `touch` | Create empty file |
| `rm` | Remove file |
| `cp` | Copy file |
| `mv` | Move/rename file |
| `cat` | Display file contents |
| `cp2l` | Copy from Linux filesystem to our filesystem |
| `cp2fs` | Copy from our filesystem to Linux filesystem |
| `help` | Show available commands |
| `history` | Show command history |

## Building and Running

```bash
# Compile the project
make

# Run the filesystem
make run

# Clean build files
make clean
```
*This file system implementation demonstrates low-level systems programming concepts including disk I/O, memory management, data structures, and operating system interfaces.*