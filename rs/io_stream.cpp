#include "rslib.h"
#pragma hdrstop

Stream::Stream(TType _Type) : error(0)
{
Type=_Type;
}

Stream::~Stream()
{
 if (error) {delete error; error = 0;}
}

#define BLOCKSIZE 16384

#ifdef __WIN16__
//for when you don't have a flat memory model.

#define BUFSIZE 65536
int32 Stream::lread(char * dest,int32 maxcopy)
   {
  int32 iters = maxcopy / (int32)BUFSIZE, __i;

  for (__i=0;__i<iters;__i++)
  {
   if (!read((char*)dest,BUFSIZE)) return (__i * BUFSIZE);
   dest += BUFSIZE;
  }
  int remsize= (int) (maxcopy % BUFSIZE);
  if (! read((char*)dest,remsize)) return (iters * BUFSIZE);
  return maxcopy;
   };

int32 Stream::lwrite(const char * src,int32 maxcopy)
   {
  int32 iters = maxcopy / (int)BUFSIZE, __i;
  int32 ret=0;
  int bw;
  for (__i=0;__i<iters;__i++)
  {
   if ((bw= write((char*)src,BUFSIZE))==0) return ret;
   src += BMSBLOCK;
   ret += bw;
  }
  int remsize= (int) (maxcopy % BUFSIZE);
  if ((bw = write((char*)src,remsize))==0) return ret;
  ret += bw;
  return ret;
   };
#endif

int Stream::peek()
 {
  int i = get();
  if (i != EOF) putback(1);
  return i;
 }

long Stream::Append(Stream& copy,long maxcopy)
{
 long size=0;
 int block;
 char * tempbuf = new char[BLOCKSIZE];
 while (maxcopy && (block = copy.read(tempbuf,(size_t)min((long)BLOCKSIZE,maxcopy))) != 0)
 {
  write (tempbuf,block);
  size += block;
  maxcopy -= block;
 }
 delete [] tempbuf;
 return size;
}

long Stream::AppendAsText(Stream& copy, long maxcopy)
{
 char c;
 long size=0;

 //char * tempbuf = new char[BLOCKSIZE];
 while (maxcopy && copy.read(&c,1))
 {
  maxcopy --;
  if (c == '\r') continue;
  if (c == '\n') size+=write("\r\n",2);
  else size+=write(&c,1);
 }
 return size;
}

bool Stream::ReadUntilWord(const char * check, Stream* out)
{
 size_t len = strlen(check);
 if (len == 0) return false;
 TStr buf(len);
 size_t pos=0;


 int k = read(buf,len);
 if (k != len)
  {
   if (k>0 && out) out->write(buf,k);
   return false;
  }

 len --;

// int i=0,j=0;
 while (buf != check)
  {
   if (out) out->write((char*)buf,1);
   memmove((char*)buf,(char*)buf+1,len);
   if (read((char*)buf + len,1) != 1)
   {
    if (out) out->write(buf,len);
    return false;
   }

/*   if (! ((i+100) % 4096))
    {
     j++;
    }
   i++;
*/
 }
 return true;
}


bool Stream::ReadUntilBytes(const char * check, size_t length, Stream* out)
{
 size_t len = length;
 if (len == 0) return false;
 size_t pos=0;
 TStr buf(len);

 int k = read(buf,len);
 if (k != len)
  {
   if (k>0 && out) out->write(buf,k);
   return false;
  }

 len --;

// int i=0,j=0;
 while (memcmp(buf,check,length))
  {
   if (out) out->write((char*)buf,1);
   memmove((char*)buf,(char*)buf+1,len);
   if (read((char*)buf + len,1) != 1)
   {
    if (out) out->write(buf,len);
    return false;
   }
  }
 return true;
}

int Stream::ReadUntilChar(int check,Stream* out)
{
 char c;
 while (read(&c,1))
 {
  if (check == c) return (int)(unsigned char)c;
  if (out) out->write(&c,1);
 }
 return -1;
}

int Stream::ReadUntilChar(const char * check,Stream* out)
{
 char c;
 while (read(&c,1))
 {
  if (strchr(check,c)) return (int)(unsigned char)c;
  if (out) out->write(&c,1);
 }
 return -1;
}

int Stream::EatChars(const char * skip)
{
 char c;
 while (read(&c,1))
 {if (!strchr(skip,c)) return (int)(unsigned char)c;
 }
 return -1;
}


size_t Stream::writeints(int*ints,size_t count)
{
 long a;
 size_t ret = 0;
 for (int i = 0 ; i < count ; i++)
 {
  a=(long)ints[i];
  ret += write(&a,4);
 }
 return count;
}


size_t Stream::readints(int*ints,size_t count)
{
 long a;
 size_t ret = 0;
 for (int i = 0 ; i < count ; i++)
 {
  if (read(&a,4) != 4) break;
  ints[i] = (int)a;
  ret++;
 }
 return ret;
}

// readline, implies text mode, so the function must
// strip out linefeed characters such as \r

size_t Stream::readline(char * dest, size_t maxcopy,int delim)
{size_t i = 0;
 if (!dest) return 0;
 while ((i < maxcopy) && read(dest,1))
  {
   i++;
   if (*dest == delim) break;
   else dest++;
  }
 *dest=0;
 return i;
}

size_t Stream::readline(TStr & ts,int delim)
{
 TStr s(8192);
 size_t ret = readline((char*)s,8192,delim);
 ts.Exchange(s);
 return ret;
}

count_t Stream::readline(Stream& dest,int delim)
{
 char c;
 count_t i = 0;
 while (read(&c,1))
   {
    if (c == delim) c = 0;
    i += dest.write(&c,1);
    if (c == 0) break;
   }
 return i;
}

