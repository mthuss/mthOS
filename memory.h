#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"

//1GB de memória
#define MAX_MEM_SIZE 1074000000

//8kbytes
#define PAGE_SIZE 8192

#define NUMBER_OF_FRAMES MAX_MEM_SIZE/PAGE_SIZE

typedef struct memoryPage
{
	//char data[PAGE_SIZE]; //doesn't actually need to store any data
	long* associated_pTable_entry;
	char reference_bit;
} memPage;

typedef struct memoryPageTable
{
	long* address; //address[virt] == phys;
	//-1 means it's stored in disk
} pageTable_t;

typedef struct memoryFrameTable
{
	//list associating each and every frame in memory to the page stored in it
	memPage* frame[NUMBER_OF_FRAMES];
} frameTable_t;

typedef struct cmd{
	int arg;
	char* call;
} command_t;

//Function prototypes:
//-------------------------
int inFrameTable(int pos, Process* proc);
void memLoadReq(Process* proc);
void Free(BCPitem_t* a);
Process* readProgramfromDisk(char* filename);
