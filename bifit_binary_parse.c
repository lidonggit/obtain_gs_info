/*
   This program outputs the starting addresses and sizes of global data by analyzing application binary.
   The program is implemented based on dwarf-20110113/dwarfexample/simplereader.c and dwarf-20110113/dwarfdump/print_die.c

   Author: Dong Li (lid1@ornl.gov)
*/

#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>   
#include <stdlib.h> 
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "dwarf.h"
#include "libdwarf.h"

//#define MY_DEBUG

struct srcfilesdata {
  char ** srcfiles;
  Dwarf_Signed srcfilescount;
  int srcfilesres;
};

static void read_cu_list(Dwarf_Debug dbg);
static void print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,int level,
    struct srcfilesdata *sf);
static void get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level,
    struct srcfilesdata *sf);
static void resetsrcfiles(Dwarf_Debug dbg,struct srcfilesdata *sf);

const char *keyword = "<stdin>";
const char *lang_name = "<stdin>";
 
int main(int argc, char **argv)
{

  Dwarf_Debug dbg = 0;
  int fd = -1;
  const char *filepath = "<stdin>";
  int res = DW_DLV_ERROR;
  Dwarf_Error error;
  Dwarf_Handler errhand = 0;
  Dwarf_Ptr errarg = 0;

  if(argc != 4) 
  {
    printf("Usage: obtain_static_global_variables binary_file_path app_keyword language_name(fortran or c)\n");
    exit(1);
  } 
  else 
  {
    filepath = argv[1];
    fd = open(filepath,O_RDONLY);
    keyword = argv[2];
    lang_name = argv[3];
    #ifdef MY_DEBUG
    printf("filepath=%s, keyword=%s, lang_name=%s\n", filepath, keyword, lang_name);
    #endif
  }

  if(fd < 0) 
  {
    printf("Failure attempting to open \"%s\"\n",filepath);
    exit(1);
  }
  res = dwarf_init(fd,DW_DLC_READ,errhand,errarg, &dbg,&error);
  if(res != DW_DLV_OK) {
    printf("Giving up, cannot do DWARF processing\n");
    exit(1);
  }

  read_cu_list(dbg);
  res = dwarf_finish(dbg,&error);
  if(res != DW_DLV_OK) {
    printf("dwarf_finish failed!\n");
  }
  close(fd);
  return 0;
}

  static void
read_cu_list(Dwarf_Debug dbg)
{
  Dwarf_Unsigned cu_header_length = 0;
  Dwarf_Half version_stamp = 0;
  Dwarf_Unsigned abbrev_offset = 0;
  Dwarf_Half address_size = 0;
  Dwarf_Unsigned next_cu_header = 0;
  Dwarf_Error error;
  int cu_number = 0;

  for(;;++cu_number) {
    struct srcfilesdata sf;
    sf.srcfilesres = DW_DLV_ERROR;
    sf.srcfiles = 0;
    sf.srcfilescount = 0;
    Dwarf_Die no_die = 0;
    Dwarf_Die cu_die = 0;
    int res = DW_DLV_ERROR;
    res = dwarf_next_cu_header(dbg,&cu_header_length,
	&version_stamp, &abbrev_offset, &address_size,
	&next_cu_header, &error);
    if(res == DW_DLV_ERROR) {
      printf("Error in dwarf_next_cu_header\n");
      exit(1);
    }
    if(res == DW_DLV_NO_ENTRY) {
      /* Done. */
      return;
    }
    /* The CU will have a single sibling, a cu_die. */
    res = dwarf_siblingof(dbg,no_die,&cu_die,&error);
    if(res == DW_DLV_ERROR) {
      printf("Error in dwarf_siblingof on CU die \n");
      exit(1);
    }
    if(res == DW_DLV_NO_ENTRY) {
      /* Impossible case. */
      printf("no entry! in dwarf_siblingof on CU die \n");
      exit(1);
    }
    get_die_and_siblings(dbg,cu_die,0,&sf);
    dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
    resetsrcfiles(dbg,&sf);
  }
}

  static void
