# System-Call-To-Optimize-Memory-Utilization
## Files:
Utils.h (header file)
Kernel.config (config file)
xdedup.c (user space code)
sys_xdedup.c (kernel space code) 

## User Space:
The user level code throws error:
if unrecognized flags are supplied
if the given input files do not exist 
We don’t have the permission to read the two input files
insufficient no of arguments are supplied
extra arguments are supplied

## Kernel Space:
asmlinkage long xdedup(void *arg)  function:
The arguments received from the user space are  copied to a memory allocated in the kernel space and the DFLAG for the debugging option is enabled
Initially the following checks are performed:
if the required arguments supplied are NULL
if we can access the given input files and if they have read permissions
If the supplied files are directories or are not regular
In the case when user has not supplied -p option, if the owners of the files are different or if the two files differ in size (return error in these cases)
If the output file supplied is hardlinked to one of the input files (return error)
If the two input files are hardlinked to each other:
If -n option is given we return the size of one of the files
If -p option is given and -n is not given the data from one of the files is written onto the output file [called function partialData()]
If no flag supplied return error

### P and N
	If the user supplied both the options -p and -n, partialData() function is called which calculates the size of the similar data starting from the beginning of the files.

### P and !N
	If the user supplies -p option and does not supply -n we call partialData() with appropriate arguments. This function in turn will compare the file byte by byte and will write the similar data onto the output file. A temporary file is created first which is then renamed (using vfs_rename) to the name of the output file. This has been done to protect the output file from having erroneous data if some function fails midway. 

### !P and !N
	If the user has not supplied -p or -n, completeFileRead() function is called which compares data byte and byte. If the two files are entirely similar, file2 is unlinked and then linked to file1. Incase the data of file1 does not match the data of file2 entirely, -1 is returned.

### !P and N
	If the data in both the files is entirely same, the two given files are not linked, only the size of the files is returned.

### D
	If D flag is given the debugging statements will be printed in the console.

## Symlinks
The file2 will be hardlinked to file1 incase it matches file1 completely. During this process the absolute path of file2 is retrieved using the file pointer of file2. This path is passed to the link function which links file2 to file1 and this handles the case of symlinks

## REFERENCES:
http://ytliu.info/notes/linux/file_ops_in_kernel.html

https://elixir.bootlin.com/linux/v4.6.7/source
