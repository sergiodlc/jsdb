#pragma hdrfile "jsdb.csm"
#include "rslib.h"
#include "jsdb.h"
#include "rs/wrap_jsdb.h"
#pragma hdrstop
#include "js/jsautocfg.h"


/* 1.7.2.7 Removed Stream_Format and global printReport
<fuction name=format>
This function was removed in version 1.7.2.7. You can replace it with
<pre>
Stream.prototype.format = function(source, values)
{
  var text = source.replace(/{(.*?)}/g, function (x, name) {return values.get(name)} )
  this.write(text)
}
</pre>
</function>

<function name=printReport type=String>
<parameter name="format" type="Stream" />
<parameter name="values" type="Object|Record" />
<i>Deprecated in 1.7.</i> <i>Removed in 1.7.2.7.</i> Reads <b>format</b> and replaces {fieldname} with appropriate record values.
</function>
<function name=format >
<parameter name="source" type="Stream|String" />
<parameter name="values" type="Record|Object" />
<parameter name="start_delimiter" type="String" optional="1"/>
<parameter name="end_delimiter" type="String" optional="1"/>
<i>Note: this function seems to be broken</i><br>
Reads <b>format</b> and replaces <tt>{fieldname}</tt> with the properties of <b>values</b>. The standard delimiters,
are curly braces, but you can override these with any text string. If end_delimiter is null and
start_delimiter is non-null, end_delimiter = start_delimiter.
</function>
*/

void Stream_JSFinalize(JSContext *cx, JSObject *obj)
{
 DELPRIVATE(Stream);
}

WRAP(Stream,Close)
{
 CLOSEPRIVATE(Stream,Stream);
 RETBOOL(true);
}

static void GetProxy(TStr& x,const char* address)
{
#ifdef XP_WIN
 TStr s;
 RegGetKey("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
           "ProxyEnable",s,HKEY_CURRENT_USER);
 if (!*s) return;
 if (!atoi(s)) return;

 TStr override;
 RegGetKey("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
           "ProxyOverride",override,HKEY_CURRENT_USER);

 if (stristr(override, "<local>") && !strchr(address,'.'))
   return;

 if (!strncasecmp(address,"http://",7)) address += 7;

 TStringList list(override,";");

 FOREACH(const char*c, list)
  if (!*c) continue;
  if (stristr(address,c) == 0) return;
 DONEFOREACH

 RegGetKey("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
           "ProxyServer",s,HKEY_CURRENT_USER);

 x = TStr("http://",s,"/");
#endif
}
/*
class InetCache: public MemoryStream
{
  public:
   TStr name;
   const char * filename() {return name;}
   InetCache() :MemoryStream() {}
   ~InetCache() {}
};
*/

