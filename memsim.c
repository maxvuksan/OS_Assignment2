#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
  int empty; 
  // saves constant memory allocation, simply mark an existing page as empty to be overwritten 
  
  int pageNo;
  
  int modified;
  
  int use; // for LRU swapping, is incremented everytime the page isn't accessed
          // set to 0 on access

} page;
		   
enum repl 
{ _random, //random name conflicts with random() function, so i added '_' :) 
fifo, 
lru, 
clock };

int createMMU(int);
int checkInMemory(int);
int allocateFrame(int);
page selectVictim(int, enum repl);

const int pageoffset = 12; /* Page size is fixed to 4 KB */
// 12 bits can represent 4KB (0-4095)

int numFrames;

page** page_table; // allocated by createMMU()

/* Creates the page table structure to record memory allocation */
int createMMU(int frames) {

	#pragma region notes
	/*

		say 'frames' = 4, our page table could look like this
		 ________
		|_______|       
		|_______|       	
		|_______|       	
		|_______|       

		where in each frame we store

		____________________
    |                   |
    |  Page Number      
    |  Dirty Bit
    |  Use Bit           		   
    |___________________|
	   
	   when a new line comes in 

	   000001000011 11111111111101000101

	   ____________: interpret first part as page

	               _____________________: second part as offset (which can be ignored for our simulation)

    luckily the provided skeleton handles all this yucky binary stuff for us!
	   
	*/
	#pragma endregion
	
	/* we will be implimenting a linear page table */
	
	page_table = malloc(frames * sizeof(page*)); /* page directory */
	
	for(int i = 0; i < frames; i++){
		page_table[i] = malloc(sizeof(page)); // NULL will denote an empty space
		page_table[i]->empty = 1; // all frames are initally empty
    page_table[i]->modified = 0;
    page_table[i]->use = 0;
	}

  	return 0;
}

/* Checks for residency: returns frame no or -1 if not found */
int checkInMemory(int page_number) {
	int result = -1;

	// iterates over each frame in page table, checking if page_number
	for(int i = 0; i < numFrames; i++){

		if(page_table[i]->pageNo == page_number && !page_table[i]->empty){
      result = i; // reset time since used
      page_table[i]->use = 0;
		}
    else{ 
      page_table[i]->use += 1;
    }
    
	}
	return result;
}

/* allocate page to the next free frame and record where it put it */
int allocateFrame(int page_number) {

	for(int i = 0; i < numFrames; i++){
		// frame i is empty, allocate there
		if(page_table[i]->empty){

			page_table[i]->pageNo = page_number;
			page_table[i]->empty = 0;
			page_table[i]->modified = 0;
			page_table[i]->use = 0; 

			return i;
		}
	}

	return -1;
}

/* Selects a victim for eviction/discard according to the replacement algorithm,
 * returns chosen frame_no  */
page selectVictim(int page_number, enum repl mode) {
	page victim;

	int evic_index = 0; // who are we removing?

	switch(mode){

		case fifo:

			break;

		case _random:

			evic_index = rand() % numFrames;
			break;
  
		case lru: {

      // find the largest 'use' value while ensuring the frame we pick is not empty 

      int max = page_table[0]->use;
      while(page_table[evic_index]->empty){
        evic_index += 1;
        if(evic_index >= numFrames - 1){
          break;
        }
      }
      max = page_table[evic_index]->use;

      for(int i = evic_index; i < numFrames; i++){

        if(page_table[i]->use > max && !page_table[i]->empty){
          evic_index = i;
          max = page_table[i]->use;
        }
      }

			break;
    }

		case clock:

			break;
		
	}

	victim = *page_table[evic_index];
	page_table[evic_index]->empty = 1;
  page_table[evic_index]->modified = 0;
  page_table[evic_index]->use = 0;
  
	return (victim);
}

/* marks modified bit as 1, will write to disk */
void modifyPage(int page_number){

	for(int i = 0; i < numFrames; i++){

		if(page_table[i]->pageNo == page_number && !page_table[i]->empty){
			page_table[i]->modified = 1;
			break;
		}
	}
}

main(int argc, char *argv[]) {
  char *tracename;
  int page_number, frame_no, done;
  int do_line, i;
  int no_events, disk_writes, disk_reads;
  int debugmode;
  enum repl replace;
  int allocated = 0;
  int victim_page;
  unsigned address;
  char rw;

  page Pvictim;
  FILE *trace;

  if (argc < 5) {
    printf(
        "Usage: ./memsim inputfile numberframes replacementmode debugmode \n");
    exit(-1);
  } else {
    tracename = argv[1];
    trace = fopen(tracename, "r");

    if (trace == NULL) {
      printf("Cannot open trace file %s \n", tracename);
      exit(-1);
    }

    numFrames = atoi(argv[2]);
    if (numFrames < 1) {
      printf("Frame number must be at least 1\n");
      exit(-1);
    }

    if (strcmp(argv[3], "lru\0") == 0)
      replace = lru;
    else if (strcmp(argv[3], "rand\0") == 0)
      replace = _random;
    else if (strcmp(argv[3], "clock\0") == 0)
      replace = clock;
    else if (strcmp(argv[3], "fifo\0") == 0)
      replace = fifo;
    else {
      printf("Replacement algorithm must be rand/fifo/lru/clock  \n");
      exit(-1);
    }

    if (strcmp(argv[4], "quiet\0") == 0)
      debugmode = 0;

    else if (strcmp(argv[4], "debug\0") == 0)
      debugmode = 1;
    else {
      printf("Replacement algorithm must be quiet/debug  \n");
      exit(-1);
    }
  }

  done = createMMU(numFrames);
  if (done == -1) {
    printf("Cannot create MMU");
    exit(-1);
  }
  no_events = 0;
  disk_writes = 0;
  disk_reads = 0;

  do_line = fscanf(trace, "%x %c", &address, &rw);

  while (do_line == 2) {
    page_number = address >> pageoffset;
    frame_no = checkInMemory(page_number); /* ask for physical address */

    if (frame_no == -1) {
      disk_reads++; /* Page fault, need to load it into memory */
      if (debugmode) printf("Page fault %8d \n", page_number);

      if (allocated < numFrames) /* allocate it to an empty frame */
      {
        frame_no = allocateFrame(page_number);
        allocated++;
      } else {
        Pvictim = selectVictim(page_number,
                               replace); /* returns page number of the victim */
        frame_no = checkInMemory(
            page_number); /* find out the frame the new page is in */

        if (Pvictim.modified) /* need to know victim page and modified  */
        {
          disk_writes++;
          if (debugmode) {
            printf("Disk write %8d \n", Pvictim.pageNo);
          }
        } else if (debugmode) {
          printf("Discard    %8d \n", Pvictim.pageNo);
        }
      }
    }
    if (rw == 'R') {
      if (debugmode) {
        printf("reading    %8d \n", page_number);
      }
    } else if (rw == 'W') {
      // mark page in page table as written - modified
	  modifyPage(page_number);
      if (debugmode) {
        printf("writting   %8d \n", page_number);
      }
    } else {
      printf("Badly formatted file. Error on line %d\n", no_events + 1);
      exit(-1);
    }

    no_events++;
    do_line = fscanf(trace, "%x %c", &address, &rw);
  }

  printf("total memory frames:  %d\n", numFrames);
  printf("events in trace:      %d\n", no_events);
  printf("total disk reads:     %d\n", disk_reads);
  printf("total disk writes:    %d\n", disk_writes);
  printf("page fault rate:      %.4f\n", (float)disk_reads / no_events);
}
