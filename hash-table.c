/*********************************************************************************
 * Written by Lance N. Le
 *
 * Free to use.
 * Free to distribute.
 * No warranty, expressed or implied, comes with this program.
 *********************************************************************************/
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <linux/limits.h>




/*********************************************************************************
 * Structure definitions.
 *********************************************************************************/
typedef enum {FALSE, TRUE} boolean;


/*********************************************************************************
 * Command line parameters passed from the shell.
 *********************************************************************************/
typedef struct
{
   boolean bDebug;
   int     nBucketSize;
} strCommandLine;


/*********************************************************************************
 * Basic hash table structure to store data for chaining.
 * This application stores the indexes as a linked list, thus, the hash table is
 * an array of linked lists and uses the bucket, element 0, to store data.
 *
 * We could optimize the linked list by making it bidirectional and sorted, then
 * use a binary search to look up data.  Perhaps a future update.
 *********************************************************************************/
typedef struct _strHashTable
{
   char                  szData[BUFSIZ+1];    /* Data entered by user            */
   struct _strHashTable *pstrNext;            /* Link to next linked list entry  */
} strHashTable;


/*********************************************************************************
 * Function prototypes.
 *********************************************************************************/
int AddEntryToHashTable(strHashTable *, int, const char *);
int Debug(const char *, ...);
int DebugOn(int);
int DeleteEntryFromHashTable(strHashTable *, int, const char *);
int HashFunction(int, const char *);
int ListHashTable(const strHashTable *, int);
strCommandLine *ProcessCommandLine(strCommandLine *, int, char **);
int SearchHashTable(const strHashTable *, int, const char *);




/*********************************************************************************
 * Function: main()
 * Params:   argc
 *           argv
 * Returns:  0 - no errors
 *           Less than 0 means a problem, such as unable to allocate memory
 * Call by:  Shell
 * Call to:  Debug()
 *           DebugOn()
 *           ProcessCommandLine()
 * Overview: Loops through a menu, and add, list, search, or delete from hash
 *           table.
 * Notes:    None
 *********************************************************************************/
int main(int argc, char *argv[])
{
   strCommandLine  strRunOptions;
   strHashTable   *pastrHash        = NULL;
   char            szData[BUFSIZ+1] = "";
   int             nBucketSize      = 0;
   int             nExitCode        = 0;
   int             nMenuChoice      = 0;



   if (argc < 3)
   {
      printf("Not enough parameters\n\n");
      printf("Example: %s --hashsize 26 --debug\n", argv[0]);
      printf("Example: %s --hashsize 5\n\n", argv[0]);
      printf("--debug is the only optional argument.\n");
      exit(1);
   }


   memset(&strRunOptions, 0, sizeof(strRunOptions));
   ProcessCommandLine(&strRunOptions, argc, argv);


   /******************************************************************************
    * First call to DebugOn() will determine whether Debug() will write debug
    * messages for the rest of the program.
    *
    * If bDebug is FALSE then no logging messages are displayed when calling
    * Debug(). 
    * If bDebug is TRUE then logging messages are displayed when calling Debug().
    ******************************************************************************/
   DebugOn(strRunOptions.bDebug);

   Debug("Hash size: %d; Debug: %s.\n\n",
         strRunOptions.nBucketSize,
         strRunOptions.bDebug==TRUE?"On":"Off");


   nBucketSize = strRunOptions.nBucketSize;


   /******************I***********************************************************
    * Create the hash table based on the command line.
    * For example, if --hashsize 26, then create a hash table for 26 buckets.
    ******************I***********************************************************/
   pastrHash = (strHashTable *) calloc(nBucketSize, sizeof(strHashTable));

   if (pastrHash == NULL)
   {
      fprintf(stderr, "Failed calloc() in main(). errno=%d.\n", errno);
      exit(-1);
   }


   for (nMenuChoice=0; nMenuChoice != 5; memset(szData,0,sizeof(szData)))
   {
      printf("   [1] Enter new data\n");
      printf("   [2] List table\n");
      printf("   [3] Search data\n");
      printf("   [4] Delete data\n");
      printf("   [5] Quit\n");
      printf("   Choice:  ");


      /**************************************************************************
       * Read the user choice AND the newline character with the scanf(),
       * otherwise, it will be left in the input buffer causing fgets() problems.
       * fflush(stdin) may not work in certain Linux/Unix implementations.
       **************************************************************************/
      scanf("%d%*c", &nMenuChoice);

      switch(nMenuChoice)
      {
         case 1:
            printf("Enter new data:  ");

            if (fgets(szData, sizeof(szData), stdin) != NULL)
            {
               szData[strcspn(szData, "\r\n")] = 0;  /* Strip trailing new line */

               Debug("New data to add: [%s]\n", szData);

               AddEntryToHashTable(pastrHash, nBucketSize, szData);
            }
            break;

         case 2: 
            ListHashTable(pastrHash, nBucketSize);
            break;

         case 3:
            printf("Search data:  ");

            if (fgets(szData, sizeof(szData), stdin) != NULL)
            {
               szData[strcspn(szData, "\r\n")] = 0;

               Debug("Data to search: [%s]\n", szData);

               SearchHashTable(pastrHash, nBucketSize, szData);
            }
            break;

         case 4:
            printf("Data to delete:  ");

            if (fgets(szData, sizeof(szData), stdin) != NULL)
            {
               szData[strcspn(szData, "\r\n")] = 0;

               Debug("Data to delete: [%s]\n", szData);

               DeleteEntryFromHashTable(pastrHash, nBucketSize, szData);
            }
            break;

         case 5:
            free(pastrHash);
            break;

         default:
            printf("Invalid option, please try again\n");
            break;
      }
   }


   exit(nExitCode);
}