JSBool
Stream_Stream(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
    jsval *rval)
{
 GETENV;
 if (!Env) return JS_FALSE;
 //bool loadStatus = true;
 Stream * dt = NULL;
 jsval val;
 GETUTF8(0);
 char* s0 = u0;

 if (*u0)
  {
   xdb err;
   if (Env->SafeMode)
    {//only web and memory streams are allowed
     if (strncasecmp(s0,"http://",7) && strncasecmp(s0,"inet://",7) && strncasecmp(s0,"net://",6))
     {
       if (Env->SafeMode) ERR_MSG(Stream,Stream,"blocked by security settings");
     }
    }
   else
   // stdin and stdout are already mapped.
   if (*s0 == 0 || !strcasecmp(s0,"stdin") ||
       !strcasecmp(s0,"stdout") || !strcasecmp(s0,"stderr"))
    ERR_MSG(Stream,"Invalid filename",s0);

   if (!strncasecmp(s0,"http://",7))
    {//opens HTTP and reads the reply
     TPointer<TParameterList> sendHeaderAutoDelete;
     TNameValueList* sendHeader = 0;
     if (argc >= 3 && JSVAL_IS_OBJECT(argv[2]))
      {
       GETREC(2,sendHeader)
      }
     else if (argc >= 2 && JSVAL_IS_OBJECT(argv[1]))
      {
       GETREC(1,sendHeader)
      }
     if (argc >= 2 && ISINT(1) && INT(1) == 0 )
     {
      dt = new InternetStream(s0,sendHeader);
     }
     else
     {
     try {
      TStr proxy;
      GetProxy(proxy,s0);
      TParameterList Headers;

      InternetStream * is = new InternetStream(TStr(proxy,s0),sendHeader);
      ConcatStream *cs = new ConcatStream;

      MemoryStream *headers = 0;
      if (argc >= 2 && ISINT(1) && INT(1) == 0)
      {
           headers = new MemoryStream;
           cs->insert(headers);
      }

      dt = cs; //stream to return

      TStr statusText;
      int status = 0;
      is->readline(statusText);
      char * c = strchr(statusText,' ');
      if (c)
      {
        status = atoi(c+1);
        is->ReadMIME(Headers);
      }

      val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,statusText));
      JS_SetProperty(cx, obj,"statusText",&val);

      val = INT_TO_JSVAL(status);
      JS_SetProperty(cx, obj,"status",&val);

      const char* chunked = Headers("Transfer-Encoding");
      if (!strcasecmp(chunked,"chunked")) //chunked always comes first
      {
       //InetCache*ic = new InetCache();
       cs->name = is->filename();
       if (strchr(chunked,';')) //other encodings
       {
          Headers.Set("Transfer-Encoding",strchr(chunked,';') + 1);
       }
       else
       {
          Headers.Unset("Transfer-Encoding");
       }

       MemoryStream *cache = new MemoryStream;
       cs->insert(cache);

       TStr line;
       size_t length = 0;
       while (is->readline(line))
       {
        Replace(line,'\r',0);
        if (!*line) continue;
        unsigned count = strtoul(line,0,16);
        if (!count) break;
        length += cache->Append(*is,count);
       }

       Headers.Set("Content-length",length);

       delete is;
       cache->rewind();
      }
      else
      {
       cs->insert(is);
      }

      if (headers)
      {
          headers->WriteMIME(Headers);
          headers->rewind();
      }

      //header is a property
      JSObject * h = JS_NewObject(cx, NULL, NULL, NULL);
      ListToObject(cx,Headers,h);
      jsval val = OBJECT_TO_JSVAL(h);
      JS_SetProperty(cx, obj,"header",&val);

      if (dt->error) {delete dt; dt=0;}
     } catch(...) {dt=0;}
     //loadStatus = false;
     }
    }
//#ifndef XP_UNIX
   else if (Env->AllowExec && !strncasecmp(s0,"exec://",7))
   {
     try {
     bool detached = false;
     if (ISSTR(1)) if (toupper(*STR(1))=='D')
      detached = true;
      dt = new PipeStream(s0+7,detached);
      if (dt->error) {delete dt; dt=0;}
     }
     catch (...) {ERR_MSG(Stream,"Could not open the pipe",s0);}
   }
//#endif

   if (!dt && strncasecmp(s0,"file://",7))
    dt = OpenStream(s0,&err);
   //OpenStream returns NULL if the string doesn't have a ://

   if (dt) if (dt->error)
   {
     delete dt;
     if (Env->errorOnFailure)
       return JS_FALSE;

     ERR_MSG(Stream,"Could not open the stream",s0)
    // else
    //  return JS_FALSE; //

   }

   if (dt && (!strncasecmp(s0,"inet://",7) || !strncasecmp(s0,"net://",6)))
   {
     InternetStream* is = TYPESAFE_DOWNCAST(dt,InternetStream);
     if (is && argc > 1 && ( (ISINT(1) && INT(1) == 1) || (ISSTR(1) && toupper(*STR(1)) == 'I') ))
       is->NoDelay();
   }

   if (!dt && s0)
    if (*s0)
    {
     if (!strncasecmp(s0,"file://",7)) s0 = s0 + 7;
     Stream::TType Type = Stream::IORead;
     Stream::TOpenMode Mode = Stream::OMText;
     if (argc >= 2 && ISSTR(1))
      {
       const char * mode = STR(1);
       if (strchr(mode,'a')) Type = Stream::IOAppend;
       else if (strchr(mode,'c')) Type = Stream::IOCreate;
       else if (strchr(mode,'r'))
       {
        if (strchr(mode,'w')) Type = Stream::IOReadWrite;
        else Type = Stream::IORead;
       }
       else if (strchr(mode,'w')) Type = Stream::IOWrite;

       if (strchr(mode,'b')) Mode = Stream::OMBinary;
       if (strchr(mode,'+') || strchr(mode,'u')) Mode = (Stream::TOpenMode)((int)Mode | (int)Stream::OMUnbuffered);
      }

     try {
      dt = new FileStream(s0,Mode,Type);
     } catch(...)
     {
      dt = NULL;
     }
   }
 }

 /*else if (argc == 1 && ISINT(0))
 {
  try {
  dt = new MemoryStream(INT(1));
  }catch (...) {dt = NULL;}
 }    */
 else if (argc == 0)
 {
  try{
  dt = new MemoryStream;
  }  catch (...) {dt = NULL;}
 }

 if (!dt)
  {
   if (Env->errorOnFailure)
   return JS_FALSE; //

   ERR_MSG(Stream,"Could not open the stream",s0)
  }
 //if (!dt) return JS_FALSE;

 if (dt)
 SETPRIVATE(obj,Stream,dt,true,NULL);
 // JS_SetPrivate(cx,obj,dt);

 return JS_TRUE;
}

