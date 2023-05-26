/*----------------------------------------------------------------------------------
  					TO-DO
----------------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

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
	char reference_bit;
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
	//list associating each and every frame in memory to the page stored in it
	memPage* frame[NUMBER_OF_FRAMES];
} frameTable_t;

typedef struct proc
{
	char name[8]; //limited to 8 bytes
	int SID; //Segment ID
	int priority; //won't actually be used
	int seg_size; //in kbytes
	char* used_semaphores; //list of semaphores, separated by spaces
	char* code;
	pageTable_t* pTable;
	
} Process;

typedef struct bcp_item
{
	Process* proc;
	long remaining_time;
	struct bcp_item* next;
} BCPitem_t;

typedef struct bcp
{
	BCPitem_t* head;
} BCP_t;

//Global variables:
//------------------------------------------------------------------------------
frameTable_t  frameTable;
BCP_t BCP;

//Functions:
//------------------------------------------------------------------------------
void init_data_structures()
{
	//frameTable
	for(int i = 0; i < NUMBER_OF_FRAMES; i++)
		frameTable.frame[i] = NULL;

	//BCP
	BCP.head = NULL;
}


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
	if(strcmp(&filename[i],FILE_EXT) == 0)
		return 1;

	return 0;
}

//make sure to check for NULL returns whenever this function is called
Process* readProgramfromDisk(char* filename)
{
	if(!validateFilename(filename))
	{
		printf("Invalid filename!! Only " FILE_EXT " files allowed!\n");
		return NULL;
	}

	FILE* file = fopen(filename,"r");

	if(!file)
	{
		printf("Requested file does not exist!\n");
		return NULL;
	}


	//get filesize
	fseek(file,0L,SEEK_END);
	long filesize = ftell(file);
	rewind(file);

	Process* proc = malloc(sizeof(Process));


	fscanf(file,"%[^\n]",&proc->name);
	fscanf(file,"%d\n",&proc->SID);
	fscanf(file,"%d\n",&proc->priority);
	fscanf(file,"%d\n",&proc->seg_size);
	proc->seg_size*=1024;

	//initialize page table
	proc->pTable = malloc(sizeof(pageTable_t));
	proc->pTable->address = malloc(ceil((float)proc->seg_size/PAGE_SIZE)*sizeof(long)); 

	int i = 0;
	char c;	
	//properly implement this as a list of semaphores, using strtok to remove spaces
	long sem_start_pos = ftell(file);
	char prevchar;
	int num_of_semaphores = 0; //one of the characters read was the semaphore, the other a space
	while(1) //figure out how many semaphores the process uses (assuming they only use single-letter names
	{
		c = fgetc(file);
		if(c == '\n' || c == EOF)
			break;
		if(c != ' ')
			num_of_semaphores++;
	}

	proc->used_semaphores = malloc(num_of_semaphores+1); //+1 for a null terminator
	fseek(file,sem_start_pos,SEEK_SET);
	i = 0;
	while(1) //actually retrieve the semaphores
	{
		c = fgetc(file);
		if(c == '\n' || c == EOF)
			break;
		if(c != ' ')
		{
			proc->used_semaphores[i] = c;
			i++;
		}
	}
	proc->used_semaphores[num_of_semaphores] = '\0';
	

	proc->code = malloc(filesize - ftell(file) + 1); //+1 for a null terminator
	i = 0;
	while(1) //save the remainder of the file as code
	{
		c = fgetc(file);
		if(c == EOF)
			break;
		proc->code[i] = c;
		i++;
			
	}
	proc->code[i] = '\0';
	
	fclose(file);

	return proc;

}

void printProcessInfo(Process* proc)
{
	printf("Name: %s\nSegment ID: %d\nPriority: %d\nSegment Size: %d bytes\n",proc->name,proc->SID,proc->priority,proc->seg_size);
	printf("Used semaphores: ");
	for(int i = 0; proc->used_semaphores[i] != '\0'; i++)
		printf("%c ",proc->used_semaphores[i]);
	printf("\n\n");
	printf("---------------------------\n");
	printf("Code:\n%s",proc->code);
	printf("---------------------------\n");
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
Process* memLoadReq(char* file)
{
	Process* proc = readProgramfromDisk(file);
	if(!proc)
	{
		printf("Error retrieving program from disk\n");
		return NULL;
	}
	long size = proc->seg_size;

	int nFrames; //number of frames the data will be divided into
	nFrames = ceil((float)size / PAGE_SIZE);
//	if(size > available_memory) //can't load page into main memory: memory full
//	{
//		//calculates how many MORE frames are needed to store all the data
//		long frames_needed = ceil((float)(size - available_memory)/PAGE_SIZE);
//		long *freed_addresses;
//		sc_free(frames_needed, freed_addresses);
//		//should not free nFrames, rather, should free exactly how many frames are missing to store all of the data
//
//		//implement second chance algorithm here
//		return NULL;
//	}


	//actually load page into memory
	//-------------------------------------------------------------

	int i = 0;	
	int j = 0;

	memPage* newPage = NULL;
	int pageNum = 0;
	long remaining_size = size;

	while(nFrames > 0)
	{
		printf("rem: %d | avail: %d\n",remaining_size,available_memory);
		if(available_memory == 0 || size - nFrames*PAGE_SIZE > available_memory) //no memory available for current page
		{
			printf("not enough memory!\n");
			//second chance
			for(j = 0; j <= NUMBER_OF_FRAMES && nFrames > 0; j++)
			{
				if(j == NUMBER_OF_FRAMES)
					j = 0;
				if(frameTable.frame[j] != NULL)
				{

					if(frameTable.frame[j]->reference_bit == 1)
						frameTable.frame[j]->reference_bit = 0;
					else
					{
						//freed memory is instantly replaced, so the available_memory counter is not changed
						free(frameTable.frame[j]);
						printf("freed frame %d\n",j);
						newPage = malloc(sizeof(memPage));
						memset(newPage,0,sizeof(memPage));
						frameTable.frame[j] = newPage;
						frameTable.frame[j]->reference_bit = 1; //sets reference bit of newPage to 1

						//updates page table
						proc->pTable->address[pageNum] = j;
						break;
					}
				}
			}
		}
		else //memory is available for current page
		{
			for(; i < NUMBER_OF_FRAMES && nFrames > 0; i++)
				if(frameTable.frame[i] == NULL) //find available frames
				{
					//initialize new page to be inserted in the frame
					newPage = malloc(sizeof(memPage));
					memset(newPage,0,sizeof(memPage));

					frameTable.frame[i] = newPage;
					frameTable.frame[i]->reference_bit = 1; //sets reference bit of newPage to 1

					//updates page table
					proc->pTable->address[pageNum] = i;
					break;
				}
			available_memory -= PAGE_SIZE;
		}

		//update counter variables
		pageNum++;
		nFrames--;
		remaining_size -= PAGE_SIZE;
	}

	return proc;
}

int isDigit(char c)
{
	if(c >= '0' && c <= '9')
		return 1;
	return 0;
}

long calculateRemainingTime(Process* proc)
{
	char* code = proc->code;
	long remaining_time = 0;

	int i,j;
	i = 0;
	char time[10];
	while(1)
	{
		if(isDigit(code[i]))
		{
			for(j = 0; isDigit(code[i]); i++, j++)
				time[j] = code[i];
			time[j] = '\0';

			remaining_time += strtol(time,NULL,10); 
		}
		if(code[i] == '\0')
			break;
		i++;

	}

	return remaining_time;
}

void queueProcess(BCPitem_t* proc) //adds proc into the scheduling list
{
	if(BCP.head == NULL)
	{
		BCP.head = proc;
		return;
	}
	
	//ordering by shortest remaining time
	BCPitem_t *aux, *prev = NULL;
	for(aux = BCP.head; aux != NULL && aux->remaining_time < proc->remaining_time; prev = aux, aux = aux->next);
	proc->next = aux;
	if(prev)
		prev->next = proc;
	else
		BCP.head = proc;
}

void processCreate(char* filename)
{
	//create a bcp register of said process
	//load process into memory
	BCPitem_t* new = malloc(sizeof(BCPitem_t));
	new->proc = NULL;
	new->proc = memLoadReq(filename);
	if(new->proc == NULL)
	{
		free(new);
		return;
	}

	new->remaining_time = calculateRemainingTime(new->proc);

	//queue the process in the scheduling list	
	queueProcess(new);
	printf("started process %s\n\n",new->proc->name);
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

////here, assume these attributes are being retrieved from a file
//Process* processCreate(char* name, int SID, int priority, int seg_size, char* used_semaphores, char* code)
//{
//	Process* newProc = malloc(sizeof(Process));
//
//	//page table will have seg_size/PAGE_SIZE lines
//	newProc->pTable = malloc(sizeof(pageTable_t));
//	newProc->pTable->address = malloc(ceil((float)seg_size/PAGE_SIZE)*sizeof(long)); 
//	strncpy(newProc->name,name,strlen(name));
//	newProc->SID = SID;
//	newProc->priority = priority;
//	newProc->seg_size = seg_size * 1024; //convert from kbytes to bytes
//	newProc->used_semaphores = used_semaphores;
//	newProc->code = code;
//	return newProc;
//}
void showList()
{
	BCPitem_t* head = BCP.head;
	for(;head;head = head->next)
		printf("%d ",head->remaining_time);
	printf("\n");
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
	init_data_structures();
//	Process* p1 = processCreate("proc1",1,1,30,"s t", NULL);
//	memLoadReq(p1);
//	queueTest();
//	readProgramfromDisk("synthetic_2.prog");
//	printProcessInfo(readProgramfromDisk("synthetic_2.prog"));
	processCreate("synthetic_2.prog");
	processCreate("synthetic_2.prog");
//	showList(BCP.head);
	
}
