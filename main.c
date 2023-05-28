/*----------------------------------------------------------------------------------
  					TO-DO
----------------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

//1GB de memória
#define MAX_MEM_SIZE 1074000000

//8kbytes
#define PAGE_SIZE 8192

#define NUMBER_OF_FRAMES MAX_MEM_SIZE/PAGE_SIZE

#define FILE_EXT ".prog"

long available_memory = MAX_MEM_SIZE;

pthread_mutex_t lock;


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

typedef struct cmd{
	int arg;
	char* call;
} command_t;

typedef struct proc
{
	char name[8]; //limited to 8 bytes
	int SID; //Segment ID
	int priority; //won't actually be used
	int seg_size; //in kbytes
	char* used_semaphores; //list of semaphores, separated by spaces
	pageTable_t* pTable;
	command_t** code; //list of commands that comprise the code of the program
	int nCommands; //number of commands the program has
	
} Process;

typedef struct bcp_item
{
	Process* proc;
	int next_instruction;
	char status; //r: ready, R: running, b: blocked 
	long remaining_time;
	int PID;
	struct bcp_item* next;
} BCPitem_t;

typedef struct bcp
{
	BCPitem_t* head;
} BCP_t;

typedef struct ioop
{
	BCPitem_t* process; //stores a pointer to the blocked process
	char type; //r: read, w: write, p: print
	int remaining_time;
	struct ioop* next;
}IOop_t;

typedef struct ioqueue
{
	IOop_t* head;
} IOqueue_t;

//Global variables:
//------------------------------------------------------------------------------
frameTable_t  frameTable;
BCP_t BCP;
IOqueue_t IOqueue;
BCPitem_t* curr_running = NULL;
volatile int PID = 0;
volatile long cpuclock = 0;
volatile int stop = 0;
sem_t list_sem;

//Functions:
//------------------------------------------------------------------------------
void init_data_structures()
{
	//frameTable
	for(int i = 0; i < NUMBER_OF_FRAMES; i++)
		frameTable.frame[i] = NULL;

	//BCP
	BCP.head = NULL;

	//IO queue
	IOqueue.head = NULL;
}

void io_queue_add(BCPitem_t* item, char type)
{
	pthread_mutex_lock(&lock);
	IOop_t* new = malloc(sizeof(IOop_t));
	new->remaining_time = item->proc->code[item->next_instruction]->arg;
	new->process = item;
	new->type = type;
	new->next = NULL;

	if(IOqueue.head == NULL)
	{
		IOqueue.head = new;
		pthread_mutex_unlock(&lock);
		return;
	}

	IOop_t* aux, *prev = NULL;
	for(aux = IOqueue.head; aux; prev = aux, aux = aux->next);
	new->next = aux;
	if(prev)
		prev->next = new;
	else
		IOqueue.head = new;
	pthread_mutex_unlock(&lock);
}

//void advanceIOqueue(int time)
//{
//	IOop_t* aux = IOqueue.head;
//
//	for(;aux && time > 0; aux = aux->next)
//		if(aux->remaining_time < time)
//		{
//			IOqueue.head = IOqueue.head->next; //unqueue aux
//			aux->process->status = 'r';
//			time-=aux->remaining_time;
//		}
//		else
//		{
//			aux->remaining_time -= time;
//			time = 0;
//		}
//}
void queueProcess(BCPitem_t* proc) //adds proc into the scheduling list
{
	if(BCP.head == NULL)
	{
		BCP.head = proc;
//		proc->status = 'R';
		sem_post(&list_sem);
		return;
	}
	
	//ordering by shortest remaining time
	BCPitem_t *aux, *prev = NULL;
	for(aux = BCP.head; aux != NULL && aux->remaining_time < proc->remaining_time; prev = aux, aux = aux->next);
	proc->next = aux;
	if(prev)
		prev->next = proc;
	else //proc is the new head
	{
		BCP.head = proc;
//		proc->status = 'R';
	}
//	sem_post(&list_sem);
}

void dequeueProcess(BCPitem_t* item)
{
	BCPitem_t *aux = BCP.head, *prev = NULL;
	for(; aux && aux != item; prev = aux, aux = aux->next);
	if(aux)
	{
		if(prev)
			prev->next = aux->next;
		else //aux is the current head
			BCP.head = BCP.head->next;
		aux->next = NULL;
	}
	else
		printf("Something went really wrong!!\n");

}

void processInterrupt() // interrupt current process and reschedule it
{
	BCPitem_t* curr = curr_running;
	if(curr)
	{
		curr->status = 'r';
		dequeueProcess(curr);
		queueProcess(curr);
	}
}
void advanceIOqueue()
{
	IOop_t* aux = IOqueue.head;
	if(aux)
	{
		aux->remaining_time--;
		aux->process->remaining_time--;
		if(aux->remaining_time == 0) //IO operation finished
		{
			aux->process->status = 'r';
			IOqueue.head = IOqueue.head->next;

			processInterrupt();
			//reschedule the now unblocked process
			dequeueProcess(aux->process);
			queueProcess(aux->process);
			aux->process->next_instruction++;

			
		}
	}
}

void Free(BCPitem_t* a)
{
	pthread_mutex_lock(&lock);
	free(a->proc->pTable);
	free(a->proc->code);
	free(a->proc);
	free(a);
	pthread_mutex_unlock(&lock);
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

command_t** parsecommands(char* code, int* inst_counter)
{
	char temp[100];
	int i;
	int count = 0;
	for(i = 0; code[i] != '\0'; i++)
		if(code[i] == '\n' || code[i] == '\0')
			count++;
//	printf("count: %d\n",count);
	*inst_counter = count;

	char** lines = malloc(count*sizeof(char*));
	command_t** cmd = malloc(count*sizeof(command_t*));
	int oldcount = count;

	count = 0;
	int j;
	pthread_mutex_lock(&lock);
	i = 0;
	while(code[i] != '\0')
	{
		for(j = 0; code[i] != '\n'; i++, j++)
		{
			temp[j] = code[i];
		}
		temp[j] = '\0';
		lines[count] = malloc(j);
		strncpy(lines[count],temp,j);
		count++;
		if(code[i] == '\0')
			break;

		i++; //go to next line
	}
	pthread_mutex_unlock(&lock);

	char* arg = NULL;
	for(i = 0; i < count; i++)
	{
		cmd[i] = malloc(sizeof(command_t));
		cmd[i]->call = strtok(lines[i]," ");
		arg = strtok(NULL," ");
		if(arg)
			cmd[i]->arg = (int)strtol(arg,NULL,10);
		else
			cmd[i]->arg = -1;
	}
	return cmd;

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
	

	char* code = malloc(filesize - ftell(file) + 1); //+1 for a null terminator
	i = 0;
	fgetc(file); //consume semaphore's newline
	while(1) //save the remainder of the file as code
	{
		c = fgetc(file);
		if(c == EOF)
			break;
		code[i] = c;
		i++;
			
	}
	code[i] = '\0';
	int inst_number;
	proc->code = parsecommands(code,&inst_number);
	proc->nCommands = inst_number;
	
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
//	printf("---------------------------\n");
//	printf("Code:\n");
//	for(int i = 0; i < proc->nCommands; i++)
//	{
//		printf("%s ",proc->code[i]->call);
//		if(proc->code[i]->arg != -1)
//			printf("%d",proc->code[i]->arg);
//		printf("\n");
//	}	
//	printf("---------------------------\n");
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

	//actually load page into memory
	//-------------------------------------------------------------

	int i = 0;	
	int j = 0;

	memPage* newPage = NULL;
	int pageNum = 0;
	long remaining_size = size;

	while(nFrames > 0)
	{
	//	printf("rem: %d | avail: %d\n",remaining_size,available_memory);
		if(available_memory == 0 || size - nFrames*PAGE_SIZE > available_memory) //no memory available for current page
		{
	//		printf("not enough memory!\n");
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
	//					printf("freed frame %d\n",j);
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
	long remaining_time = 0;
	for(int i = 0; i < proc->nCommands; i++)
	{
		if(proc->code[i]->arg != -1)
			remaining_time += proc->code[i]->arg;
	}
	return remaining_time;

}
//{
//	command_t* code[] = proc->code;
//	long remaining_time = 0;
//
//	int i,j;
//	i = 0;
//	char time[10];
//	while(1)
//	{
//		if(isDigit(code[i]))
//		{
//			for(j = 0; isDigit(code[i]); i++, j++)
//				time[j] = code[i];
//			time[j] = '\0';
//
//			remaining_time += strtol(time,NULL,10); 
//		}
//		if(code[i] == '\0')
//			break;
//		i++;
//
//	}
//
//	return remaining_time;
//}


void processCreate(char* filename)
{
	//create a bcp register of said process
	//load process into memory
	BCPitem_t* new = malloc(sizeof(BCPitem_t));
	new->proc = NULL;
	new->next = NULL;
	new->next_instruction = 0;
	new->proc = memLoadReq(filename);
	new->PID = PID;
	PID++;
	new->status = 'r';
	if(new->proc == NULL)
	{
		free(new);
		return;
	}

	new->remaining_time = calculateRemainingTime(new->proc);

	pthread_mutex_lock(&lock);
	processInterrupt();
	//queue the process in the scheduling list	
	queueProcess(new);
	pthread_mutex_unlock(&lock);
	printf("started process %s\n\n",new->proc->name);
}

void processFinish(BCPitem_t* proc)
{
	dequeueProcess(proc);
	Free(proc);
}

//void initializeMemory(Process* proc)
//{
//
//	long size = proc->seg_size;
//
//	int nFrames; //number of frames the data will be divided into
//	nFrames = ceil((float)size / PAGE_SIZE);
//
//	if(size > available_memory) //can't load page into main memory: memory full
//	{
//		long* freed_addresses;
//		sc_free(nFrames,freed_addresses);
//	}
//
//}

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

char* getStatus(char st)
{
	if(st == 'r')
		return "Ready";
	if(st == 'R')
		return "Running";
	if(st == 'b')
		return "Blocked";
	return "Unknown";
}
void viewProcessInfo()
{
	pthread_mutex_lock(&lock);
	if(BCP.head == NULL)
	{
		printf("No processes currently scheduled!\n");
		pthread_mutex_unlock(&lock);
		return;
	}
	printf("\nCurrent processes: ");
	printf("\nPID | Name (Status)\n");
	printf("-----------------------------------------\n");
	BCPitem_t* aux = NULL;
	for(aux = BCP.head;aux;aux = aux->next)
		printf("%d | %s (%s)\n",aux->PID,aux->proc->name,getStatus(aux->status));

	int search_pid;
	printf("\nWhich process do you want to view info about?\nPID: ");
	scanf("%d",&search_pid);
	int found = 0;
	for(aux = BCP.head;aux;aux = aux->next)
		if(aux->PID == search_pid)
		{
			printf("\n\n");
			printProcessInfo(aux->proc);
			printf("next instruction: %d\n",aux->next_instruction);
			printf("Remaining time: %ld\n",aux->remaining_time);
			found = 1;
			break;
		}
	if(!found)
		printf("Invalid PID! Process not found.\n");
	pthread_mutex_unlock(&lock);
	return;
}

void count_used_frames()
{
	int count = 0;
	for(int i = 0; i < NUMBER_OF_FRAMES; i++) 
		if(frameTable.frame[i])
			count++;
	printf("number of frames currently used: %d\n",count);

}

void* menu()
{
	int opt;
	char filename[128];
	do{

		printf("\nMenu:\n-----------------------------------------------\n[1] Create process\n[2] View process info\n[0] Quit\n");
		printf("\nOption: ");
		scanf("%d",&opt);
		switch(opt)
		{
			case 0: 
				stop = 1;
				break;
			case 1:
				printf("Program filename: ");
				scanf(" %[^\n]",filename);
				processCreate(filename);
				break;
			case 2:
				viewProcessInfo();
				sleep(3);
				break;
			default: 
				printf("Invalid option!\n");
				break;

		}
	}while(opt != 0);
}



void interpreter(BCPitem_t* curr)
{
	Process* proc = curr->proc;
	command_t* instruction = proc->code[curr->next_instruction];

	if(strcmp(instruction->call,"exec") == 0)
	{
//		if(IOqueue.head && IOqueue.head->remaining_time < instruction->arg)
//		{
//			advanceIOqueue(IOqueue.head->remaining_time);
//			instruction->arg -= IOqueue.head->remaining_time;
//			cpuclock += IOqueue.head->remaining_time;
//			return;
//		}
//		cpuclock += instruction->arg;
		pthread_mutex_lock(&lock);
		instruction->arg--;
		curr->remaining_time--;


		if(instruction->arg == 0)
			curr->next_instruction++;
		pthread_mutex_unlock(&lock);
		return;
	}
	if(strcmp(instruction->call,"read") == 0)
	{
//		BCP.head = BCP.head->next; //unqueues current process and goes to the next
		pthread_mutex_lock(&lock);
		curr_running = NULL;
		curr->status = 'b';
		pthread_mutex_unlock(&lock);
		io_queue_add(curr,'r');
		//io_queue_add(curr,'r');
	}

	if(strcmp(instruction->call,"write") == 0)
	{
//		BCP.head = BCP.head->next; //unqueues current process and goes to the next
		pthread_mutex_lock(&lock);
		curr_running = NULL;
		curr->status = 'b';
		pthread_mutex_unlock(&lock);
		io_queue_add(curr,'w');

	}
	if(strcmp(instruction->call,"print") == 0)
	{
//		BCP.head = BCP.head->next; //unqueues current process and goes to the next
		pthread_mutex_lock(&lock);
		curr_running = NULL;
		curr->status = 'b';
		pthread_mutex_unlock(&lock);
		io_queue_add(curr,'p');

	}
	if(strcmp(instruction->call,"P(s)") == 0)
	{
		curr->next_instruction++;
	}

	if(strcmp(instruction->call,"P(t)") == 0)
	{
		curr->next_instruction++;
	}

	if(strcmp(instruction->call,"V(s)") == 0)
	{
		curr->next_instruction++;
	}

	if(strcmp(instruction->call,"V(t)") == 0)
	{
		curr->next_instruction++;
	}

}
void* mainLoop()
{
	sleep(5);
	while(!stop)
	{
//		while(!curr_running)
//		{
			curr_running = BCP.head;
			while(curr_running && curr_running->status == 'b') //skip blocked processes
				curr_running = curr_running->next;
//			if(!curr_running)
//				sem_wait(&list_sem); //wait for a ready process to be added
//		}
		if(curr_running)
		{
			curr_running->status = 'R';
			interpreter(curr_running);
			if(curr_running != NULL && curr_running->next_instruction >= curr_running->proc->nCommands)
			{
				processFinish(curr_running);
			}
		}
		pthread_mutex_lock(&lock);
		advanceIOqueue();
		pthread_mutex_unlock(&lock);
		cpuclock++;
//		sleep(1);
		usleep(10000);
	}
		
}
int main()
{
	init_data_structures();
	sem_init(&list_sem,0,0);
	pthread_mutex_init(&lock,NULL);
	pthread_t kernel;
	pthread_t sim_menu;
	pthread_create(&sim_menu,NULL,menu,NULL);
	pthread_create(&kernel,NULL,&mainLoop,NULL);
	pthread_join(kernel,NULL);
	pthread_join(sim_menu,NULL);

//	processCreate("synthetic_2.prog");
//	BCPitem_t* cur = BCP.head;
//	for(int i = 0; i < cur->proc->nCommands; i++)
////		printf("%s %d\n",cur->proc->code[i]->call,cur->proc->code[i]->arg);
//		if(strcmp(cur->proc->code[i]->call,"exec") == 0)
//			printf("exec\n");
	
}