WRAP(Stream,Wait)
{
 GETOBJ(Stream,Stream,t);
 InternetStream* s = 0;
 int timeout=-1;
 if (argc > 1)
  TOINT(1,timeout);
 s = TYPESAFE_DOWNCAST(t,InternetStream);
 bool ret = false;
 if (s) ret = s->wait(timeout);
 RETBOOL(ret);
}

JSBool
Stream_JSGet(JSContext *cx, JSObject *obj, jsval id, jsval *rval)
{
 GETOBJ(Stream,Stream,t);

 int x = JSVAL_TO_INT(id);

 if (JSVAL_IS_INT(id))
 {
  InternetStream* s = 0;
//#ifdef XP_WIN
  PipeStream* p =0;
//#endif
  switch (x)
  {
   case 0: RETSTR(t->filename());
   case 1: RETINT(t->size());
   case 2:
   case 3: RETINT(t->pos());
   case 4: RETBOOL(t->eof());
   case 5:
   case 6: s = TYPESAFE_DOWNCAST(t,InternetStream);
           break;
   case 7: RETBOOL(t->canread());
   case 8: RETBOOL(t->canwrite());
   case 9: RETSTRW(L"Stream");
//#ifdef XP_WIN
   case 10: p=TYPESAFE_DOWNCAST(t,PipeStream);
//#endif
  }
  switch(x)
  {
   case 5: if (s) RETSTRWC(s->gethostinfo());
           RETSTRW(L"");
   case 6: if (s) RETINT(s->hostaddr);
           RETINT(0);
//#ifdef XP_WIN
   case 10: if (p && p->StdErr) RETOBJ(Stream_Object(cx,p->StdErr,false,GETPOINTER));
           RETOBJ(0);
//#endif
  }
 }

 return JS_FALSE;
}