/********************************************************************************
 * Function: AddEntrytoHashTable
 * Params:   pastrHash - array of buckets
 *           nSize - number of buckets in hash table
 *           pszData - data to add
 * Returns:  0 - data added successfully
 *           greater than 0 - data already exists
 *           less than 0 - error, normally cannot allocate memory
 * Call by:  main()
 * Call to:  HashFunction()
 * Overview: Inserts the data into the hash table by finding the appropriate
 *           bucket. If data already exist, do not add.
 * Notes:    Freeing memory for each linked list chain is done by
 *           DeleteEntryFromHashTable().
 ********************************************************************************/
int AddEntryToHashTable(strHashTable *pastrHash, int nSize, const char *pszData)
{
   boolean       bFirstChainHasData = FALSE;
   int           nHashIndex         = 0;
   int           nReturnCode        = 0;
   strHashTable *pstrCurrent        = NULL;
   strHashTable *pstrNewChain       = NULL;
   strHashTable *pstrPrevious       = NULL;



   Debug("Inside AddEntryToHashTable()\n");

   nHashIndex = HashFunction(nSize, pszData);

   Debug("Hash index for [%s] is bucket [%d]\n", pszData, nHashIndex);


   /****************************************************************************
    * Loop through through the linked list in the bucket until an empty entry is
    * found.  The empty entry will be used to store the new data.
    *
    * If data already exists, the return immediately.
    ****************************************************************************/
   for (pstrCurrent  = &pastrHash[nHashIndex];
        pstrCurrent != NULL;
        pstrCurrent  = pstrCurrent->pstrNext)
   {
      pstrPrevious = pstrCurrent;

      if (pstrCurrent->szData[0] == '\0')
      {
          break;                              /* This chain is available for new */
      }
      else if (strcmp(pstrCurrent->szData, pszData) == 0)
      {
          return(1);                          /* Data already exists             */
      }
      else
      {
          bFirstChainHasData = TRUE;
      }
   }


   /****************************************************************************
    * At this point, pstrCurrent should point to the first link without data.
    * Allocate memory to store create a new linked list chain and store data.
    *
    * Freeing linked list chain memory is done by DeleteEntryFromHashTable().
    ****************************************************************************/
   pstrNewChain = (strHashTable *) calloc(1, sizeof(strHashTable));

   if (pstrNewChain == NULL)
   {
      fprintf(stderr, "Failed calloc() in AddEntryToHashTable(). errno=%d.\n", errno);
      nReturnCode = -1;
   }
   else
   {
      pstrNewChain->pstrNext = NULL;
      strncpy(pstrNewChain->szData, pszData, sizeof(pstrNewChain->szData)-1);


      /************************************************************************
       * If the first chain/element in the bucket is empty, we want to copy the
       * new content.  Otherwise, we link to it.
       ************************************************************************/
      if (bFirstChainHasData == FALSE)
      {
         memcpy(pstrPrevious, pstrNewChain, sizeof(strHashTable));
      }
      else
      {
         pstrPrevious->pstrNext = pstrNewChain;
      }
   }


   return(nReturnCode);
}




