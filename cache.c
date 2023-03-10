/*
 * 
 * cache.c
 * 
 * Donald Yeung
 */


#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE; 
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static Pcache icache;
static Pcache dcache;
static Pcache ucache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;

/************************************************************/
void set_cache_param(param, value)
  int param;
  int value;
{

  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }

}

void init_cache()
{ 
  int i;
  // Building unified Cache
  if (cache_split == 0){
  	ucache = (cache*)malloc(sizeof(cache));   
        ucache->size = cache_usize;
        ucache->associativity = cache_assoc;
        ucache->n_sets = (cache_usize)/(cache_block_size*cache_assoc);
        ucache->set_contents = (int*)malloc(sizeof(int)*ucache->n_sets);
        ucache->LRU_head = (Pcache_line*)malloc(sizeof(Pcache_line)*ucache->n_sets); 
        for (i = 0; i < ucache->n_sets; i++){
            ucache->LRU_head[i] = NULL;   
        }
        ucache->LRU_tail = (Pcache_line*)malloc(sizeof(Pcache_line)*ucache->n_sets); 
        for (i = 0; i < ucache->n_sets; i++){
            ucache->LRU_tail[i] = NULL;
        }
        ucache->index_mask_offset = LOG2(cache_block_size);  
        //ucache->index_mask = pow(2, LOG2(ucache->n_sets) + LOG2(cache_block_size)) - 1;  // convert to hexadecimal mask somehow 
        ucache->index_mask = ((1<<LOG2(ucache->n_sets)) - 1)<<(ucache->index_mask_offset);
  }
  
  // Building split caches 
  if (cache_split == 1){
      icache = (cache*)malloc(sizeof(cache)); 
      icache->size = cache_isize;
      icache->associativity = cache_assoc;
      icache->n_sets = cache_isize/(cache_block_size*cache_assoc);
      icache->set_contents = (int*)malloc(sizeof(int)*icache->n_sets);
      icache->LRU_head = (Pcache_line*)malloc(sizeof(Pcache_line)*icache->n_sets);
      for (i = 0; i < icache->n_sets; i++){
          icache->LRU_head[i] = NULL;
      }
      icache->LRU_tail = (Pcache_line*)malloc(sizeof(Pcache_line)*icache->n_sets);
      for (i = 0; i < icache->n_sets; i++){
          icache->LRU_tail[i] = NULL;
      }
      //icache->index_mask = pow(2, LOG2(icache->n_sets) + LOG2(cache_block_size)) - 1;
      icache->index_mask_offset = LOG2(cache_block_size);
      icache->index_mask = ((1<<LOG2(icache->n_sets)) - 1)<<(icache->index_mask_offset);

      dcache = (cache*)malloc(sizeof(cache));
      dcache->size = cache_dsize;
      dcache->associativity = cache_assoc;
      dcache->n_sets = cache_dsize/(cache_block_size*cache_assoc);
      dcache->set_contents = (int*)malloc(sizeof(int)*dcache->n_sets);
      dcache->LRU_head = (Pcache_line*)malloc(sizeof(Pcache_line)*dcache->n_sets);
      for (i = 0; i < dcache->n_sets; i++){
          dcache->LRU_head[i] = NULL;
      }
      dcache->LRU_tail = (Pcache_line*)malloc(sizeof(Pcache_line)*dcache->n_sets);
      for (i = 0; i < dcache->n_sets; i++){
          dcache->LRU_tail[i] = NULL;
      }
      //dcache->index_mask = pow(2, LOG2(dcache->n_sets) + LOG2(cache_block_size)) - 1;
      dcache->index_mask_offset = LOG2(cache_block_size);
      dcache->index_mask = ((1<<LOG2(dcache->n_sets)) - 1)<<(dcache->index_mask_offset);
  }
 
  // create cache_data_stats struct and initialise: use double pointers
  Pcache_stat cache_stat_inst_addr;
  cache_stat_inst_addr = &cache_stat_inst;   
  cache_stat_inst_addr = (cache_stat*)malloc(sizeof(cache_stat));
  (&cache_stat_inst)->accesses = 0;
  (&cache_stat_inst)->misses = 0;
  (&cache_stat_inst)->replacements = 0;
  (&cache_stat_inst)->demand_fetches = 0;
  (&cache_stat_inst)->copies_back = 0;

  Pcache_stat cache_stat_data_addr;
  cache_stat_data_addr = &cache_stat_data;
  cache_stat_data_addr = (cache_stat*)malloc(sizeof(cache_stat));
  (&cache_stat_data)->accesses = 0;
  (&cache_stat_data)->misses = 0;
  (&cache_stat_data)->replacements = 0;
  (&cache_stat_data)->demand_fetches = 0;
  (&cache_stat_data)->copies_back = 0; 
}