static JSPropertySpec Stream_properties[] = {
    {"name",      0,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"size",  1,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"position",  2,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"pos",  3,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"eof",  4,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"hostName", 5, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"hostAddress", 6, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"canRead", 7, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"canWrite", 8, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
    {"className", 9, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
//#ifdef XP_WIN
    {"stderr", 10, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Stream_JSGet},
//#endif
    {0}
};

WRAP(Stream,Flush)
{
 GETOBJ(Stream,Stream,t);
 t->pos();
 return JS_TRUE;
}

WRAP(Stream,SetEndOfFile)
{
 if (argc != 1) ERR_COUNT(Stream,read);
 int32 len;
 if (!TOINT(0,len)) ERR_TYPE(Stream,SetEndOfFile,1,integer);
 GETOBJ(Stream,Stream,t);
 {
 FileStream* f;
 f = TYPESAFE_DOWNCAST(t,FileStream);
 if (f)
 {
  f->SetEndOfFile(len);
  RETBOOL(true);
 }
 }
   {
 MemoryStream* f;
 f = TYPESAFE_DOWNCAST(t,MemoryStream);
 if (f)
 {
  f->Maxsize = min(f->Maxsize,len);
  RETBOOL(true);
 }
  }
  {
 ByteStream* f;
 f = TYPESAFE_DOWNCAST(t,ByteStream);
 if (f)
 {
  f->Maxsize = min(f->Maxsize,len);
  RETBOOL(true);
 }
 }

 RETBOOL(false);
}

WRAP(Stream,Clear)
{
 if (argc > 1) ERR_COUNT(Stream,Clear);
 int32 size = 0;
 if (argc == 1 && !TOINT(0,size)) ERR_TYPE(Stream,Clear,1,integer);
 GETOBJ(Stream,Stream,t);
 MemoryStream * m;
 m = TYPESAFE_DOWNCAST(t,MemoryStream);
 if (!m) RETBOOL(false);

 m->Clear(size);
 RETBOOL(true);
}

WRAP(Stream,Resize)
{
 if (argc != 1) ERR_COUNT(Stream,Resize);
 int32 size = 0;
 if (!TOINT(0,size)) ERR_TYPE(Stream,Resize,1,integer);
 GETOBJ(Stream,Stream,t);
 MemoryStream * m;
 m = TYPESAFE_DOWNCAST(t,MemoryStream);
 if (m)
  {
   m->Resize(size);
   RETBOOL(true);
  }
 FileStream * f = TYPESAFE_DOWNCAST(t,FileStream);
 if (f)
  {
   f->SetEndOfFile(size);
   RETBOOL(true);
  }
 RETBOOL(true);
}

static JSBool
Stream_ToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
 GETOBJ(Stream,Stream,t);

 const char* s = t->filename();
 if (s) RETSTR(s);

 MemoryStream * m;
 ByteStream * b;
 m = TYPESAFE_DOWNCAST(t,MemoryStream);
 if (m) RETSTR((char*)*m);
 b = TYPESAFE_DOWNCAST(t,ByteStream);
 if (b) RETSTRN(b->Buf,b->Maxsize);
 RETSTRW(L"");
}

I_WRAP_I(Stream,Stream,seek)
I_WRAP_I(Stream,Stream,goforward)
I_WRAP_I(Stream,Stream,putback)
C_WRAP_V(Stream,Stream,peek)
C_WRAP_V(Stream,Stream,get)
V_WRAP_C(Stream,Stream,put)
V_WRAP_V(Stream,Stream,rewind)
I_WRAP_R(Stream,Stream,Write)
I_WRAP_R1(Stream,Stream,Read)
I_WRAP_R(Stream,Stream,WriteMIME)
//I_WRAP_R(Stream,Stream,ReadMIME)

WRAP(Stream,ReadMIME)
{
 TNameValueList * r1;
 GETOBJ(Stream,Stream,t);
 if (argc)
 {
  GETREC1(0,r1); if (!r1) ERR_TYPE(Stream,ReadMIME,1,record);
  RETINT(t->ReadMIME(*r1));
 }
 else
 {
  r1 = new TParameterList;
  t->ReadMIME(*r1);
  RETOBJ(Record_Object(cx,r1,true,0));
 }
}

WRAP(Stream,ReadTag)
{
 GETOBJ(Stream,Stream,t);

 bool EndTag=false;
 TStr type;
 MemoryStream text;
 TNameValueList * n=NULL;
 TStr allowedtags;

 if (argc > 0) GETREC1(0,n);

 TParameterList * p=NULL;
 p = TYPESAFE_DOWNCAST(n,TParameterList);

 if (argc > 1 && ISSTR(1))
  {
   allowedtags = STR(1);// ![CDATA[ is always an allowed tag
  }

 int lastc = t->StartTag(type,&text,&EndTag,allowedtags[0]?(char*)allowedtags:(char*)0);

 if (!strncasecmp(type, "![CDATA[",8))
  {
   size_t len = strlen(type);
   if (lastc == '>' && len > 8 && type[len-1] == ']' && type[len-2] == ']')
    {
     type[len-2] = 0;
     text.writestr(((char*)type)+8);
    }
   else
    {
     text.writestr(((char*)type)+8);
     if (len > 8 || !IsSpace(lastc)) text.put(lastc);
     t->ReadUntilWord("]]>",&text);
    }
   *rval = BOOLEAN_TO_JSVAL(false);
   JS_SetProperty(cx, obj,"hasChildren",rval);
   *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,text));
   JS_SetProperty(cx, obj,"tagText",rval);
   RETSTRW(L"![CDATA[");
  }
 else if (t->FinishTag(lastc,p,&EndTag))
 {
  *rval = BOOLEAN_TO_JSVAL((EndTag==false));
  JS_SetProperty(cx, obj,"hasChildren",rval);
  char * c = text;

  if (IsWhitespace(text,strlen(text)))
   c = (char*)"";
  else
  {
   c = StripCharsFB(c,"\r\n\f\v");
   //don't strip trailing spaces or tabs

   if (strchr(" \t",c[0]) && strchr(" \t",c[1]))
   {
    while (c[0] && strchr(" \t",c[0])) c++;
    //but do strip leading ones, except for the first space
   }
  }
  *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,c));
  JS_SetProperty(cx, obj,"tagText",rval);
  RETSTRWC(type);
 }
 else
 {
  *rval = BOOLEAN_TO_JSVAL(false);
  JS_SetProperty(cx, obj,"hasChildren",rval);
  *rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,text));
  JS_SetProperty(cx, obj,"tagText",rval);
  RETSTRW(L"");
 }
}

WRAP(Stream,WritePaired)
{
 GETOBJ(Stream,Stream,t);
 if (argc == 0 || argc > 3) ERR_COUNT(Stream,WritePaired);
 TPointer<TParameterList> pAutoDelete;
 TNameValueList * p;
 GETREC(0,p);
 if (!p) ERR_TYPE(Stream,WritePaired,0,record);
 const char * delim = 0, *eq = 0;
 if (argc >  1) delim = STR(1);
 if (argc == 3) eq = STR(2);
 RETINT(t->WritePaired(*p,delim,eq));
}

