#include <stdio.h>
#include <string.h>
#include "hashtable.h"
#include "hashtable_itr.h"

typedef struct {
  int occurence;
  unsigned long long pos;
} hash_value;

static unsigned int hash_from_key_fn( void* k ) {
  unsigned char* key=(unsigned char*)k;
  unsigned int ret=0;
  int i, len;
/*  for(i=0; i<10 && i<strlen(key); i++) { */
  /* reverse */
  len=strlen((const char *)key);
  for(i=len-1; i>=0 && len-i<10; i--) {
    ret+=key[i];
  }
  /* i++;
  ret=ret/i; */
  ret=ret/(len-i);
  return ret;
}

static int keys_equal_fn ( void* k1, void* k2 ) {
  unsigned char* key1=(unsigned char*)k1;
  unsigned char* key2=(unsigned char*)k2;
  if(strcmp((const char *)key1, (const char *)key2)==0)
    return 1;
  else
    return 0;
}
  
