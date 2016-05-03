This program stores data entered into a hash table.  The hash table is implemented as an 
array of linked lists with the first bucket element containing data.

Written in C originally with CentOS 7.


To compile the program:

cc hash-table.c -o hash-table


To run the program, these are sample commands:

./hash-table --hashsize 26 --debug

./hash-table --hashsize 5


--hashsize is required, but --debug is optional