//readpaired(record, delimiter, equals sign)
WRAP(Stream,ReadPaired)
{
 GETOBJ(Stream,Stream,t);
 if (argc == 0 || argc > 3) ERR_COUNT(Stream,ReadPaired);
 TNameValueList * p;
 TPointer<TParameterList> pAutoDelete;
 GETREC(0,p);
 if (!p) ERR_TYPE(Stream,ReadPaired,0,record);
 const char * delim = 0, *eq = 0;
 if (argc >  1) delim = STR(1);
 if (argc == 3) eq = STR(2);
 if (!delim || !*delim) delim = "\n";
 if (!eq || !*eq) eq = "=";
 RETINT(t->ReadPaired(*p,*delim,*eq));
}

static JSBool Stream_ReadUntilWord(JSContext *cx,
 JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
 if (argc == 0 || argc > 2) ERR_COUNT(Stream,ReadUntilWord);
 if (!ISSTR(0)) ERR_TYPE(Stream,ReadUntilWord,1,string);
 Stream* out = 0;
 if (argc == 2) GETFILE(1,out);
 GETOBJ(Stream,Stream,t);
 RETBOOL(t->ReadUntilWord(STR(0),out));
}

static JSBool Stream_ReadUntilBytes(JSContext *cx,
 JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
 if (argc == 0 || argc > 2) ERR_COUNT(Stream,ReadUntilBytes);
 if (!ISSTR(0)) ERR_TYPE(Stream,ReadUntilBytes,1,string);
 JSString* j0;
 char* s0;
 GETSTRING(0);
 if (!j0) RETBOOL(false);
 if (!s0 || !*s0) RETBOOL(true);
 size_t length = JS_GetStringLength(j0);
 Stream* out = 0;
 if (argc == 2) GETFILE(1,out);
 GETOBJ(Stream,Stream,t);
 RETBOOL(t->ReadUntilBytes(s0, length, out));
}

C_WRAP_S(Stream,Stream,EatChars)

WRAP(Stream,ReadFile)
{
 if (argc > 2) ERR_COUNT(Stream,ReadFile);
 GETOBJ(Stream,Stream,t);
 MemoryStream dest;
 Stream * x;
 int size = INT_MAX;
 int local = 1;

 if (ISINT(0))      size = INT(0);
 else if (ISINT(1)) size = INT(1);

 GETFILE(0,x);
 if (!x) GETFILE(1,x);

 if (!x) x = &dest;
 else local = 0;

 size = x->Append(*t, size);

 if (local) RETSTRN(dest,dest.size());

 RETINT(size);
}

WRAP(Stream,ReadText)
{
 if (argc > 2) ERR_COUNT(Stream,ReadText);
 GETOBJ(Stream,Stream,t);
 MemoryStream dest;
 int32 size = INT_MAX;
 if (argc > 1) TOINT(0,size);
 dest.AppendAsText(*t, size);
 RETSTRN(dest,dest.size());
}

WRAP(Stream,ReadLine)
{
 if (argc > 1) ERR_COUNT(Stream,ReadLine);
 GETOBJ(Stream,Stream,t);
 TStr dest;
 const char* delim = 0;
 if (argc == 1)
  {
   if (!ISSTR(0)) ERR_TYPE(Stream,ReadLine,1,string);
   delim = STR(0);
  }
 if (!delim || !*delim) delim = "\n";
 t->readline(dest,delim[0]);
 RETSTR(dest);
}

WRAP(Stream,ReadMessage)
{
 if (argc > 1) ERR_COUNT(Stream,ReadLine);
 GETOBJ(Stream,Stream,t);
 MemoryStream output;

 const char* delim = "\n";
 if (argc == 1)
  {
   if (!ISSTR(0)) ERR_TYPE(Stream,ReadLine,1,string);
   delim = STR(0);
  }

 char s;
 if (!t->read(&s,1)) RETOBJ(0);

 while (s != delim[0])
 {
     output.write(&s,1);
     if (!t->read(&s,1)) if (t->eof()) break;
 }
 RETSTR((char*)output);
}

WRAP(Stream,read16)
{
 if (argc != 1) ERR_COUNT(Stream,read);
 int32 len;
 if (!TOINT(0,len)) ERR_TYPE(Stream,Read,1,integer);

 GETOBJ(Stream,Stream,t);

 if (!len) RETSTRW(L"");
 len *= 2;
 TChars s(len);
 s[len=t->read(s,len)]=0;
 RETSTRWN((jschar*)(char*)s,len/2);
}