void perform_access(addr, access_type)
  unsigned addr, access_type;
{  
    unsigned tag1; 
    unsigned index;
    int match = 0;    

    if (cache_split == FALSE){
       tag1 = addr>>(ucache->index_mask_offset+LOG2(ucache->n_sets));
       index = (addr & ucache->index_mask)>>ucache->index_mask_offset;
    }

    if (cache_split == TRUE){
       tag1 = addr>>(icache->index_mask_offset+LOG2(icache->n_sets));
       index = (addr & icache->index_mask)>>icache->index_mask_offset;
    }  
    

    Pcache_line temp, temp_insert;
    temp_insert = (cache_line*)malloc(sizeof(cache_line)); // space reserved for the block to be inserted. 
    temp_insert->dirty = 0;
    temp_insert->tag = tag1;
    temp_insert->LRU_next = NULL;
    temp_insert->LRU_prev = NULL; 

    if (cache_split == TRUE){     
       if (access_type == 1){  
           temp = dcache->LRU_head[index];
            
           if (cache_writeback == TRUE && cache_writealloc == TRUE){
               (&cache_stat_data)->accesses++;
               while(temp != NULL){
                  if (temp->tag == tag1){
                    temp_insert->dirty = 1;
                    insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                    delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
               }
               if (match == 0){
                  (&cache_stat_data)->misses++;
                  temp_insert->dirty = 1;
                  (&cache_stat_data)->demand_fetches += (cache_block_size)/4; 
                  if (dcache->set_contents[index] == dcache->associativity){
                     if (dcache->LRU_tail[index]->dirty == 1)
                        (&cache_stat_data)->copies_back+= (cache_block_size)/4;
                     (&cache_stat_data)->replacements++;
                     insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                     delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), dcache->LRU_tail[index]);
                  }
                  else {
                     insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                     dcache->set_contents[index]++;
                  }
               }
           }

           if (cache_writeback == TRUE && cache_writealloc == FALSE){
              (&cache_stat_data)->accesses++;
               while(temp != NULL){
                  if (temp->tag == tag1){
                    temp_insert->dirty = 1;
                    insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                    delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
               }
               if (match == 0){
                  (&cache_stat_data)->misses++;
                  (&cache_stat_data)->copies_back++;
               }
           }

           if (cache_writeback == FALSE && cache_writealloc == TRUE){
              (&cache_stat_data)->copies_back++;
              (&cache_stat_data)->accesses++;
              while(temp != NULL){
                  if (temp->tag == tag1){
                    insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                    delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
              }
              if (match == 0){
                  (&cache_stat_data)->misses++;
                  (&cache_stat_data)->demand_fetches += (cache_block_size)/4;
                  if (dcache->set_contents[index] == dcache->associativity){
                     if (dcache->LRU_tail[index]->dirty == 1)
                        (&cache_stat_data)->copies_back+= (cache_block_size)/4;
                     (&cache_stat_data)->replacements++;
                     insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                     delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), dcache->LRU_tail[index]);
                  }
                  else{
                     insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                     dcache->set_contents[index]++;
                  }
              }
           }

           if (cache_writeback == FALSE && cache_writealloc == FALSE){
              (&cache_stat_data)->copies_back++;
              (&cache_stat_data)->accesses++;
              while(temp != NULL){
                  if (temp->tag == tag1){
                    insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                    delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
              }
              if (match == 0){
                  (&cache_stat_data)->misses++;
                  (&cache_stat_data)->copies_back++;
              }
           }           
        }
    
        if (access_type == 0){
           temp = dcache->LRU_head[index];
           while (temp != NULL){
              if (temp->tag == tag1){
         	 (&cache_stat_data)->accesses++;
                 insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);  
                 delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp);
                 match = 1;
                 break; 
              }
              temp = temp->LRU_next;
           }
           if (match == 0 ){
                 (&cache_stat_data)->accesses++;
                 (&cache_stat_data)->misses++;
                 (&cache_stat_data)->demand_fetches+= (cache_block_size)/4;
                 if (dcache->set_contents[index] == dcache->associativity){
                    (&cache_stat_data)->replacements++;
                    if (dcache->LRU_tail[index]->dirty == 1)
                         (&cache_stat_data)->copies_back+= (cache_block_size)/4;
                    insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert); 
                    delete(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), dcache->LRU_tail[index]);   
                 }
                 else {
                   dcache->set_contents[index]++;
                   insert(&(dcache->LRU_head[index]), &(dcache->LRU_tail[index]), temp_insert);
                 }               
           }   
        }   
    
        if (access_type == 2){
           temp = icache->LRU_head[index];
           while ( temp != NULL){         
              if (temp->tag == tag1){
                 (&cache_stat_inst)->accesses++;
                 insert(&(icache->LRU_head[index]), &(icache->LRU_tail[index]), temp_insert);
                 delete(&(icache->LRU_head[index]), &(icache->LRU_tail[index]), temp);  
                 match = 1;
                 break; 
              }
              temp = temp->LRU_next;
           }
           if (match == 0){
                 (&cache_stat_inst)->accesses++;
                 (&cache_stat_inst)->misses++;
                 (&cache_stat_inst)->demand_fetches+= (cache_block_size)/4;
                 if (icache->set_contents[index] == icache->associativity){
                    (&cache_stat_inst)->replacements++;
                    insert(&(icache->LRU_head[index]), &(icache->LRU_tail[index]), temp_insert);
                    delete(&(icache->LRU_head[index]), &(icache->LRU_tail[index]), icache->LRU_tail[index]);
                 }
                 else {
                   icache->set_contents[index]++;
                   insert(&(icache->LRU_head[index]), &(icache->LRU_tail[index]), temp_insert);
                 } 
           }
        }     
    }

    if (cache_split == FALSE){  
       if (access_type == 0 || access_type == 2){
           temp = ucache->LRU_head[index];   
           while (temp != NULL){ 
              if (temp->tag == tag1){
                 if (access_type == 0) (&cache_stat_data)->accesses++; else (&cache_stat_inst)->accesses++;
                 insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert); 
                 delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                 match = 1;
                 break;
              }
              temp = temp->LRU_next;
           }
           if (match == 0){
                if (access_type == 0) (&cache_stat_data)->accesses++; else (&cache_stat_inst)->accesses++;
                if (access_type == 0) (&cache_stat_data)->misses++;   else (&cache_stat_inst)->misses++;
                if (access_type == 0) (&cache_stat_data)->demand_fetches += (cache_block_size)/4; 
                                           else (&cache_stat_inst)->demand_fetches+= (cache_block_size)/4;
                if (ucache->set_contents[index] == ucache->associativity){
                    if (access_type == 0) (&cache_stat_data)->replacements++;  else (&cache_stat_inst)->replacements++; 
                    insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                    if (ucache->LRU_tail[index]->dirty == 1){
                         (&cache_stat_data)->copies_back += (cache_block_size)/4;  
                         (&cache_stat_data)->copies_back++; // sending addresses also
                    }
                    delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), ucache->LRU_tail[index]);
                }    
                else if (ucache->set_contents[index] < ucache->associativity) {
                   ucache->set_contents[index]++;
                   insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert); 
                }                              
           } 	
       }
 
       else if (access_type == 1){
           temp = ucache->LRU_head[index];   

           if (cache_writeback == TRUE && cache_writealloc == TRUE){
               (&cache_stat_data)->accesses++;
               while(temp != NULL){ ///////////////////////////////BUG HERE///////////////////////////
                  if (temp->tag == tag1){
                    temp_insert->dirty = 1;
                    delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                    //free(temp);
                    insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                    //delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                    //(&cache_stat_data)->copies_back++;
                    match = 1;
                    break; 
                  }
                  temp = temp->LRU_next; 
               }
               if (match == 0){
                  (&cache_stat_data)->misses++; 
                  temp_insert->dirty = 1;     
                  (&cache_stat_data)->demand_fetches += (cache_block_size)/4;
                  if (ucache->set_contents[index] == ucache->associativity){
                     if (ucache->LRU_tail[index]->dirty == 1){
                        (&cache_stat_data)->copies_back+= (cache_block_size)/4;
                        (&cache_stat_data)->copies_back++; // sending addresses also
                     }
                     (&cache_stat_data)->replacements++;
                     insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                     delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), ucache->LRU_tail[index]);
                  }
                  else {
                     insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                     ucache->set_contents[index]++;              
                  }        
               }
           }

           if (cache_writeback == TRUE && cache_writealloc == FALSE){
              (&cache_stat_data)->accesses++;
               while(temp != NULL){ ///////////////////////// BUG HERE ///////////////////////////////
                  if (temp->tag == tag1){
                    temp_insert->dirty = 1;
                    delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                    insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                    //delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
               }
               if (match == 0){
                  (&cache_stat_data)->misses++;
                  (&cache_stat_data)->copies_back++;      
               } 
           }
 
           if (cache_writeback == FALSE && cache_writealloc == TRUE){
              (&cache_stat_data)->copies_back++; 
              (&cache_stat_data)->accesses++;
              while(temp != NULL){
                  if (temp->tag == tag1){
                    insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                    delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
              }
              if (match == 0){
                  (&cache_stat_data)->misses++;
                  (&cache_stat_data)->demand_fetches += (cache_block_size)/4;
                  if (ucache->set_contents[index] == ucache->associativity){
                     if (ucache->LRU_tail[index]->dirty == 1)
                        (&cache_stat_data)->copies_back+= (cache_block_size)/4;
                     (&cache_stat_data)->replacements++;
                     insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                     delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), ucache->LRU_tail[index]);
                  }
                  else{
                     insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                     ucache->set_contents[index]++; 
                  }         
              } 
           }
 
           if (cache_writeback == FALSE && cache_writealloc == FALSE){
              (&cache_stat_data)->copies_back++;
              (&cache_stat_data)->accesses++;
              while(temp != NULL){
                  if (temp->tag == tag1){
                    insert(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp_insert);
                    delete(&(ucache->LRU_head[index]), &(ucache->LRU_tail[index]), temp);
                    match = 1;
                    break;
                  }
                  temp = temp->LRU_next;
              }
              if (match == 0){
                  (&cache_stat_data)->misses++;
                  (&cache_stat_data)->copies_back++;                                
              }                    
           }                               
       }// access_type 
    }//cache_type
}//perform_access


