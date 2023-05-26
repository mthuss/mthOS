/*----------------------------------------------------------------------------------
  					TO-DO
----------------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "memory.h"

//1GB de memória
#define MAX_MEM_SIZE 1074000000

//8kbytes
#define PAGE_SIZE 8192

#define NUMBER_OF_FRAMES MAX_MEM_SIZE/PAGE_SIZE

#define FILE_EXT ".prog"

long available_memory = MAX_MEM_SIZE;


typedef struct memoryPage
{
	//char data[PAGE_SIZE]; //doesn't actually need to store any data
	short reference_bit;
} memPage;

typedef struct pTableItem
{
	long virtual_addr;
	long physical_addr; //-1 means it is stored on disk
} pageItem_t;

typedef struct memoryPageTable
{
	long* address; //address[virt] == phys;
} pageTable_t;

typedef struct memoryFrameTable
{
	//list of pointers to each and every frane stored in memory
	memPage* frame[NUMBER_OF_FRAMES];
} frameTable_t;

frameTable_t  frameTable;

void frameTable_init(frameTable_t* table)
{
	for(int i = 0; i < NUMBER_OF_FRAMES;i++)
		table->frame[i] = NULL;
}


typedef struct proc
{
	char name[8]; //limited to 8 bytes
	int PID;
	int priority;
	int seg_size; //in kbytes
	char* used_semaphores; //list of semaphores, separated by spaces
	char* code;
	pageTable_t* pTable;
	
} Process;

typedef struct bcp
{
	Process* proc;
	int remaining_time;
	struct bcp* next;
} BCP;

BCP* bcpHead = NULL;

long sc_free(int nFrames, long* freed_addresses) //second chance free: returns address of the freed memory frame
{

}

int validateFilename(char* filename)
{
	int i;
	for(i = strlen(filename) - 1; i >= 0; i--)
	{
		if(filename[i] == '.')
			break;
	}
	if(strncmp(&filename[i],FILE_EXT,strlen(FILE_EXT)) == 0)
		return 1;

	return 0;
}

//make sure to check for NULL returns whenever this function is called
Process* readProgramfromDisk(char* filename)
{
	if(!validateFilename(filename))
	{
		printf("Invalid filename!!\nOnly " FILE_EXT " files allowed!\n");
		return NULL;
	}

	FILE* file = fopen(filename,"r");

	if(!file)
		printf("Requested file does not exist!\n");
}
//checks if the requested virtual address is already in memory
//apparently won't be used, since the only memory transaction that will be done is that of loading a process into memory (once).
int pageFault(Process* proc, long virt_addr)
{
	long numPages = ceil(proc->seg_size/PAGE_SIZE);
	if(virt_addr > numPages)
	{
		printf("Segmentation fault\n");
		exit(1);
	}
	if(proc->pTable->address[virt_addr] == -1) //requested page is in disk
		return 1;
	return 0;
}

//loads a process into memory and sets its virtual adresses
int memLoadReq(Process* proc)
{
	long size = proc->seg_size;

	int nFrames; //number of frames the data will be divided into
	nFrames = ceil((float)size / PAGE_SIZE);
	if(size > available_memory) //can't load page into main memory: memory full
	{
		long *freed_addresses;
		sc_free(nFrames, freed_addresses);

		return 1;
	}


	//actually load page into memory
	//-------------------------------------------------------------

	int i;	

	memPage* newPage = NULL;
	int pageNum = 0;
	long remaining_size = size;

	for(i = 0; i < NUMBER_OF_FRAMES && nFrames > 0; i++)
	{
		if(frameTable.frame[i] == NULL) //find available frames
		{
			//initialize new page to be inserted in the frame
			newPage = malloc(sizeof(memPage));
			memset(newPage,0,sizeof(memPage));

			frameTable.frame[i] = newPage;
			frameTable.frame[i]->reference_bit = 1; //sets reference bit of newPage to 1

			//updates page table
			proc->pTable->address[pageNum] = i;

			//update counter variables
			pageNum++;
			nFrames--;
		}
	}
	available_memory -= size;
	
}

void startProgram(Process* proc)
{
	//load process into memory
	//create a bcp register of said process
	//queue the process in the scheduling list	
}

void queueProcess(BCP* proc) //adds proc into the scheduling list
{
	if(bcpHead == NULL)
	{
		bcpHead = proc;
		return;
	}
	
	//ordering by shortest remaining time
	BCP *aux, *prev = NULL;
	for(aux = bcpHead; aux != NULL && aux->remaining_time < proc->remaining_time; prev = aux, aux = aux->next);
	proc->next = aux;
	if(prev)
		prev->next = proc;
	else
		bcpHead = proc;
}

BCP* initializeBCPregister(int time)
{
	BCP* new = malloc(sizeof(BCP));
	new->proc = NULL;
	new->remaining_time = time;
	new->next = NULL;
	return new;
}


void initializeMemory(Process* proc)
{

	long size = proc->seg_size;

	int nFrames; //number of frames the data will be divided into
	nFrames = ceil((float)size / PAGE_SIZE);

	if(size > available_memory) //can't load page into main memory: memory full
	{
		long* freed_addresses;
		sc_free(nFrames,freed_addresses);
	}

}

//here, assume these attributes are being retrieved from a file
Process* processCreate(char* name, int PID, int priority, int seg_size, char* used_semaphores, char* code)
{
	Process* newProc = malloc(sizeof(Process));

	//page table will have seg_size/PAGE_SIZE lines
	newProc->pTable = malloc(sizeof(pageTable_t));
	newProc->pTable->address = malloc(ceil((float)seg_size/PAGE_SIZE)*sizeof(long)); 
	strncpy(newProc->name,name,strlen(name));
	newProc->PID = PID;
	newProc->priority = priority;
	newProc->seg_size = seg_size * 1024; //convert from kbytes to bytes
	newProc->used_semaphores = used_semaphores;
	newProc->code = code;
	return newProc;
}
void showList(BCP* head)
{
	for(;head;head = head->next)
		printf("%d ",head->remaining_time);
	printf("\n");
}

void queueTest()
{
	BCP* p1 = initializeBCPregister(30);
	BCP* p2 = initializeBCPregister(40);
	BCP* p3 = initializeBCPregister(35);
	BCP* p4 = initializeBCPregister(45);
	BCP* p5 = initializeBCPregister(15);
	queueProcess(p1);
	queueProcess(p2);
	queueProcess(p3);
	queueProcess(p4);
	queueProcess(p5);
	showList(bcpHead);
}

void count_used_frames()
{
	int count = 0;
	for(int i = 0; i < NUMBER_OF_FRAMES; i++) 
		if(frameTable.frame[i])
			count++;
	printf("number of frames currently used: %d\n",count);

}

int main()
{
	frameTable_init(&frameTable);
	Process* p1 = processCreate("proc1",1,1,30,"s t", NULL);
	memLoadReq(p1);
	queueTest();
	count_used_frames();
	
}