WRAP(Stream,read)
{
 if (argc == 2 || argc == 0) return CALL(Stream,ReadFile);
 if (argc != 1) ERR_COUNT(Stream,read);
 int32 len;
 if (!TOINT(0,len)) ERR_TYPE(Stream,Read,1,integer);

 GETOBJ(Stream,Stream,t);

 if (!len) RETSTRW(L"");
 TChars s(len);
 s[len=t->read(s,len)]=0;
 RETSTRN((char*)s,len);
}

WRAP(Stream,write16)
{
 if (argc == 0) RETINT(0); //ERR_COUNT(Stream,write);
 GETOBJ(Stream,Stream,t);
 int ret = 0;

 for (int i = 0; i < argc; i++)
 {
   Stream* in;
   GETFILE(i,in);
   if (in)
     t->Append(*in);
   else
   {
      GETUCS2(i);
      if (ji)
      {
       int len = JS_GetStringLength(ji);
       ret += t->write(si,len*2);
      }
   }
 }
 RETINT(ret);
}

WRAP(Stream,write)
{
 if (argc == 0) RETINT(0); //ERR_COUNT(Stream,write);
 GETOBJ(Stream,Stream,t);
 int ret = 0;

 for (int i = 0; i < argc; i++)
 {
   Stream* in;
   GETFILE(i,in);
   if (in)
     t->Append(*in);
   else
   {
      char * si;
      JSString* ji;
      GETSTRING(i);

      if (ji)
      {
       int len = JS_GetStringLength(ji);
       ret += t->write(si,len);
      }
   }
 }
 RETINT(ret);
}

#if 0  //unused
class ArgList: public TNameValueList
{
 public:
 const char ** args;
 size_t length;
 ArgList(const char**a, size_t l) : args(a), length(l) {};
 ~ArgList() {};

 size_t Count() {return length;}
 const char* Name (size_t i)
  {return i < length ? args[2 * i] : 0; }

 const char* Value (size_t i)
  {return i < length ? args[2 * i + 1]  : 0; }
};
#endif

#ifdef RSLIB_FORMAT
WRAP(Stream,Format)
{
 if (argc  == 0) return JS_TRUE;
 if (argc == 1) CALL(Stream,write);
 GETOBJ(Stream,Stream,t);
 //size_t l=0;

 TNameValueList * p=0;
 TPointer<TParameterList> pAutoDelete;
 GETREC(1,p);

 Stream* in;
 GETFILE(0,in);

 if (!p) ERR_TYPE(Stream,Format,2,Record);

 const char* start = 0;
 const char* end = 0;

 if (argc == 3 && ISSTR(2)) start = end = STR(2);
 if (argc == 4 && ISSTR(2) && ISSTR(3))
  {
   start = STR(2);
   end = STR(3);
  }

 if (!start || !*start) start = "{";
 if (!end || !*end) end = "}";

 if (in)
 {
  FormatText(*p,*in,*t,start,end);
 }
 else
 {
   //char * s;
   //JSString* j = JS_ValueToString(cx,argv[0]);
   GETUTF8(0);
   if (j0)
   {
    //s = JS_GetStringBytes(j);
    ByteStream src(s0);
    FormatText(*p,src,*t,start,end);
   }
 }
 return JS_TRUE;
}
#endif

WRAP(Stream,writeln)
{
 if (argc) CALL(Stream,write);
 GETOBJ(Stream,Stream,t);
 t->write("\n",1);
 return JS_TRUE;
}

#define SWAP32(i) ( ((i << 24)& 0xff000000) | ((i << 8) & 0x00ff0000) | ((i >> 8) & 0x0000ff00) | ((i >> 24) & 0x000000ff))

#ifdef IS_LITTLE_ENDIAN
#define INTEL(x) x
#define NETWORK(x) SWAP32(x)
#else
#define INTEL(x) SWAP32(x)
#define NETWORK(x) x
#endif


WRAP(Stream,ReadInt)
{
 GETOBJ(Stream,Stream,t);
 int32 i;
 t->read(&i,sizeof(i));
 if (argc > 1 && JSVAL_TO_BOOLEAN(argv[1]))
   i = NETWORK(i);
 else
   i = INTEL(i);
 RETINT(i);
}

WRAP(Stream,WriteInt)
{
 if (argc == 0) ERR_COUNT(Stream,WriteInt);
 GETOBJ(Stream,Stream,t);
 int32 i;
 TOINT(0,i);
 if (argc > 1 && JSVAL_TO_BOOLEAN(argv[1]))
   i = NETWORK(i);
 else
   i = INTEL(i);
 RETBOOL(t->write(&i,sizeof(i)) != 0);
}