void flush(){
     if (cache_split == FALSE){
        free(ucache->set_contents);
        free(ucache->LRU_head);
        free(ucache->LRU_tail);
        free(ucache);
     }
     if (cache_split == TRUE){
        free(icache->set_contents);
        free(icache->LRU_head);
        free(icache->LRU_tail);
        free(dcache->set_contents);
        free(dcache->LRU_head);
        free(dcache->LRU_tail); 
        free(icache);
        free(dcache);       
     } 
}

void delete(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;
  
  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("Cache Settings:\n");
  if (cache_split) {
    printf("\tSplit I- D-cache\n");
    printf("\tI-cache size: \t%d\n", cache_isize);
    printf("\tD-cache size: \t%d\n", cache_dsize);
  } else {
    printf("\tUnified I- D-cache\n");
    printf("\tSize: \t%d\n", cache_usize);
  }
  printf("\tAssociativity: \t%d\n", cache_assoc);
  printf("\tBlock size: \t%d\n", cache_block_size);
  printf("\tWrite policy: \t%s\n", 
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("\tAllocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
void print_stats()
{
  printf("*** CACHE STATISTICS ***\n");
  printf("  INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  printf("  miss rate: %f\n", 
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf("  DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  printf("  miss rate: %f\n", 
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf("  TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches + 
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/