get_die_and_siblings(Dwarf_Debug dbg, Dwarf_Die in_die,int in_level,
    struct srcfilesdata *sf)
{
  int res = DW_DLV_ERROR;
  Dwarf_Die cur_die=in_die;
  Dwarf_Die child = 0;
  Dwarf_Error error;

  print_die_data(dbg,in_die,in_level,sf);

  for(;;) {
    Dwarf_Die sib_die = 0;
    res = dwarf_child(cur_die,&child,&error);
    if(res == DW_DLV_ERROR) {
      printf("Error in dwarf_child , level %d \n",in_level);
      exit(1);
    }
    if(res == DW_DLV_OK) {
      get_die_and_siblings(dbg,child,in_level+1,sf);
    }
    /* res == DW_DLV_NO_ENTRY */
    res = dwarf_siblingof(dbg,cur_die,&sib_die,&error);
    if(res == DW_DLV_ERROR) {
      printf("Error in dwarf_siblingof , level %d \n",in_level);
      exit(1);
    }
    if(res == DW_DLV_NO_ENTRY) {
      /* Done at this level. */
      break;
    }
    /* res == DW_DLV_OK */
    if(cur_die != in_die) {
      dwarf_dealloc(dbg,cur_die,DW_DLA_DIE);
    }
    cur_die = sib_die;
    print_die_data(dbg,cur_die,in_level,sf);
  }
  return;
}

static void
get_number(Dwarf_Attribute attr,Dwarf_Unsigned *val)
{
  Dwarf_Error error = 0;
  int res;
  Dwarf_Signed sval = 0;
  Dwarf_Unsigned uval = 0;
  res = dwarf_formudata(attr,&uval,&error);
  if(res == DW_DLV_OK) {
    *val = uval;
    return;
  }
  res = dwarf_formsdata(attr,&sval,&error);
  if(res == DW_DLV_OK) {
    *val = sval;
    return;
  }
  return;
}

 Dwarf_Unsigned
get_location(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Attribute attr)
{
  Dwarf_Locdesc *llbuf = 0;
  Dwarf_Locdesc **llbufarray = 0;
  Dwarf_Signed no_of_elements;
  Dwarf_Error err;
  int i;
  int lres = 0;
  Dwarf_Unsigned opd1 = 0;

  lres = dwarf_loclist_n(attr, &llbufarray, &no_of_elements, &err);
  if (lres == DW_DLV_ERROR)
    printf("dwarf_loclist error\n");
  if (lres == DW_DLV_NO_ENTRY)
    return 0;

#ifdef MY_DEBUG
  if (no_of_elements != 1)
    printf("no_of_elements error\n");
#endif

  llbuf = llbufarray[0];

  //for(i = 0; i < llbuf->ld_cents; i++)
  {
    //Dwarf_Loc * op = &llbuf->ld_s[i];
    Dwarf_Loc * op = &llbuf->ld_s[0];
    if(op->lr_atom == DW_OP_addr)
      opd1 = op->lr_number;
  }

  for (i = 0; i < no_of_elements; ++i) {
    dwarf_dealloc(dbg, llbufarray[i]->ld_s, DW_DLA_LOC_BLOCK);
    dwarf_dealloc(dbg, llbufarray[i], DW_DLA_LOCDESC);
  }
  dwarf_dealloc(dbg, llbufarray, DW_DLA_LIST);
  return opd1;
}

  static void
resetsrcfiles(Dwarf_Debug dbg,struct srcfilesdata *sf)
{
  Dwarf_Signed sri = 0;
  for (sri = 0; sri < sf->srcfilescount; ++sri) {
    dwarf_dealloc(dbg, sf->srcfiles[sri], DW_DLA_STRING);
  }
  dwarf_dealloc(dbg, sf->srcfiles, DW_DLA_LIST);
  sf->srcfilesres = DW_DLV_ERROR;
  sf->srcfiles = 0;
  sf->srcfilescount = 0;
}

  int