/********************************************************************************
 * Function: DeleteEntryFromHashTable
 * Params:   pastrHash - array of buckets
 *           nSize - number of buckets in hash table
 *           pszData - data to search for in hash table to remove if found
 * Returns:  0  - data found and deleted
 *           >0 - data not found to delete
 *           <0 - error attempting to delete, such as not able to free memory
 * Call by:  main()
 * Call to:  HashFunction()
 * Overview: Searchs the hash table by finding the correct bucket index, and then
 *           traverse the linked list to find the data to delete.
 *           Shift the linked list pointer around the deleted element and free
 *           the memory of deleted record.
 *           If the data is found in the first chain of the bucket, empty the
 *           data, but do not free the memory of bucket head.
 * Notes:    Memory for each linked list chain is done in AddEntryToHashTable().
 *           The bucket head (first chain in linked list) is not freed.
 ********************************************************************************/
int DeleteEntryFromHashTable(strHashTable *pastrHash, int nSize, const char *pszData)
{
   boolean       bFirstChain  = TRUE;
   int           nHashIndex   = 0;
   int           nReturnCode  = 1;
   strHashTable *pstrCurrent  = NULL;
   strHashTable *pstrPrevious = NULL;


   Debug("Inside DeleteEntryFromHashTable()\n");


   nHashIndex = HashFunction(nSize, pszData);


   for (pstrCurrent  = &pastrHash[nHashIndex];
        pstrCurrent != NULL;
        pstrPrevious = pstrCurrent, pstrCurrent = pstrCurrent->pstrNext)
   {
      Debug("Comparing [%s] with [%s]\n", pstrCurrent->szData, pszData);

      if (strcmp(pstrCurrent->szData, pszData) != 0)
      {
         bFirstChain = FALSE;
      }
      else
      {
         Debug("Found [%s] in bucket [%d]\n", pszData, nHashIndex);


         /***********************************************************************
          * If data is found in the bucket head, reset the data, but do not free
          * any memory.  Each bucket head will always be around.
          ***********************************************************************/
         if (bFirstChain == TRUE)
         {
            memset(pstrCurrent->szData, 0, sizeof(pstrCurrent->szData));
         }

         /***********************************************************************
          * If data is found anywhere in the linked list except the bucket head
          * (first element of linked list), then rearrange linked list and free
          * the memory of data found.
          ***********************************************************************/
         else
         {
            if ((pstrPrevious->pstrNext != NULL) && (pstrCurrent->pstrNext != NULL))
            {
               pstrPrevious->pstrNext = pstrCurrent->pstrNext;
               free(pstrCurrent);
            }
            else
            {
               pstrPrevious->pstrNext = NULL;
               free(pstrCurrent);
            }
         }


         nReturnCode = 0;
         break;
      }
   }


   if (nReturnCode == 0)
   {
      printf("Data [%s] deleted\n", pszData);
   }
   else
   {
      printf("Data [%s] not found\n", pszData);
   }



   return(nReturnCode);
}




/********************************************************************************
 * Function: HashFunction
 * Params:   nSize - Maximum buckets
 *           pszData - string to create determine bucket index
 * Returns:  Bucket index starting at 0 as the first index
 * Call by:  main()
 * Call to:  None
 * Overview: Adds up each character in a string and modular the sum to create a
 *           bucket index.
 * Notes:    None
 ********************************************************************************/
int HashFunction(int nSize, const char *pszData)
{
   int  nIndex  = 0;
   int  nLength = 0; 
   long lnSum   = 0;



   Debug("Inside HashFunction()\n");


   for (nLength=strlen(pszData); nIndex<nLength; nIndex++)
   {
      lnSum = lnSum + pszData[nIndex];
   }


   return((int)(lnSum%nSize));
}




/********************************************************************************
 * Function: ListHashTable
 * Params:   pastrHash
 *           nSize
 * Returns:  Number of non-empty items in hash table.
 * Call by:  main()
 * Call to:  None
 * Overview: Lists all entries in the hash table that is non-NULL data.
 * Notes:    None
 ********************************************************************************/
int ListHashTable(const strHashTable *pastrHash, int nSize)
{
   int           nIndex       = 0;
   int           nReturnCode  = 0;
   strHashTable *pstrCurrent  = NULL;



   Debug("Inside ListHashTable()\n");


   for (nIndex=0; nIndex<nSize; nIndex++)
   {
      for (pstrCurrent = (strHashTable *) &pastrHash[nIndex];
           pstrCurrent != NULL;
           pstrCurrent = pstrCurrent->pstrNext)
      {
         printf("Bucket[%d] data:  [%s]\n", nIndex, pstrCurrent->szData);
         ++nReturnCode;
      }
   }


   return(nReturnCode);
}




