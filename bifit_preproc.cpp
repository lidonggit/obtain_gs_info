#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <fstream>

#define MY_DEBUG

#define MAX(a,b) \
  ({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a > _b ? _a : _b;})

#define MIN(a,b) \
  ({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b); _a < _b ? _a : _b;})

using namespace std;

typedef struct memreginfo {
  int size;
  unsigned int addr; 
  string other_info;
  bool valid_flag;  //flag for adjusting memory record
}MEMREF;

void adjust_mem_record(MEMREF *item, MEMREF *mem_record, int mem_record_counter)
{
  bool adjust_flag; 
  int round_counter = 0;

  while(1)
  {
    round_counter++;
    adjust_flag = false;
    for(int i=0; i < mem_record_counter; i++)
    {
      if(mem_record[i].valid_flag == false)
	continue; 

      if(item->addr <= mem_record[i].addr && 
	  item->addr+item->size >= mem_record[i].addr + mem_record[i].size)
      {
	mem_record[i].valid_flag = false; //invalidate the record
	if((item->other_info.find(mem_record[i].other_info) == string::npos) && 
	    (mem_record[i].other_info.find(item->other_info) == string::npos))
	  item->other_info = item->other_info + mem_record[i].other_info;
	else if((item->other_info.find(mem_record[i].other_info) == string::npos) &&
	    (mem_record[i].other_info.find(item->other_info) != string::npos))
	  item->other_info = mem_record[i].other_info;

	adjust_flag = true; 
	continue;
      }

      if((item->addr >= mem_record[i].addr && 
	    item->addr < mem_record[i].addr + mem_record[i].size -1)
	  || (item->addr < mem_record[i].addr && 
	    item->addr+item->size-1 >= mem_record[i].addr))
      {
	item->size = MAX(item->addr+item->size-1, mem_record[i].addr+mem_record[i].size-1) - MIN(item->addr, mem_record[i].addr) + 1;
	item->addr = MIN(item->addr, mem_record[i].addr);
	if((item->other_info.find(mem_record[i].other_info) == string::npos) &&
	    (mem_record[i].other_info.find(item->other_info) == string::npos))
	  item->other_info = item->other_info + mem_record[i].other_info; 
	else if((item->other_info.find(mem_record[i].other_info) == string::npos) &&
	    (mem_record[i].other_info.find(item->other_info) != string::npos))
	  item->other_info = mem_record[i].other_info;

	adjust_flag = true;
	mem_record[i].valid_flag = false;
	break;
      }
    }  //for loop

    if(adjust_flag)
      continue;
    else
      break;
  } //while

  return;
}

int main(int argc, char *argv[])
{
  char *filepath;
  int mem_size_cut = 0;

  if(argc != 3)
  {
    printf("Usage: bifit_preproc file_path mem_size_cut\n");
    exit(1);
  }
  else
  {
    filepath = argv[1];
    mem_size_cut = (int)atol(argv[2]); 
  }

  //read data object info into the memory
  ifstream sg_input;
  sg_input.open(filepath);
  if(!sg_input)
  {
    cerr << "Error: could not open the input file " << filepath
      << endl; 
  } 	

  int item_counter = 0;
  string obj_info;
  while(getline(sg_input, obj_info))
    ++item_counter; 

  cout << "***** There are " << item_counter << " data objects *****" << endl; 

  MEMREF *mr_temp, *mr_record;
  mr_temp = new MEMREF[item_counter];
  mr_record = new MEMREF[item_counter]; 

  sg_input.clear();
  sg_input.seekg(0, ios::beg);
  for(int i=0; i<item_counter; i++)
  {
    std::getline(sg_input, obj_info);
    //get addr and size
    size_t addr_pos = obj_info.find("addr");
    size_t size_pos = obj_info.find("size"); 

    string obj_size = obj_info.substr(size_pos+4);  
    string obj_addr = obj_info.substr(addr_pos+4, size_pos-1-(addr_pos+4)+1);
    mr_temp[i].size = strtol(obj_size.c_str(), NULL, 0);
    mr_temp[i].addr = (unsigned int)strtol(obj_addr.c_str(), NULL, 0);
    mr_temp[i].other_info = obj_info.substr(0, addr_pos-1);
  }
  sg_input.close();

  //consolidating the overlapped memory regions
  bool adjust_flag = false;
  int mem_record_counter=0;
  for(int i=0; i<item_counter; i++)
  {
    adjust_flag = false;
    adjust_mem_record(mr_temp+i, mr_record, mem_record_counter);

    /*add into mem_record*/
    mr_record[mem_record_counter].addr = mr_temp[i].addr;
    mr_record[mem_record_counter].size = mr_temp[i].size;
    mr_record[mem_record_counter].other_info = mr_temp[i].other_info;
    mr_record[mem_record_counter].valid_flag = true;  
    mem_record_counter++;
  } //for loop

  int valid_record_counter = 0;
  for(int i=0; i< mem_record_counter; i++)
  {
    if(mr_record[i].valid_flag)
    {
      valid_record_counter++;
      /*cout << mr_record[i].other_info << "  addr " << hex << mr_record[i].addr 
	<< dec << "   size " << mr_record[i].size << endl; */
    }
  }  
  cout << "***** Collected " << valid_record_counter << " valid mem records in total (mem_size_cut=1048976)*****" << endl;

  //gather consolidated memory region information
  MEMREF valid_record[valid_record_counter];
  int temp_counter=0;
  for(int i=0; i < mem_record_counter; i++)
  {
    if(mr_record[i].valid_flag)
    {     
      valid_record[temp_counter].size = mr_record[i].size;
      valid_record[temp_counter].addr = mr_record[i].addr; 
      valid_record[temp_counter].other_info = mr_record[i].other_info;
      temp_counter++;
    }
  }

  //bubble sorting
  MEMREF temp_record;
  for (int i=0 ; i <(valid_record_counter - 1); i++)
  {
    for (int j=0 ; j < valid_record_counter - i - 1; j++)
    {
      if (valid_record[j].size < valid_record[j+1].size) /* For decreasing order use < */
      {
	temp_record.size = valid_record[j].size;
	temp_record.addr = valid_record[j].addr;
	temp_record.other_info = valid_record[j].other_info;

	valid_record[j].size = valid_record[j+1].size;
	valid_record[j].addr = valid_record[j+1].addr;
	valid_record[j].other_info = valid_record[j+1].other_info;

	valid_record[j+1].size = temp_record.size;
	valid_record[j+1].addr = temp_record.addr;
	valid_record[j+1].other_info = temp_record.other_info;
      }
    }
  }

  //output
  for(int i=0; i<valid_record_counter; i++)
  {
    if(valid_record[i].size > mem_size_cut)
      cout << valid_record[i].other_info << "  addr " << hex << valid_record[i].addr 
	<< dec << "   size " << valid_record[i].size << endl; 
  }
}