is_location_form(int form)
{
  if(form == DW_FORM_block1 ||
      form == DW_FORM_block2 ||
      form == DW_FORM_block4 ||
      form == DW_FORM_block ||
      form == DW_FORM_data4 ||
      form == DW_FORM_data8 ||
      form == DW_FORM_sec_offset){
    return 1;  //true
  }
  return 0;   //false
}

static int get_mem_region_size(Dwarf_Debug dbg, Dwarf_Die die_for_check)
{

  Dwarf_Attribute *attrbuf = 0;
  Dwarf_Signed attrcount = 0;
  Dwarf_Error err;
  Dwarf_Unsigned i, j;
  Dwarf_Unsigned mem_size = 0, type_size = 0;
  int array_dimension_range = 1;
  Dwarf_Half tag = 0;

  int res = dwarf_tag(die_for_check,&tag,&err);
  if(res != DW_DLV_OK){
    printf("Error in dwarf_tag (get_mem_region_size)\n");
    exit(1);
  }

  res = dwarf_attrlist(die_for_check,&attrbuf,&attrcount,&err);
  if(res != DW_DLV_OK) {
    printf("error in get attrlist (get_mem_region_size)\n");
    exit(1);
  }


  if(tag == DW_TAG_base_type || tag == DW_TAG_structure_type ||
      tag == DW_TAG_string_type || tag == DW_TAG_enumeration_type ||
      tag == DW_TAG_union_type || tag == DW_TAG_class_type)
  {
    mem_size = 0;
    for(i = 0; i < attrcount ; ++i) {
      Dwarf_Half aform;
      res = dwarf_whatattr(attrbuf[i],&aform,&err);
      if(aform == DW_AT_byte_size){
	get_number(attrbuf[i],&mem_size);
	break;
      }
      dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
    }
    if(!mem_size){ //some exceptions may be there
      printf("Error in getting mem size for base type or "
	  "structure type or string_type or union type\n");
      exit(1);
    }
    else
      return mem_size;
  }
  else if(tag == DW_TAG_array_type)
  {
    mem_size = 0;
    for(i = 0; i < attrcount ; ++i) {
      Dwarf_Half aform;
      res = dwarf_whatattr(attrbuf[i],&aform,&err);
      if(aform == DW_AT_byte_size){
	get_number(attrbuf[i],&mem_size);
	break;
      }
    }
    if(mem_size)
      return mem_size;
    else{
      //get dimension subranges from the child
      Dwarf_Die child = 0, current_die = 0, next_die=0;
      array_dimension_range = 1;
      res = dwarf_child(die_for_check, &child, &err);
      current_die = child;
      for(;;){
	//dwarf_child(die_for_check, &child, &err);
	if(res == DW_DLV_NO_ENTRY)
	  break;
	if(res == DW_DLV_ERROR){
	  printf("Error in dwarf_child for DW_TAG_array_type)\n");
	  exit(1);
	}
	if(res == DW_DLV_OK){
	  //get lower_bound and upper_bound
	  Dwarf_Attribute *current_attrbuf = 0;
	  Dwarf_Signed current_attrcount = 0;
	  dwarf_attrlist(current_die,&current_attrbuf,&current_attrcount,&err);
	  Dwarf_Unsigned lower_bound, upper_bound = 0;  
	  if(strstr(lang_name, "c"))
	     lower_bound = 0;
	  else if(strstr(lang_name, "fortran"))
	     lower_bound = 1;
	  
          for(j=0; j< current_attrcount; j++){
	    Dwarf_Half current_aform;
	    res = dwarf_whatattr(current_attrbuf[j],&current_aform,&err);
	    if(current_aform == DW_AT_upper_bound)
	      get_number(current_attrbuf[j], &upper_bound);
	    if(current_aform == DW_AT_lower_bound)
	      get_number(current_attrbuf[j], &lower_bound);
	    dwarf_dealloc(dbg,current_attrbuf[j],DW_DLA_ATTR);
	  }
	  if(upper_bound >= lower_bound){
	    array_dimension_range = (upper_bound - lower_bound + 1)*
	      array_dimension_range;
	  }
	}
	res = dwarf_siblingof(dbg, current_die, &next_die, &err);
	if(res == DW_DLV_ERROR)
	{
	  printf("Error in dwarf_siblingof (position mark 1)\n");
	  exit(1);
	}

	if(res == DW_DLV_NO_ENTRY){
	  break;
	}
	if(next_die == child)
	  break;
	else
	  current_die = next_die;
      } //for(;;)

      if(array_dimension_range < 1){
	printf("Error in getting subrange for the array\n");
	exit(1);
      }

      //get type size
      for(i=0; i<attrcount; i++)
      {
	Dwarf_Half aform;
	res = dwarf_whatattr(attrbuf[i],&aform,&err);
	if(res == DW_DLV_OK){
	  if(aform == DW_AT_type)
	  {
	    Dwarf_Off cu_offset, attrib_offset;
	    dwarf_die_CU_offset(die_for_check, &cu_offset, &err);
	    dwarf_global_formref(attrbuf[i], &attrib_offset, &err);
	    dwarf_offdie(dbg, attrib_offset,
		&die_for_check, &err);
	    type_size = get_mem_region_size(dbg, die_for_check);
	    break;
	  }
	}
	dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
      }
    }

    if(type_size)
    {
      mem_size = type_size * array_dimension_range;
      return mem_size;
    }
    else{
      printf("Error in getting array type size\n");
      exit(1);
    }

  }  //TAG_array_type
  else if(tag == DW_TAG_pointer_type){
    return -1;
  }
  else if(tag == DW_TAG_variable){
    /*Dong: this shouldn't happen, because DW_AT_type usually should not point to a DW_TAG_variable. 
      But if it does happens, return -2 to notify print_die_data */
    return -2;
  }
  else if(tag == DW_TAG_typedef || tag == DW_TAG_const_type || tag == DW_TAG_reference_type){
    Dwarf_Die new_die_for_check;
    Dwarf_Off attrib_offset;
    for(i = 0; i<attrcount; i++)
    {
      Dwarf_Half aform;
      res = dwarf_whatattr(attrbuf[i],&aform,&err);
      if(res == DW_DLV_OK) {
	if(aform == DW_AT_type){
	  dwarf_global_formref(attrbuf[i], &attrib_offset, &err);
	  dwarf_offdie(dbg, attrib_offset, &new_die_for_check, &err);
	  break;
	}
      } //if res
      dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
    } //for
    mem_size = get_mem_region_size(dbg, new_die_for_check);
  }
  else{
    printf("Abnormal. new type found\n");
    exit(1);
  }

  return mem_size;
}

  static void
