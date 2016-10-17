#include "rslib.h"
#pragma hdrstop

/* we don't trust the standard library implementations */

#ifdef __MWERKS__ // lousy compiler
int strnicmp(const char*a,const char*b, size_t l)
{
 for ( size_t i=0; i<l; i++)
  {
   int x = toupper(a[i])-toupper(b[i]);
   if (x) return x;
  }
 return 0;
}

int stricmp(const char*a,const char*b)
{
 size_t i;
 for (i=0 ; a[i] && b[i]; i++)
  {
   int x = toupper(a[i])-toupper(b[i]);
   if (x) return x;
  }
 return a[i] - b[i];
}
#endif

bool strsplit(const char * in, const char * split,TStr& before,TStr& after)
{
 const char * c = stristr(in,split);
 if (!c) return false;
 before.Resize(c-in,in);
 after.Resize(strlen(c) - strlen(split),c+strlen(split));
 return true;
}


void TStr::replace(const char* remove, const char * replace)
{

 TChars stream;
 size_t l1 = strlen(remove);
 size_t l2 = strlen(replace);

 char * d = str;
 while (*d)
 {
  char * c = stristr(d,(char*)remove);
  if (!c) break;
  stream.Append(d,c-d);
  if (l2) stream.Append(replace,l2);
  d = c + l1;
 }
 stream << d;
 Set(stream.buf,stream.size);
}

inline char * myAppend(char*d,const char *s)
  {size_t i = strlen(s); memmove(d,s,i); return d+i;}

char * ConcatStrings(const char ** c)
    {
     size_t len = 0;
     char * str;
     const char **p = c;
     while (*p) len += strlen(*(p++));

     str = AllocStr(len);

     char * s = str;
     while (*c) s = myAppend(s,*(c++));
     str[len] = 0;
     return str;
    }; //appends a list of strings

TStr::TStr(char ** c)
    {
     str=ConcatStrings((const char ** )c);
    }


TStr::TStr(const char * s1,const char * s2,const char * s3,const char * s4,
               const char * s5,const char * s6,const char * s7,const char * s8,
               const char * s9,const char * sA,const char * sB,const char * sC)
    {
    const char * array[14];
    array[0] = s1;
    array[1] = s2;
    array[2] = s3;
    array[3] = s4;
    array[4] = s5;
    array[5] = s6;
    array[6] = s7;
    array[7] = s8;
    array[8] = s9;
    array[9] = sA;
    array[10]= sB;
    array[11]= sC;
    array[12]=0;
    str=ConcatStrings(array);
    } //appends a list of strings

    TStr::TStr(const char ** c)
    {str=ConcatStrings(c);
    } //appends a list of strings
/*
    TStr::TStr( HINSTANCE instance,...)
    {
     int count = 0;
     va_list ap;

     int arg;
     va_start(ap, instance);
     while ((arg = va_arg(ap,int)) != 0) { count++;}
     va_end(ap);

#ifdef UNICODE
#define RESOURCE_LENGTH 512
#else
#define RESOURCE_LENGTH 256
#endif

     str = AllocStr(count * RESOURCE_LENGTH);
     char * c = str;

     va_start(ap, instance);
     while ((arg = va_arg(ap,int)) != 0)
      {
       int i = LoadString(instance,arg,c,RESOURCE_LENGTH);
       if (i) c += i;
       else c += wsprintf(c,"?%d?",i);
      }
     va_end(ap);
    }
 */
/* make inline by moving implementations to the header file
TStr::TStr() {str=AllocStr((size_t)0u);}

TStr::TStr(const char * c)
         {str=AllocStr(c);}

TStr::TStr(const char * c, size_t len)
             {str=AllocStr(len);
          memcpy(str,c,len);
         }

TStr::TStr(size_t len,char v )
             {str=AllocStr(len);
          memset(str,v,len);}

TStr::TStr(size_t len)
             {str=AllocStr(len);
          str[0]=0;}

TStr::TStr(const TStr& ts)
        {str = AllocStr(ts.str);}
TStr & TStr::operator = (const char * c)
{
 Set(c,c?strlen(c):0);
 return *this;
}

*/
TStr::~TStr()
 {
  if(str) FreeStr(str) ; //delete [] str;
  str = (char*)"";
 }

