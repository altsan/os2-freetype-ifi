#ifndef FT2DEBUG_H
#define FT2DEBUG_H

/****************************************************************************/
/* some debug macros and functions. the debug version logs system requests  */
/* to the file \FTIFI.LOG                                                   */
/*                                                                          */
#ifdef DEBUG
  HFILE LogHandle = NULLHANDLE;
  ULONG Written   = 0;
  char  log[2048] = "";
  char  buf[2048] = "";


char*  itoa10( int i, char* buffer ) {
    char*  ptr  = buffer;
    char*  rptr = buffer;
    char   digit;

    if (i == 0) {
      buffer[0] = '0';
      buffer[1] =  0;
      return buffer;
    }

    if (i < 0) {
      *ptr = '-';
       ptr++; rptr++;
       i   = -i;
    }

    while (i != 0) {
      *ptr = (char) (i % 10 + '0');
       ptr++;
       i  /= 10;
    }

    *ptr = 0;  ptr--;

    while (ptr > rptr) {
      digit = *ptr;
      *ptr  = *rptr;
      *rptr = digit;
       ptr--;
       rptr++;
    }

    return buffer;
}

  #define  COPY(s)     strcpy(log, s)
  #define  CAT(s)      strcat(log, s)
  #define  CATI(v)     strcat(log, itoa10( (int)v, buf ))
  #define  CATX(v)     strcat(log, _ultoa( (ULONG)v, buf, 16 ))
  #define  WRITE       DosWrite(LogHandle, log, strlen(log), &Written)

  #define  ERET1(label) { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          goto label;        \
                       }

  #define  ERRRET(e)   { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          return(e);         \
                       }


#else

  #define  COPY(s)
  #define  CAT(s)
  #define  CATI(v)
  #define  CATX(v)
  #define  WRITE

  #define  ERET1(label)  goto label;

  #define  ERRRET(e)  return(e);

#endif /* DEBUG */

#endif /* FT2DEBUG_H */