WRAP(Stream,ReadDouble)
{
 GETOBJ(Stream,Stream,t);
 double i;
 t->read(&i,sizeof(i));
 *rval = DOUBLE_TO_JSVAL(JS_NewDouble(cx,i));
 return JS_TRUE;
}

WRAP(Stream,WriteDouble)
{
 if (argc == 0) ERR_COUNT(Stream,WriteDouble);
 GETOBJ(Stream,Stream,t);
 double i;
 TODBL(0,i);
 RETBOOL(t->write(&i,sizeof(i)) != 0);
}

WRAP(Stream,ReadFloat)
{
 GETOBJ(Stream,Stream,t);
 float i;
 t->read(&i,sizeof(i));
 *rval = DOUBLE_TO_JSVAL(JS_NewDouble(cx,i));
 return JS_TRUE;
}

WRAP(Stream,WriteFloat)
{
 if (argc == 0) ERR_COUNT(Stream,WriteDouble);
 GETOBJ(Stream,Stream,t);
 double d;
 TODBL(0,d);
 float i = d;
 RETBOOL(t->write(&i,sizeof(i)) != 0);
}

#undef INTEL
#undef NETWORK
#undef SWAP32

#define SWAP16(i) ( ((i >> 8) & 0x00ff) | ((i << 8) & 0xff00) )
#ifdef IS_LITTLE_ENDIAN
#define INTEL(x) x
#define NETWORK(x) SWAP16(x)
#else
#define INTEL(x) SWAP16(x)
#define NETWORK(x) x
#endif

WRAP(Stream,ReadInt16)
{
 GETOBJ(Stream,Stream,t);
 int16 i;
 t->read((void*)&i,sizeof(i));
 if (argc > 1 && JSVAL_TO_BOOLEAN(argv[1]))
   i = NETWORK(i);
 else
   i = INTEL(i);
 RETINT(i);
}

WRAP(Stream,WriteInt16)
{
 if (argc == 0) ERR_COUNT(Stream,WriteInt);
 GETOBJ(Stream,Stream,t);
 int32 x;
 TOINT(0,x);
 int16 i= (int16)x;
 if (argc > 1 && JSVAL_TO_BOOLEAN(argv[1]))
   i = NETWORK(i);
 else
   i = INTEL(i);
 t->write(&i,sizeof(i));
 RETBOOL(true);
}

WRAP(Stream,ReadUInt16)
{
 GETOBJ(Stream,Stream,t);
 uint16 i;
 t->read(&i,sizeof(i));
 if (argc > 1 && JSVAL_TO_BOOLEAN(argv[1]))
   i = NETWORK(i);
 else
   i = INTEL(i);
 RETINT(i);
}

WRAP(Stream,ReadInt8)
{
 GETOBJ(Stream,Stream,t);
 int8 i;
 t->read(&i,sizeof(i));
 RETINT(i);
}

WRAP(Stream,WriteInt8)
{
 if (argc == 0) ERR_COUNT(Stream,WriteInt);
 GETOBJ(Stream,Stream,t);
 int32 x;
 TOINT(0,x);
 int8 i= (int8)x;
 t->write(&i,sizeof(i));
 RETBOOL(true);
}

WRAP(Stream,ReadByte)
{
 GETOBJ(Stream,Stream,t);
 uint8 i;
 t->read(&i,sizeof(i));
 RETINT(i);
}

WRAP(Stream,WriteByte)
{
 if (argc == 0) ERR_COUNT(Stream,WriteInt);
 GETOBJ(Stream,Stream,t);
 int32 x;
 TOINT(0,x);
 uint8 i= (uint8)x;
 t->write(&i,sizeof(i));
 RETBOOL(true);
}


WRAP(Stream,Append)
{
 if (argc == 0) ERR_COUNT(Stream,Append);
 GETOBJ(Stream,Stream,t);
 int32 maxcopy = INT_MAX;
 Stream* in;
 GETFILE(0,in);
 if (!in) ERR_TYPE(Stream,Append,1,Stream);
 if (argc > 1 && ISINT(1)) maxcopy = INT(1);
 RETINT(t->Append(*in,maxcopy));
}

WRAP(Stream,AppendAsText)
{
 if (argc == 0) ERR_COUNT(Stream,Append);
 GETOBJ(Stream,Stream,t);
 int32 maxcopy = INT_MAX;
 Stream* in;
 GETFILE(0,in);
 if (!in) ERR_TYPE(Stream,AppendAsText,1,Stream);
 if (argc > 1 && ISINT(1)) maxcopy = INT(1);
 RETINT(t->AppendAsText(*in,maxcopy));
}
//I_WRAP_F(Stream,Append);
//I_WRAP_F(Stream,AppendAsText);

