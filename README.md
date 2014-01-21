obtain_gs_info
=====================

<< bifit_binary_parser: >>
Parsing DWARF debug information from the application binary to get memory address 
and memory size information for global data.

Usage:
   bifit_binary_parser app_binary_file_path app_keyword language_name(fortran or c)
   e.g.,
   bifit_binary_parser nek/exampels/vortex/nek5000 nek fortran 

About app_keyword:
We only consider application specific data objects. The data objects from
system libraries or from any third-party libary are not considered.
For application specific data objects, their DWARF debug information,
such as DW_AT_decl_file, has application specific key words.
Leveraging these key words, we are able to distinguish application specific data objects
from other unrelated data objects. 

<< bifit_preproc >>
Pre-processing the memory data object information before fault injection.
This program sorts data objects based on their size, and consolidates
those data objects that have overlapped memory regions. 
The input memory data object information should be the output from the program bifit_binary_parser. 

Usage:
   bifit_preproc input_file_path mem_size_cut
   e.g., 
   bifit_preproc nek5000_vortex_global.txt 1048576

About mem_size_cut:
Only the data object that has its memory size larger than mem_size_cut will be 
output as a valid data object. 