/********************************************************************************
 * Function: ProcessCommandLine
 * Params:   pstrRunOptions - stores options read from the command line
 *           argc - number of command line parameters
 *           argv - each command line parameter is a string
 * Returns:  Pointer to strCommandLine with options from command line.
 * Call by:  main()
 * Call to:  None
 * Overview: Reads arguments from command line and fills options structure.
 *           Valid options are
 *           --debug (optional)
 *           --hashsize
 * Notes:    None
 ********************************************************************************/
strCommandLine *ProcessCommandLine(strCommandLine *pstrRunOptions, int argc, char **argv)
{
   int nIndex = 0;



   pstrRunOptions->bDebug      = FALSE;
   pstrRunOptions->nBucketSize = 0;


   for (nIndex=1; nIndex<argc; nIndex++)
   {
      if (strcmp(argv[nIndex], "--hashsize") == 0)
      {
         pstrRunOptions->nBucketSize = atoi(argv[nIndex+1]);
      }
      else if (strcmp(argv[nIndex], "--debug") == 0)
      {
         pstrRunOptions->bDebug = TRUE;
      }
   }


   return (pstrRunOptions);
}




/********************************************************************************
 * Function: SearchHashTable
 * Params:   pastrHash
 *           nSize
 * Returns:  -1 - not found
 *           >0 - bucket index where found
 * Call by:  main()
 * Call to:  None
 * Overview: Search a value in the hash table after finding the proper bucket.
 * Notes:    None
 ********************************************************************************/
int SearchHashTable(const strHashTable *pastrHash, int nSize, const char *pszData)
{
   int           nChain      = 0;
   int           nHashIndex  = 0;
   int           nReturnCode = -1;
   strHashTable *pstrCurrent = NULL;



   Debug("Inside SearchHashTable()\n");

   nHashIndex = HashFunction(nSize, pszData);


   /****************************************************************************
    * Loop through through the linked list in the proper bucket to search.
    ****************************************************************************/
   for (pstrCurrent  = (strHashTable *) &pastrHash[nHashIndex];
        pstrCurrent != NULL;
        pstrCurrent  = pstrCurrent->pstrNext, nChain++)
   {
      if (strcmp(pstrCurrent->szData, pszData) == 0)
      {
          printf("Data [%s] found in bucket [%d] in chain [%d]\n", pszData, nHashIndex, nChain);
          nReturnCode = nHashIndex;
          break;
      }
   }


   return(nReturnCode);
}




/********************************************************************************
 * Function: Debug()
 * Params:   pszFormat - Formatting of variable parameters.
 *           ...
 * Returns:  Returns the number of characters printed by vprintf()
 * Call by:  most fuctions in this program
 * Call to:  DebugOn()
 * Overview: Debugging aid if needed, which prints messages to standard output.
 * Notes:    va_start, va_end uses stdarg.h
 *           DebugOn(0) the zero does not matter, it can be any value, but 0 for
 *           simplicity.
 *           Debug is enabled by calling the program with --debug
 ********************************************************************************/
int Debug(const char *pszFormat, ...)
{
    int nReturnCode = 0;
    va_list pstrList;



    if (DebugOn(0) == TRUE)
    {
       printf("DEBUG ");

       va_start(pstrList, pszFormat);
       nReturnCode = vprintf(pszFormat, pstrList);
       va_end(pstrList);
    }


    return(nReturnCode);
}




/********************************************************************************
 * Function: DebugOn()
 * Params:   bInit - value is either TRUE or FALSE on the first call.
 *           On subsequent calls, bInit doesn't matter, but should be something
             simple like 0.
 * Returns:  TRUE
 *           FALSE
 * Call by:  main()
 *           Debug()
 * Call to:  None
 * Overview: The first time DebugOn() is called, bDebug will always be -100.  The
 *           value of bInit will determine whether bDebug will be TRUE or FALSE.
 *           On subsequent calls to DebugOn(), bDebug will either have a value of
 *           TRUE or FALSE, that is, it's only -100 on the first call only.
 *
 *           if bInit is FALSE on the first call, debugging is turned off.
 *           if bInit is TRUE  on the first call, debugging is turned on.
 *           Subsequent calls to DebugOn() checks whether debug messages should be
 *            printed.
 *           The argument on subsequent calls does not matter, but use 0 for
 *           simplicity.
 * Notes:    This keeps us from having to utilize a global variable.
 ********************************************************************************/
int DebugOn(int bInit)
{
   static int bDebug = -100;



   if (bDebug == -100)
   {
      if (bInit == TRUE)
      {
         bDebug = TRUE;
      }
      else
      {
         bDebug = FALSE;
      }
   }


   return(bDebug);
}
