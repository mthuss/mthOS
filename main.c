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

long available_memory = MAX_MEM_SIZE;

typedef struct memoryPage
{
	char data[PAGE_SIZE];
	short reference_bit;
} memPage;

typedef struct pTableItem
{
	long virtual_addr;
	long physical_addr; //-1 means it is stored on disk
} pageItem_t;

typedef struct memoryPageTable
{
	long address; //address[virt] == phys;
} pageTable_t;

typedef struct memoryFrameTable
{
	//list of pointers to each and every frane stored in memory
	memPage* frame[NUMBER_OF_FRAMES];
} frameTable_t;

void frameTable_init(frameTable_t* table)
{
	for(int i = 0; i < NUMBER_OF_FRAMES;i++)
		table->frame[i] = NULL;
}

frameTable_t  frameTable;

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

//int memLoadReq(long size, char* data)
int memLoadReq(Process* proc, long address)
{
	if(!pageFault(proc,address)) //page already in memory, do nothing
		return 1;

	if(size > available_memory) //can't load page into main memory; memory full
	{
		//implement second chance algorithm here
	}


	//actually load page into memory
	//-------------------------------------------------------------
	long size = proc->seg_size;
	char* data = proc->code;

	int i;	

	int nFrames; //number of frames the data will be divided into
	nFrames = ceil(size / PAGE_SIZE);
	memPage* newPage = NULL;
	long pos = 0;
	long remaining_size = size;

	for(i = 0; i < NUMBER_OF_FRAMES && remaining_size > 0; i++)
	{
		if(frameTable.frame[i] == NULL) //find available frames
		{
			//initialize new page to be inserted in the frame
			newPage = malloc(PAGE_SIZE);
			memset(newPage,0,PAGE_SIZE);

			//creates a page of data and copies it to the available memory frame
			if(remaining_size < PAGE_SIZE)
				memcpy(newPage->data, data+pos, remaining_size);
			else
				memcpy(newPage->data, data+pos, PAGE_SIZE);
			frameTable.frame[i] = newPage;
			frameTable.frame[i]->reference_bit = 1;

			//update counter variables
			remaining_size -= PAGE_SIZE;
			pos += PAGE_SIZE;
		}
	}
	available_memory -= size;
	
}

void queueProcess(BCP** head, BCP* proc) //adds proc into the scheduling list
{
	if(*head == NULL)
	{
		*head = proc;
		return;
	}
	
	//ordering by shortest remaining time
	BCP *aux, *prev = NULL;
	for(aux = *head; aux != NULL && aux->remaining_time < proc->remaining_time; prev = aux, aux = aux->next);
	proc->next = aux;
	if(prev)
		prev->next = proc;
	else
		*head = proc;
}

BCP* initializeBCPregister(int time)
{
	BCP* new = malloc(sizeof(BCP));
	new->proc = NULL;
	new->remaining_time = time;
	new->next = NULL;
	return new;
}

Process* processCreate(char* name, int PID, int priority, int seg_size, char* used_semaphores, char* code, pageTable_t* pTable)
{
	Process* newProc = malloc(sizeof(Process));

	//page table will have seg_size/PAGE_SIZE lines
	newProc->pTable = malloc(ceil((float)seg_size/PAGE_SIZE)*sizeof(pageItem_t)); 
}
void showList(BCP* head)
{
	for(;head;head = head->next)
		printf("%d ",head->remaining_time);
	printf("\n");
}

void queueTest(BCP* bcpHead)
{
	BCP* p1 = initializeBCPregister(30);
	BCP* p2 = initializeBCPregister(40);
	BCP* p3 = initializeBCPregister(35);
	BCP* p4 = initializeBCPregister(45);
	BCP* p5 = initializeBCPregister(15);
	queueProcess(&bcpHead, p1);
	queueProcess(&bcpHead, p2);
	queueProcess(&bcpHead, p5);
	queueProcess(&bcpHead, p4);
	queueProcess(&bcpHead, p3);
	showList(bcpHead);
}

int main()
{
	BCP* bcpHead = NULL;
	frameTable_init(&frameTable);
	
}