print_die_data(Dwarf_Debug dbg, Dwarf_Die print_me,int level,
    struct srcfilesdata *sf)
{
  char *name = 0;
  Dwarf_Error error = 0;
  Dwarf_Half tag = 0;
  const char *tagname = 0;
  int localname = 0;

  int res = dwarf_diename(print_me,&name,&error);
  if(res == DW_DLV_ERROR) {
    printf("Error in dwarf_diename , level %d \n",level);
    exit(1);
  }
  if(res == DW_DLV_NO_ENTRY) {
    name = "<no DW_AT_name attr>";
    localname = 1;
  }
  res = dwarf_tag(print_me,&tag,&error);
  if(res != DW_DLV_OK) {
    printf("Error in dwarf_tag , level %d \n",level);
    exit(1);
  }
  res = dwarf_get_TAG_name(tag,&tagname);
  if(res != DW_DLV_OK) {
    printf("Error in dwarf_get_TAG_name , level %d \n",level);
    exit(1);
  }

  /*Dong: DW_TAG_common_block is broken into DW_TAG_variable, which is processed later.*/
  if(tag == DW_TAG_pointer_type || tag == DW_TAG_formal_parameter || tag == DW_TAG_common_block)
    return;

  Dwarf_Attribute *attrbuf = 0;
  Dwarf_Signed attrcount = 0;
  Dwarf_Unsigned i;
  Dwarf_Unsigned mem_addr = 0;
  Dwarf_Unsigned filenum = 0;
  Dwarf_Unsigned linenum = 0;
  char *filename = 0;
  Dwarf_Error err;
  Dwarf_Die die_for_check;
  Dwarf_Off cu_offset, attrib_offset;
  res = dwarf_attrlist(print_me,&attrbuf,&attrcount,&error);
  if(res != DW_DLV_OK) {
    return;
  }
  sf->srcfilesres = dwarf_srcfiles(print_me,&sf->srcfiles,&sf->srcfilescount,
      &error);

