# FAT32
Simple utility that let's the user to do basic operations on a FAT32 disc copy in .dd format. List of recognizable operations: ls, cd, cp, mkdir, rm, rmdir, rmdir-r, touch.

Features yet to be added:
  - support for creating long file names ( right now to ensure compability with windows I'm using 8.3 filename(SFN) - limited only to 8     characters + 3 characters for extension)
  - making the code more readable by diving it into separate files, and also some things  inside seems redundant
  - right now when doing any operations on file I load it all up to RAM - there's a need to implement some kind of a buffer
  - proper behaviour at the end of a cluster
  - defragmentation

Features to be added in more distant time. I don't know how to even start implementing them, especially the last two:
  - transform all the code into a some kind of a library, with a well documented API to let everyone interested easily do operations on a      fat32 .dd file
  - add some interface to let files from the disk be turned into a stream, so that one could operate on them with a FILE type.
  - add ability to communicate with a real disk and do operations directly on it, thus implementing a very easy file system tu use with any    machine that can run C++ code. I don't know if it wouldn't be better to start such a project in C. Maybe it would be more portable /      easy to use on small systems? Also I might consider addind support for earlier versions of FAT, more suitable for smaller disks.