void TStr::Truncate(size_t len)
{
 if (strlen(str) > len) str[len]=0;
}

void TStr::Resize(size_t len,const char * c)
{
 char * ns = AllocStr(len);
 strncpy(ns,c?c:str,len);
 delete [] str;
 str = ns;
}

void TStr::Set(const char * c, size_t length)
{
  if (!c)
  {
   str[0]=0;
   return;
  }

  if (!c[0])
  {
   str[0]=0;
   return;
  }

 size_t ol = strlen(str);
 if (str && length <= ol)
 {
     memcpy(str,c,length);
 }
 else
 {
     char* ns = AllocStr(length);
     memcpy(ns,c,length);
     delete [] str;
     str = ns;
 }
 str[length] = 0;
}

TStr & TStr::operator += (const char * c)
{
 if (!c || ! *c) return *this;
 char * ns;
 size_t b = strlen(str);
 size_t a = strlen(c);
 ns = AllocStr(a+b);
 memmove(ns,str,b);
 memmove(ns+b,c,a);
 ns[a+b]=0;
 delete [] str;
 str=ns;
 return *this;
}

void TStr::Exchange(TStr&ts)
{
 char * s = str;
 str = ts.str;
 ts.str = s;
}
/*
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
*/


/**
If you write a sentence in MS Word and paste it into IE, IE will not do the
form-appropriate character type conversion. Thus, ANSI characters appear in our
http data. You can test for this by looking for isolated high-bit characters.
Properly encoded UTF-8 text always has two or more adjacent characters with the
high bit set. If this post looks like it contains ANSI characters, we'll convert
them to utf-8. All the character codes from 127 to 160 are excluded from unicode
and the leading character in a UTF08 sequence is always >= 192.

We detect ANSI characters by looking for an isolated high-bit character or
a character with an invalid value. Unfortunately, this is a field-by-
field effect, and so we need to test every field value.
*/

bool IsANSI(const char* s)
{
/*
 ansi
 128 10000000
 159 10011111
 160 10100000
 utf-8
 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 1110xxxx 10xxxxxx 10xxxxxx
 110xxxxx 10xxxxxx
 non-ansi characters: 127, 129, 141, 143, 144, 157,
 01111111 10000001 10001101 10001111 10010000 10011101
 are also all non-unicode values
*/
 unsigned c = *s++ & 0x00ff;
 unsigned d;

 while (c)
 {
  if (c & 0x80)
  {
   if ((c & 0x60) == 0) return true; /*ANSI remap range -- no unicode characters*/
   d = *s; /*look at the next character */
   if ((d & 0xc0) != 0x80) return true;
   /* require c & 0x60 == 0x40 || c & 0x70 == 0x60 || c & 0xf8 == 0xf0 */
   /*eos | not a utf-8 start sequnce | not a utf-8 sequence */

   /* a valid UTF-8 sequence is unlikely inside an ANSI sequence, but not impossible. */
   if ((c & 0x60) == 0x40) /* 110xxxxx 10xxxxxx */
   {
    s++;
    goto next;
   }

   d = s[1];
   if ((d & 0xc0) != 0x80) return true;

   if ((c & 0xf0) == 0xe0) /* 1110xxxx 10xxxxxx 10xxxxxx */
   {
     s += 2;
     goto next;
   }

   d = s[2];
   if ((d & 0xc0) != 0x80) return true;

   if ((c & 0xf8) == 0xf0) /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
   {
    s += 3;
    goto next;
   }

   return true; /* isolated high-bit character */
  }
 next:
  c = *s++ & 0x00ff;
 }

 return false;
}