static JSFunctionSpec Stream_functions[] = {
    {"read",     Stream_read,      1},
    {"readMessage",     Stream_ReadMessage,      1},
    {"read16",     Stream_read16,      1},
    {"clear",    Stream_Clear, 0},
    {"resize",    Stream_Resize, 1},
    {"write",     Stream_write,      8},
    {"write16",     Stream_write16,      8},
    {"writeln",     Stream_writeln,     8},
    {"seek",     Stream_seek,      3},
    {"flush",    Stream_Flush, 0},
    {"goForward",     Stream_goforward,      3},
    {"goBack",    Stream_putback,          0},
    {"peek",    Stream_peek,          0},
    {"get",  Stream_get,    1},
    {"put",  Stream_put,    0},
    {"putBack",  Stream_putback,    1},
    {"rewind",  Stream_rewind,    2},
    {"writeListB",  Stream_Write,    1},
    {"readListB",  Stream_Read,    1},
    {"writeList",  Stream_WritePaired,    1},
    {"readList",Stream_ReadPaired ,1},
    {"writeMIME",Stream_WriteMIME,1},
    {"readMIME",Stream_ReadMIME,2},
    {"readUntil",Stream_ReadUntilWord,2},
    {"readUntilBytes",Stream_ReadUntilBytes,2},
    {"eatChars",Stream_EatChars,1},
    {"readln",Stream_ReadLine,0},
//    {"writeln",Stream_WriteLine,1},
    {"readLine",Stream_ReadLine,0},
    {"readFile",Stream_ReadFile,1},
    {"readText",Stream_ReadText,1},
    {"readTag",Stream_ReadTag,0},
    {"writeLine",Stream_writeln,1},
    {"readInt",Stream_ReadInt,1},
    {"writeInt",Stream_WriteInt,1},
    {"readInt32",Stream_ReadInt,1},
    {"writeInt32",Stream_WriteInt,1},
    {"readInt16",Stream_ReadInt16,1},
    {"writeInt16",Stream_WriteInt16,1},
    {"readByte",Stream_ReadByte,1},
    {"writeByte",Stream_WriteByte,1},
    {"readInt8",Stream_ReadInt8,1},
    {"writeInt8",Stream_WriteInt8,1},
    {"readUInt16",Stream_ReadUInt16,1},
    {"writeUInt16",Stream_WriteInt16,1}, //no difference in writing
    {"readUInt8",Stream_ReadByte,1},
    {"writeUInt8",Stream_WriteByte,1},
    {"readDouble",Stream_ReadDouble,1},
    {"writeDouble",Stream_WriteDouble,1},
    {"readFloat64",Stream_ReadDouble,1},
    {"writeFloat64",Stream_WriteDouble,1},
    {"readFloat32",Stream_ReadFloat,1},
    {"writeFloat32",Stream_WriteFloat,1},
    {"append",Stream_Append,1},
    {"appendText",Stream_AppendAsText,1},
    {"toString",Stream_ToString,0},
    {"close",Stream_Close,0},
    {"wait",Stream_Wait,1},
#ifdef RSLIB_FORMAT
    {"format",Stream_Format,16},
#endif
    {"setEndOfFile",Stream_SetEndOfFile,1},
    {0}
};

WRAP(Stream,HELP)
{
 MemoryStream msg;
 for (int i = 0;Stream_functions[i].name; i++)
 {
  msg << Stream_functions[i].name << "(";
  if (Stream_functions[i].nargs) msg  << Stream_functions[i].nargs;
  msg << ")\n";
 }
 RETSTR((char *)msg);
}

static JSFunctionSpec Stream_fnstatic[] = {
    {"help",  Stream_HELP,    0},
    {0}
};

static JSClass Stream_class = {
    "Stream", JSCLASS_HAS_PRIVATE,         //Stream_JSGet
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,Stream_JSFinalize
};

JSObject*
Stream_Object(JSContext *cx, Stream* t,bool autodelete,JSPointerBase* Parent)
{
 JSObject* obj;
 GETENV;
 MAKENEW(Stream);
 SETPRIVATE(obj,Stream,t,autodelete,Parent);
 return obj;
}

void Stream_InitClass(JSContext *cx, JSObject *obj)
{
 GETENV;
 INITCLASS(Stream);
}

JSClass* Stream_Class() {return &Stream_class;}