  int interested = 0;
  for(i = 0; i < attrcount ; ++i) 
  {
    Dwarf_Half aform;
    res = dwarf_whatattr(attrbuf[i],&aform,&error);

    //parse the attributions
    if(res == DW_DLV_OK) 
    {
      if(aform == DW_AT_decl_file)
      {
	get_number(attrbuf[i],&filenum);
	if((filenum > 0) && (sf->srcfilescount > (filenum-1))) {
	  filename = sf->srcfiles[filenum-1];
	}
	if(filename)
	{
	  if(strstr(filename, keyword))
	    interested++;
	}
      }
      if(aform == DW_AT_decl_line)
      {
	get_number(attrbuf[i],&linenum);
	if(linenum)
	  interested++;
      }
      if(aform == DW_AT_location)
      {
	Dwarf_Half theform = 0;
	Dwarf_Half directform = 0;
	dwarf_whatform(attrbuf[i], &theform, &err);
	dwarf_whatform_direct(attrbuf[i], &directform, &err);
	if(is_location_form(theform)){
	  mem_addr = get_location(dbg, print_me, attrbuf[i]);
	}
	if(mem_addr)
	  interested++;
      }
      if(aform == DW_AT_type)
      {
	dwarf_die_CU_offset(print_me, &cu_offset, &err);
	dwarf_global_formref(attrbuf[i], &attrib_offset, &err);
	dwarf_offdie(dbg, attrib_offset,
	    &die_for_check, &err);
	if(die_for_check)
	  interested++;
	else
	{
	  printf("Error in getting type info\n");
	  exit(1);
	}
      }
    }
    else
      interested = 0;
    dwarf_dealloc(dbg,attrbuf[i],DW_DLA_ATTR);
  }
  dwarf_dealloc(dbg,attrbuf,DW_DLA_LIST);

  if(interested == 4){
    int mem_size = get_mem_region_size(dbg, die_for_check);
    if(mem_size > 0)
    {
      #ifdef MY_DEBUG
      //printing all information
      printf("name %s  file %s  line %" DW_PR_DUu "  addr 0x%" DW_PR_DUx "  size %d\n",
	name, filename, linenum, (Dwarf_Unsigned)mem_addr, mem_size); 
      #else
      //just printing addr and size
      printf("0x%" DW_PR_DUx "\n", (Dwarf_Unsigned)mem_addr);
      printf("%d\n", mem_size);
      #endif
    }
    else if(mem_size == -1){  //pointer type
      return;
    }
    else if (mem_size == -2){ //variable type
      printf("get_mem_region_size (return value is -2). Error: name %s  file %s  line %" DW_PR_DUu "  addr 0x%" DW_PR_DUx "\n",
	  name, filename, linenum, (Dwarf_Unsigned)mem_addr);
      return;
    }
    else if(!mem_size){
      printf("Error mem_size=0\n");
      exit(1);
    }
  }

  if(!localname) {
    dwarf_dealloc(dbg,name,DW_DLA_STRING);
  }
}