/* ANSI -> Unicode values from 128 to 159*/
static const unsigned ANSIchars []=
{
8364,   /* &euro;   euro sign   Currency Symbols*/
  '?',
8218,   /* &sbquo;  single low-9 quotation mark General Punctuation*/
402,    /* &fnof;   Latin small letter f with hook  Latin Extended-B*/
8222,   /* &bdquo;  double low-9 quotation mark General Punctuation*/
8230,   /* &hellip; horizontal ellipsis General Punctuation*/
8224,   /* &dagger; dagger  General Punctuation*/
8225,   /* &Dagger; double dagger   General Punctuation*/
710,    /* &circ;   modifier letter circumflex accent   Spacing Modifier Letters*/
8240,   /* &permil; per mille sign  General Punctuation*/
352,    /* &Scaron; Latin capital letter S with caron   Latin Extended-A*/
8249,   /* &lsaquo; single left-pointing angle quotation mark   General Punctuation*/
338,    /* &OElig;  Latin capital ligature OE   Latin Extended-A*/
  '?',
381,            /* Latin capital letter Z with caron    Latin Extended-A*/
  '?',
 '?',
8216,   /* &lsquo;  left single quotation mark  General Punctuation*/
8217,   /* &rsquo;  right single quotation mark General Punctuation*/
8220,   /* &ldquo;  left double quotation mark  General Punctuation*/
8221,   /* &rdquo;  right double quotation mark General Punctuation*/
8226,   /* &bull;   bullet  General Punctuation*/
8211,   /* &ndash;  en dash General Punctuation*/
8212,   /* &mdash;  em dash General Punctuation*/
732,    /* &tilde;  small tilde Spacing Modifier Letters*/
8482,   /* &trade;  trade mark sign Letterlike Symbols*/
353,    /* &scaron; Latin small letter s with caron Latin Extended-A*/
8250,   /* &rsaquo; single right-pointing angle quotation mark  General Punctuation*/
339,    /* &oelig;  Latin small ligature oe Latin Extended-A*/
  '?',
382,            /* Latin small letter z with caron  Latin Extended-A*/
376,    /* &Yuml;   Latin capital letter Y with diaeresis   Latin Extended-A */
};


/** if dest is null, returns the needed length */
size_t ANSItoUTF8(const char* s, char* dest)
{
  size_t l=0;
  char d[4];

  for (; *s; s++)
  {
   unsigned x = (*s) & 0x00ff;
   if ((x & 0x80) == 0)
   {
    if (dest) dest[l] = x;
    l++;
    continue;
   }

   if ((x & 0x60) == 0) //between 128 and 159, remap the values!
    x = ANSIchars[x - 128u];

   l += UCS2ToUTF8C(x, dest ? dest : d);
  }
  if (dest) dest[l]=0;
  return l;
}

//UTF-16 from UTF-8.

size_t UTF8ToUCS2C(const char* in,uint16 &c)
{
 if ((in[0] & 0x80) == 0) {c=in[0]; return 1;} //1 character OK

 // 110xxxxx 10xxxxxx
 if (((in[0] & 0xe0) == 0xc0) && ((in[1] & 0xc0) == 0x80))
  { c = uint16( ((in[0] & 0x1f) << 6) | (in[1] & 0x3f)); return 2;}

 // 1110xxxx 10xxxxxx 10xxxxxx
 if (((in[0] & 0xf0) == 0xe0) && ((in[1] & 0xc0) == 0x80) && ((in[2] & 0xc0 )== 0x80))
  { c = uint16(((in[0] & 0x0f) << 12) | ((in[1] & 0x3f) << 6)| (in[2] & 0x3f)); return 3;}

 // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 if (((in[0] & 0xf8) == 0xf0) && ((in[1] & 0xc0) == 0x80) && ((in[2] & 0xc0 )== 0x80)&& ((in[3] & 0xc0) == 0x80))
  { c = uint16(((in[0] & 0x0f) << 18) | ((in[1] & 0x3f) << 12)| ((in[2] & 0x3f) <<6)| ((in[3] & 0x3f))); return 4;}

 return 0; //error
}

