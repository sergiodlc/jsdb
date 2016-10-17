#include "rslib.h"
#pragma hdrstop

struct _HTMLEscapeCode
{int c; int l; const char* t;}

const static HTMLEscapeCodes[] =
{{'<',4,"&lt;"},
{'>',4,"&gt;"},
{'&',5,"&amp;"},
{'\"',6,"&quot;"},
{0,0,(char*)0}};

void HTMLEscape(Stream& out, const uint16* s)
{
 const char* stop= "<>&\"";
 size_t i;

 while (s && s[0])
 {
  i = 0;
  while (s[i] && !strchr(stop,s[i]))
   i++;

  if (i)
   {
    char c[32];
    for (size_t j=0; j<i; j++)
    {
     if (s[j] < 0x80)
     {
      c[0] = (char)s[j];
      out.write(c,1);
     }
     else
     {
      sprintf(c,"&#%d;",s[j]);
      out.writestr(c);
     }
    }
    s += i;
   }

  if (!*s) break;

  for (size_t x=0;HTMLEscapeCodes[x].c; x++)
    if (s[0] == HTMLEscapeCodes[x].c)
     {
      out.write(HTMLEscapeCodes[x].t,HTMLEscapeCodes[x].l);
      s++;
      break;
     }
 }
}

void HTMLEscape(Stream& out, const char* s)
{
 const char* stop= "<>&\"";
 size_t i;
 wchar_t type;

 while (s && s[0])
 {
  i = strcspn(s,stop);

  out.write(s,i);
  s += i;

  if (!s[0]) break;

   for (size_t x=0;HTMLEscapeCodes[x].c; x++)
    if (s[0] == HTMLEscapeCodes[x].c)
     {
      out.write(HTMLEscapeCodes[x].t,HTMLEscapeCodes[x].l);
      s++;
      break;
     }
 }
}

void HTMLUnEscape(char* s)
{
 const char * d = s;
 while (*d)
 {
 loop:
  if (*d == '&')
   {
    for (size_t x=0; HTMLEscapeCodes[x].c; x++)
    {
     if (!strncasecmp(d,HTMLEscapeCodes[x].t,HTMLEscapeCodes[x].l))
     {
      *s++ = HTMLEscapeCodes[x].c;
      d += HTMLEscapeCodes[x].l;
      goto loop;
     }
    }

    if (d[1] == '#')
    {
     char * e = 0;
     int cv = strtoul(d+2,&e,10);
     if (*e == ';' && cv)
     {
     char u[4];
      size_t l = UCS2ToUTF8C(cv,u);
      memcpy(s,u,l);
      s += l;
      d = e+1;
      continue;
     }
    }
   }

  *s++ = *d++;
 }
 *s = 0;
}

void CEscape(Stream& out, const char* c,const char* extra)
{
 for(; *c; c++)
  {
   switch (*c)
   {
    case '\n': out.write("\\n",2); break;
    case '\t': out.write("\\t",2); break;
    case '\r': out.write("\\r",2); break;
    case '\\': out.write("\\\\",2); break;
    default:
     if (extra) if (strchr(extra,*c)) out.write("\\",1);
    out.write(c,1);
  }
 }
}

void CUnEscape(Stream& out, const char* c)
{
  while (*c)
  {
   if (*c == '\\')
   {
    c++;
    switch (*c)
    {
     case 'n': out.write("\n",1); c++; continue;
     case 't': out.write("\t",1); c++; continue;
     case 'r': out.write("\r",1); c++; continue;
     default:
      out.write(c,1);
      c++;
      continue;
    }
   }
   out.write(c,1);
   c++;
  }
}

#ifdef RSLIB_FORMAT
bool PrintReplacement(TNameValueList& Vars,Stream & out,const char* in)
{
 const char* x = 0;
 if (in[0] == '\\')
 {
  switch (in[1])
  {
   case '$':
    x = Vars(in+2);
    if (x)
     {
      for(;*x; x++)
      {
       if (*x == '$' || *x == '@' || *x == '\"') out.write("\\",1);
       out.write(x,1);
      }
     }
     break;
   case '&':
    x = Vars(in+2);
    if (x) HTMLEscape(out,x);
    break;
   case '%':
    x = Vars(in+2);
    if (x)
    {
     TStr s;
     URLEncode1(s,x);
     out.writestr(s);
    }
   case '\\':
    x = Vars(in+2);
    if (x)
     CEscape(out,x,"\"\'");
  }
 }
 else if (Vars.Has(in))
 {
  x = Vars(in);
  if (x) out.writestr(x);
 }
 else return false;

 if (x) return true;
 return false;
}

void FormatText(TNameValueList& Vars, Stream& in, Stream& out,
                 int start, int end)
{
 MemoryStream mem;
 TStr temp;
 char x;

 while (in.ReadUntilChar(start,&out) != EOF)
  {
    in.read(&x,1);

    if (strchr(" \t\r\n",x) || x == end)
     {
      out.put(start);
      out.put(x);
      continue;
     }
    mem.Clear();
    mem.put(x);
    in.ReadUntilChar(end,&mem);
    PrintReplacement(Vars,out,mem);
  }
}

void FormatText(TNameValueList& Vars, Stream& in, Stream& out,
                 const char * start, const char * end)
{
 if (!start || !end) return;
 if (!*start || !*end) return;

 MemoryStream mem;
 size_t r = strlen(end);
 TStr x(r);

 while (in.ReadUntilWord(start,&out))
  {
    x[in.read(x,r)]=0;

    if (strchr(" \t\r\n",*x) || x == end)
    {
     out.writestr(start);
     out.writestr(x);
     continue;
    }

    mem.Clear();
    mem.writestr(x);
    in.ReadUntilWord(end,&mem);
    PrintReplacement(Vars,out,mem);
  }
}

#endif
