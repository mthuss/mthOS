# mthOS

An Operating System simulator written in C

#### Functions simulated:
- Process scheduling
- Interrupts
- Semaphores
- Memory management / paging
- I/O requests 

<br>

## About the program file structure
The synthetic program files must obey the following format:
#### file.prog:
```
programName
segmentID (not used)
priority (not used)
amount of memory needed by program (in kbytes)
list of semaphores used by the program (single character, separated by spaces)

<operations>
```
Simulation of the following operations is supported:
- exec t: execute for t units of time
- read k: read track k (for k units of time)
- write k: write on track k (for k units of time)
- P(s): wait on semaphore s
- V(s): post semaphore s
- print t: print for t units of time

This file must have lines separated exclusively by Line Feeds (no carriage returns allowed)

#### How to check for Carriage Returns
you can run the following command to check if your program contains CR characters:
```bash
xxd -b <programFilename> | grep 00001101 &> /dev/null && echo "has CRs" || echo "has no CRs"
```

#### Remove Carriage Returns from file
To remove all carriage return characters from your program files, run the following command:
```bash
sed -i s/$(echo -e "\x0D")//g <programFilename>
```
