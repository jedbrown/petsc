/* $Id: ctable.h,v 1.4 1999/02/17 21:38:10 balay Exp balay $ */
/* Contributed by - Mark Adams */

#if !defined(__CTABLE_H)
#define __CTABLE_H

typedef int* CTablePos;  
typedef struct TABLE_TAG 
{
  int *keytable;
  int *table;
  int count     ;
  int tablesize ;
  int head;
}*Table,Table_struct;

extern int TableCreate(const int size,Table *ta);
extern int TableCreateCopy(const Table intable,Table *rta);
extern int TableDelete(Table ta);
extern int TableGetCount(const Table ta,int *count);
extern int TableIsEmpty(const Table ta, int* flag);
extern int TableAdd(Table ta, const int key, const int data);
extern int TableFind(Table ta, const int key,int *data);
extern int TableGetHeadPosition(Table ta, CTablePos *);
extern int TableGetNext(Table ta, CTablePos *rPosition, int *pkey, int *data);
extern int TableRemoveAll(Table ta);

#endif