///returns the number of characters (not bytes) needed  (not including the final null)
size_t UTF8ToUCS2Length(const char* in,size_t max)
{
 uint16 x;
 size_t length=0;
 if (in) while (*in && max)
  {
   size_t a = UTF8ToUCS2C(in,x);
   if (!a) a = 1; //if it's an invalid character, leave it alone and go to the next one
   length++;
   max-= a;
   in += a;
  }
 return length;
}

///returns the number of characters (not bytes) converted (not including the final null)
size_t UTF8ToUCS2(const char* in,uint16 * out,size_t max)
{
 size_t length=0;
 size_t cur=0;
 if (in) while (in[cur] && cur < max)
  {
   size_t a = UTF8ToUCS2C(in+cur,*out);
   if (!a) //probably an ANSI character sneaked in
   {
       a = 1;
       if ((unsigned)in[cur] > 127 && (unsigned)in[cur] < 160)
        *out = ANSIchars[in[cur] - 128];
       else
        *out = in[cur];
   }
   length++;
   out++;
   cur += a;
  }
 *out = 0;
 return length;
}

size_t UCS2ToUTF8C(unsigned in, char* out)
{
  if (in < 0x80)
  {
      out[0]=(char)in;
      return 1;
  }
  else if (in < 0x800) //can be described in 11 bits
  {
      out[1] = (char)((in & 0x3f) | 0x80);
      out[0] = (char)((in >> 6)   | 0xc0);
      return 2;
  }
  else if (in < 0x10000) //16 bits?
  {
      out[2] = (char)((in & 0x3f) | 0x80);
      out[1] = (char)(((in >> 6) & 0x3f) | 0x80);
      out[0] = (char)((in >> 12)  | 0xe0);
      return 3;
  }
  else //21 bits
  {
     out[3] = (char)((in & 0x3f) | 0x80);
      out[2] = (char)(((in >> 6) & 0x3f) | 0x80);
      out[1] = (char)(((in >> 12) & 0x3f) | 0x80);
      out[0] = (char)(((in >> 18) & 0x07) | 0xf0);
      return 4;
  }
}

///returns the number of bytes (not not characters) needed (not including the final null)
size_t UCS2ToUTF8Length(const uint16* in,size_t max)
{
 size_t length =0;
 char d[4];
 if (in) while (*in && max)
 {
  length += UCS2ToUTF8C(*in,d);
  in++;
  max--;
 }
 return length;
}

///returns the length of the result string (not including the final null)
size_t UCS2ToUTF8(const uint16* in,char* out,size_t max)
{
 int ret=0;
 if (in) while (*in && max)
 {
  size_t a = UCS2ToUTF8C(*in,out);
  out += a;
  ret += a;
  in++;
  max--;
 }
 *out = 0;
 return ret;
}

WStr::WStr(const char* x,size_t max)
 //Allocate enough space.
   : data(UTF8ToUCS2Length(x,max) * sizeof(uint16))
   {
    UTF8ToUCS2(x,data.w(),max);
   }

TStr::TStr(const uint16* x,size_t len)
{
 str = AllocStr(UCS2ToUTF8Length(x,len));
 UCS2ToUTF8(x,str,len);
}

#if 0
WStr::WStr(const char* x)
 //Allocate enough space.
   : data(MultiByteToWideChar(CP_UTF8,0,x,strlen(x),NULL,0) * sizeof(uint16))
   {//copy
    MultiByteToWideChar(CP_UTF8,0,x,strlen(x),data.w(),data.size/sizeof(uint16));
   }

TStr::TStr(const uint16* x)
{
 size_t l = WideCharToMultiByte(CP_UTF8,0,x,wcslen(x),NULL,0,NULL,NULL);
 str = AllocStr(l);
 WideCharToMultiByte(CP_UTF8,0,x,wcslen(x),str,l,NULL,NULL);
}
#endif
//------------------------------------------------------------------------
