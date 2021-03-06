#ifdef JS_THREADSAFE
class JSBlocker
{public:
 JSContext * c;
 JSBlocker(JSContext* cx) {JS_BeginRequest(cx);c=cx;}
 ~JSBlocker() {JS_EndRequest(c);}
};
class JSWaiter
{public:
 jsrefcount saveDepth;
 JSContext * c;
 JSWaiter(JSContext* cx) {saveDepth = JS_SuspendRequest(cx);c=cx;}
 ~JSWaiter() {JS_ResumeRequest(c,saveDepth);}
};
#endif

//Phase out the Record class. These are defined in wrap_record.cpp
void ObjectToList(JSContext* cx, JSObject* o, TNameValueList& list);
void ListToObject(JSContext* cx, TNameValueList& t, JSObject* o);

class JSRoot
{public:
 void* o;
 JSContext* c;
#if JS_VERSION > 180
 jsval v;
 JSRoot(JSContext* cx,jsval obj): c(cx), v(obj), o(0)
  {v = v && JS_AddValueRoot(cx,&v) ? v : 0;}
 JSRoot(JSContext* cx,void* obj): c(cx), o(obj), v(0)
  {o = o && JS_AddGCThingRoot(cx,&o) ? o : 0;}
 ~JSRoot()
  {
   o = o ? JS_RemoveGCThingRoot(c,&o),(void*)0: o;
   v = v ? JS_RemoveValueRoot(c,&v),0: v;
  }
#else
 JSRoot(JSContext* cx,jsval obj): c(cx), o((void*)obj)
  {o = o && JS_AddRoot(cx,&o) ? o : NULL;}
 JSRoot(JSContext* cx,void* obj): c(cx), o(obj)
  {o = obj && JS_AddRoot(cx,&o) ? obj : NULL;}
 ~JSRoot()
  {o = o ? JS_RemoveRoot(c,&o),(void*)NULL: o;}
#endif
};

JSBool SuspendGC(JSContext*cx, JSGCStatus flags);

class JSBlockGC
{public:
 JSContext* c;
 JSGCCallback old;
 JSBlockGC(JSContext* cx): c(cx)
  {old = JS_SetGCCallback(cx,SuspendGC);}
 ~JSBlockGC()
  {JS_SetGCCallback(c,old);}
};

JSBool JSBadClass(JSContext* cx);
//inline {JS_ReportError(cx,"Wrong object class"); return JS_FALSE;}

#if JS_VERSION > 180

#define JS_GetStringChars(s)  JS_GetStringCharsZ(cx,s)
#define NATIVE(name) JSBool name(JSContext *cx, uintN argc, jsval *vp)

#define GETARGS \
jsval* argv = JS_ARGV(cx, vp)

#define ARGV(x) JS_ARGV(cx,vp)[x]
//JSObject* obj = JS_THIS_OBJECT(cx, vp)

#define RVAL (&JS_RVAL(cx, vp))

#define WRAP(type,name) NATIVE(type ## _ ## name)

#define CALL(type,name) \
type ## _ ## name(cx, argc, vp)

#define GETUTF8(x) \
 JSString* j8 ## x = argc > x ? JS_ValueToString(cx,ARGV(x)) : 0; \
 size_t j8 ## x ## length = (j8 ## x) ? JS_GetStringEncodingLength(cx,j8 ## x) : 0; \
  TStr u ## x( j8 ## x ## length); \
  JS_EncodeStringToBuffer(j8 ## x , u##x.str, j8 ## x ## length)

#define GETUCS2(x) \
 JSString* j ## x = argc > x ? JS_ValueToString(cx,ARGV(x)) : 0; \
 uint16* s ## x = (j ## x) ? (uint16*)JS_GetStringChars(j ## x) : (uint16*)0

#define RETVAL(value) JS_SET_RVAL(cx,vp,value)

#define GETTHIS  JSObject* obj = JS_THIS_OBJECT(cx, vp)

//for property getters/setters
#define GETOBJ2(class,type,name) \
 jsval*vp = rval;\
 type * name = NULL; \
 if (GETCLASS != class ## _Class() ) return JSBadClass(cx);\
 JSPointer<type> *ptr_ ## name = (JSPointer<type>*)JS_GetPrivate(cx,obj); \
 if (ptr_ ## name) name = *ptr_ ## name; \
 if (!name) return JS_FALSE

#define GETOBJ(class,type,name) \
 GETTHIS;\
 type * name = NULL; \
 if (GETCLASS != class ## _Class() ) return JSBadClass(cx);\
 JSPointer<type> *ptr_ ## name = (JSPointer<type>*)JS_GetPrivate(cx,obj); \
 if (ptr_ ## name) name = *ptr_ ## name; \
 if (!name) return JS_FALSE

#define CONSTRUCTOR(x,t,ad,parent) \
  RETOBJ(x ## _Object(cx,t,ad,parent))

/* Can we number of memory copies for string arguments?
class JSStr
{   public:
    JSContext* cx;
    char* str;
    JSStr(JSContext* c,char* s): cx(c), str(s){}
    ~JSStr() {JS_free(cx,str);}
    char* operator () {return str;}
};*/

#else

#define jsid jsval

#define JS_GetStringCharsZ(cx,s)  JS_GetStringChars(s)

#define ARGV(x) argv[x]

#define NATIVE(name) \
JSBool name(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) \

#define WRAP(type,name) NATIVE(type ## _ ## name)

#define CALL(type,name) \
type ## _ ## name(cx, obj, argc, argv, rval)

#define GETARGS
#define GETTHIS

#define RETVAL(x) *rval = x

#define RVAL rval

#define GETOBJ(class,type,name) type * name = NULL; \
 if (GETCLASS != class ## _Class() ) return JSBadClass(cx);\
 JSPointer<type> *ptr_ ## name = (JSPointer<type>*)JS_GetPrivate(cx,obj); \
 if (ptr_ ## name) name = *ptr_ ## name; \
 if (!name) return JS_FALSE

#define GETOBJ2(c,t,n) GETOBJ(c,t,n)

#define GETUTF8(x) \
 JSString* j8 ## x = argc > x ? JS_ValueToString(cx,ARGV(x)) : 0; \
  TStr u ## x( (j8 ## x) ? JS_GetStringChars(j8 ## x) : (jschar*)0, \
   (j8 ## x) ? JS_GetStringLength(j8 ## x) : 0)

#define GETUCS2(x) \
 JSString* j ## x = argc > x ? JS_ValueToString(cx,ARGV(x)) : 0; \
  uint16* s ## x = (j ## x) ? (uint16*)JS_GetStringChars(j ## x) : (uint16*)0

#define CONSTRUCTOR(x,t,ad,parent) \
  SETPRIVATE(obj,x,t,ad,parent)


#endif

#ifdef JS_THREADSAFE
#define ENTERNATIVE(cx) JSBlockGC __BlockGC(cx); JSBlocker __Blocker(cx); TList<JSRoot> __Roots
#else
#define ENTERNATIVE(cx) JSBlockGC __BlockGC(cx); TList<JSRoot> __Roots
#endif

/* Version 1.8 doesn't need automatic rooting, because GC is blocked within a native method call */
/* SQLite still needs the root facility for storing callback functions*/
#if JS_VERSION > 180
#define ROOT(x) (x)
#else
#define ROOT(x) (__Roots.Add(new JSRoot(cx,x)),x)
#endif

#define ISDBL(x) JSVAL_IS_DOUBLE(ARGV(x))
#define ISINT(x) JSVAL_IS_INT(ARGV(x))
#define ISSTR(x) JSVAL_IS_STRING(ARGV(x))
#define ISBOOL(x) JSVAL_IS_BOOLEAN(ARGV(x))

#define ISOBJ(x) JSVAL_IS_OBJECT(ARGV(x))

#define TOBOOL(x,y) JS_ValueToBoolean(cx,ARGV(x),&(y))
#define TOINT(x,y) JS_ValueToInt32(cx,ARGV(x),&(y))
#define TODBL(x,y) JS_ValueToNumber(cx,ARGV(x),&(y))

#define GETPRIVATE(x,obj) (((JSPointer<x>*)JS_GetPrivate(cx,obj))->P)
#define GETPOINTER (((JSPointerBase*)JS_GetPrivate(cx,obj)))

#ifdef JS_THREADSAFE
#define GETCLASS JS_GetClass(cx,obj)
#else
#define GETCLASS JS_GetClass(obj)
#endif

#define SETPRIVATE(obj,x,t,ad,parent) \
 JS_SetPrivate(cx,obj,new JSPointer<x>(parent,t,ad))

#define DELPRIVATE(x) \
JSPointer<x> * t = \
   (JSPointer<x>*)JS_GetPrivate(cx,obj);\
 if (t) delete t; t=0; JS_SetPrivate(cx,obj,NULL)

#define CLOSEPRIVATE(class,x) \
if (GETCLASS != class ## _Class() ) return JSBadClass(cx);\
 JSPointer<x> * t = (JSPointer<x>*)JS_GetPrivate(cx,obj);\
 if (t) t->Close()

#ifdef JS_THREADSAFE
#define MAKENEW(name) \
obj = ROOT(JS_NewObject(cx, JS_GetClass(cx,Env->o##name),Env->o##name, NULL));\
JS_DefineFunctions(cx,obj,name ## _functions);\
JS_DefineProperties(cx,obj,name ## _properties)
#else
#define MAKENEW(name) \
obj = JS_NewObject(cx, JS_GetClass(Env->o##name),Env->o##name, NULL);\
JS_DefineFunctions(cx,obj,name ## _functions);\
JS_DefineProperties(cx,obj,name ## _properties)
#endif

#define INITCLASS(name) \
 Env->o##name = JS_InitClass(cx, obj, NULL, name ## _Class(),\
 name ## _ ## name, 0,\
 name ## _properties, name ## _functions,NULL,name ## _fnstatic);

#define GETREC(x,y)  \
{JSObject* jx = x < argc && JSVAL_IS_OBJECT(ARGV(x))? JSVAL_TO_OBJECT(ARGV(x)) : NULL; \
 if (jx) {\
  if (JS_InstanceOf(cx,jx,Record_Class(),0)) \
   { y = GETPRIVATE(TNameValueList,jx); }\
  else \
   { y = y ## AutoDelete = new TParameterList; ObjectToList(cx, jx, *y); }\
 } else y = 0;\
}

#define GETREC1(x,y)  \
{JSObject* jx = x < argc && JSVAL_IS_OBJECT(ARGV(x))? JSVAL_TO_OBJECT(ARGV(x)) : NULL; \
 if (jx) {\
  if (JS_InstanceOf(cx,jx,Record_Class(),0)) \
   y = GETPRIVATE(TNameValueList,jx); \
 } else y=0;\
}

#define GETFORM(x,y)  \
{JSObject* jx = x < argc && JSVAL_IS_OBJECT(ARGV(x))? JSVAL_TO_OBJECT(ARGV(x)) : NULL; \
 if (jx) {if (JS_InstanceOf(cx,jx,Form_Class(),0)) \
  y = GETPRIVATE(EZFForm,jx);} \
 else y = NULL;\
}

#define GETTABLE(x,y)  \
{JSObject* jx = x < argc && JSVAL_IS_OBJECT(ARGV(x))? JSVAL_TO_OBJECT(ARGV(x)) : NULL; \
 if (jx) {if (JS_InstanceOf(cx,jx,Table_Class(),0)) \
  y = GETPRIVATE(DataTable,jx);} \
 else y = NULL;\
}

#define GETFILE(x,y)  \
{JSObject* jx = x < argc&& JSVAL_IS_OBJECT(ARGV(x)) ? JSVAL_TO_OBJECT(ARGV(x)) : NULL; \
 if (jx && JS_InstanceOf(cx,jx,Stream_Class(),0)) \
  y = GETPRIVATE(Stream,jx); \
 else y = NULL;\
}

#define GETSTRN(x)\
 JSString* j ## x = argc > x ? JS_ValueToString(cx,ARGV(x)) : 0; \
 const char* s ## x = j ## x ? JS_GetStringBytes(j ## x) : 0; \
 size_t l ## x = j ## x ? JS_GetStringLength(j ## x) : 0

#define GETSTRING(x) \
 {j ## x = argc > x ? JS_ValueToString(cx,ARGV(x)) : 0; \
  s ## x = (j ## x) ? JS_GetStringBytes(j ## x) : 0;}

// JSString* j ## x = argc > x ? JS_ValueToString(cx,argv[x]) : 0;
//  TStr s ## x( (j ## x) ? JS_GetStringBytes(j ## x) : (char*)0)

#define INT(x) JSVAL_TO_INT(ARGV(x))
//doesn't work on 1.85
#define STR(x) JS_GetStringBytes(JSVAL_TO_STRING(ARGV(x)))
#if JS_VERSION > 180
#define WSTR(x) JS_GetStringCharsZ(cx,JSVAL_TO_STRING(ARGV(x)))
#else
#define WSTR(x) JS_GetStringChars(JSVAL_TO_STRING(ARGV(x)))
#endif
//#define UTF8STR(x) TStr((const uint16*)JS_GetStringCharsZ(cx,JSVAL_TO_STRING(ARGV(x))))
//#define DBL(x) JSVAL_TO_DOUBLE(ARGV(x))
//#define TF(x) JSVAL_TO_BOOLEAN(ARGV(x))

//RSLIB converts UTF-8 to UCS-2 to return a unicode string.
#define RETSTRWC(c) \
{WStr x(c); JSString*str = JS_NewUCStringCopyZ(cx,(jschar*)(wchar_t*)x); \
 if (!str) return JS_FALSE; \
 RETVAL(STRING_TO_JSVAL(str)); \
 return JS_TRUE; }

#define RETSTRW(x) \
{JSString*str = JS_NewUCStringCopyZ(cx,(jschar*)(wchar_t*)x); \
 if (!str) return JS_FALSE; \
 RETVAL(STRING_TO_JSVAL(str)); \
 return JS_TRUE; }

#define RETSTRWN(x,n) \
{JSString*str = JS_NewUCStringCopyN(cx,x,n); \
 if (!str) return JS_FALSE; \
 RETVAL(STRING_TO_JSVAL(str)); \
 return JS_TRUE; }

#define RETSTR(x) \
{JSString*str = JS_NewStringCopyZ(cx,x); \
 if (!str) return JS_FALSE; \
 RETVAL(STRING_TO_JSVAL(str)); \
 return JS_TRUE; }

#define RETSTRN(x,n) \
{JSString*str = JS_NewStringCopyN(cx,x,n); \
 if (!str) return JS_FALSE; \
 RETVAL(STRING_TO_JSVAL(str)); \
 return JS_TRUE; }

#define RETBOOL(x) {RETVAL(BOOLEAN_TO_JSVAL((x)?JS_TRUE:JS_FALSE)); return JS_TRUE ;}

#define RETOBJ(x) {RETVAL(OBJECT_TO_JSVAL(x)); return JS_TRUE;}

//#define RETINT(x) {*rval = INT_TO_JSVAL(x); return JS_TRUE;}

#if JS_VERSION > 180
#define RETINT(y) \
{\
 int x = y;\
 RETVAL(INT_TO_JSVAL(x));\
 return JS_TRUE;\
}
#else
#define RETINT(y) \
{int x = y; \
 if (INT_FITS_IN_JSVAL(x)) \
  RETVAL(INT_TO_JSVAL(x)); \
 else                      \
  RETVAL(DOUBLE_TO_JSVAL(JS_NewDouble(cx,x)));\
 return JS_TRUE; \
}
#endif

#if JS_VERSION > 180
#define RETUINT(x) RETVAL(UINT_TO_JSVAL(x));
#else
#define RETUINT(y) \
{int x = y; \
 if (x <= JSVAL_INT_MAX) \
  RETVAL(UINT_TO_JSVAL(x)); \
 else                      \
  RETVAL(DOUBLE_TO_JSVAL(JS_NewDouble(cx,x)));\
 return JS_TRUE;\
}
#endif

#define MAYBEGC \
 if (++Env->GCTimer > (2<<10)) {Env->GCTimer=0;JS_MaybeGC(cx);}

#define GETENV \
 JSDBEnvironment* Env = (JSDBEnvironment*)JS_GetContextPrivate(cx);\
 MAYBEGC

#define WRITELN(x) {if (Env->out) Env->out->writestr(x,"\n");}

#define ERR_COUNT(class,name) \
 {JS_ReportError(cx,"Wrong number of parameters in call to %s.%s",#class,#name); \
 return JS_FALSE; }

#define ERR_TYPE(class,name,index,type) \
 {JS_ReportError(cx,"Expected a %s in parameter %d for %s.%s",#type,index,#class,#name); \
 return JS_FALSE; }

#define ERR_XDB(class,x) \
 { JS_SetPendingException(cx,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,TStr(#class " (", x.why(),":",x.info(), ")"))));\
  RETVAL(OBJECT_TO_JSVAL(NULL)); return JS_FALSE; }

#define ERR_MSG(class,name,msg) \
 { JS_SetPendingException(cx,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,TStr(#class ":" #name " (", msg, ")"))));\
  RETVAL(OBJECT_TO_JSVAL(NULL)); return JS_FALSE; }
/*JS_ReportError(cx,"%s.%s: %s",#class,#name,msg); */

/*#define WARN_MSG(class,name,msg) \
 {JS_ReportError(cx,"%s.%s: %s",#class,#name,msg); \
 return JS_TRUE; } */

#define I_WRAP_S(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 GETOBJ(class,type,t); \
 RETINT(t->name(STR(0))); \
}

#define B_WRAP_S(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 GETOBJ(class,type,t); \
 RETBOOL(t->name(TStr(WSTR(0)))); \
}

#define B_WRAP_SET_R(class,type,list,name) WRAP(type,_set ## name)\
{ \
 if (argc != 2) ERR_COUNT(type,name); \
 int32 index; TNameValueList * r1; \
 if (!TOINT(0,index)) ERR_TYPE(type,name,1,integer); \
 TPointer<TParameterList> r1AutoDelete; \
 GETREC(1,r1); if (!r1) ERR_TYPE(type,name,2,record); \
 GETOBJ(class,type,t);\
 if (t->list[index] == NULL) RETBOOL(false);\
 if (&t->list[index]->name == r1) RETBOOL(true);\
 t->list[index]->name.Clear();\
 t->list[index]->name.Append(*r1);\
 RETBOOL(true);\
}

#define R_WRAP_GET(class,type,list,name) WRAP(type,_get ## name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 int32 index; TNameValueList * r1=0; \
 if (!TOINT(0,index)) ERR_TYPE(type,name,1,integer); \
 if (argc == 2)\
 {TPointer<TParameterList> r1AutoDelete; \
 GETREC(1,r1); if (!r1) ERR_TYPE(type,name,2,record); }\
 GETOBJ(class,type,t);\
 if (t->list[index] == NULL) RETOBJ(NULL); \
 if (r1) \
  {*r1 = t->list[index]->Responses; RETOBJ(NULL); }\
 else \
  {RETOBJ(Record_Object(cx,&t->list[index]->name,false,ptr_t));}\
}

#define B_WRAP_SET_S(class,type,list,name) WRAP(type,_set ## name)\
{int32 index; \
 if (argc != 2) ERR_COUNT(type,name); \
 if (!TOINT(0,index)) ERR_TYPE(type,name,1,string); \
 if (!ISSTR(1)) ERR_TYPE(type,name,1,string); \
 GETOBJ(class,type,t); \
 if (t->list[index] == NULL) RETBOOL(false);\
 t->list[index]->name= TStr(WSTR(1));\
 RETBOOL(true);\
}

#define S_WRAP_GET(class,type,list,name) WRAP(type,_get ## name)\
{int32 index; \
 if (argc != 1) ERR_COUNT(type,name); \
 if (!TOINT(0,index)) ERR_TYPE(type,name,1,string); \
 GETOBJ(class,type,t); \
 if (t->list[index] == NULL) RETSTRW(L"");\
 RETSTRWC(t->list[index]->name);\
}

#define B_WRAP_SET_I(class,type,list,name) WRAP(type,_set ## name)\
{int32 index; \
 if (argc != 2) ERR_COUNT(type,name); \
 if (!TOINT(0,index)) ERR_TYPE(type,name,1,string); \
 if (!ISINT(1)) ERR_TYPE(type,name,1,integer); \
 GETOBJ(class,type,t); \
 if (t->list[index] == NULL) RETBOOL(false);\
 t->list[index]->name = INT(1);\
 RETBOOL(true);\
}

#define I_WRAP_GET(class,type,list,name) WRAP(type,_get ## name)\
{int32 index; \
 if (argc != 1) ERR_COUNT(type,name); \
 if (!TOINT(0,index)) ERR_TYPE(type,name,1,string); \
 GETOBJ(class,type,t); \
 if (t->list[index] == NULL) RETBOOL(false);\
 RETINT(t-> list [index]-> name);\
}

#define B_WRAP_S_F(class,type,name)WRAP(type,name)\
{ \
 if (argc == 0 || argc > 2) ERR_COUNT(type,name); \
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 Stream* out = 0; \
 if (argc == 2) GETFILE(1,out); \
 GETOBJ(class,type,t); \
 RETBOOL(t->name(STR(0),out)); \
}

#define B_WRAP_IR(class,type,name) WRAP(type,name)\
{ \
 if (argc != 2) ERR_COUNT(type,name); \
 int32 v0; TNameValueList * r1; \
 if (!TOINT(0,v0)) ERR_TYPE(type,name,1,integer); \
 TPointer<TParameterList> r1AutoDelete; \
 GETREC(1,r1); if (!r1) ERR_TYPE(type,name,2,record); \
 GETOBJ(class,type,t);\
 RETBOOL(t->name(v0,*r1)); \
}

#define C_WRAP_S(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string);\
 jschar s[2]; s[1]=0; \
 GETOBJ(class,type,t);\
 s[0] = t->name(STR(0)); \
 RETSTRW(s); \
}

#define C_WRAP_V(class,type,name)WRAP(type,name)\
{ \
 if (argc != 0) ERR_COUNT(type,name); \
 jschar s[2]; s[1]=0; \
 GETOBJ(class,type,t);\
 s[0] = t->name(); \
 RETSTRW(s); \
}

#define V_WRAP_C(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 GETOBJ(class,type,t);\
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 t->name((STR(0))[0]); \
 RETBOOL(true);\
}

#define I_WRAP_R(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 TNameValueList * r1; \
 TPointer<TParameterList> r1AutoDelete; \
 GETREC(0,r1); if (!r1) ERR_TYPE(type,name,1,record); \
 GETOBJ(class,type,t);\
 RETINT(t->name(*r1)); \
}

#define I_WRAP_R1(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 TNameValueList * r1; \
 GETREC1(0,r1); if (!r1) ERR_TYPE(type,name,1,record); \
 GETOBJ(class,type,t);\
 RETINT(t->name(*r1)); \
}

#define I_WRAP_F(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 Stream * r1; \
 GETFILE(0,r1); \
 if (!r1) ERR_TYPE(type,name,1,stream); \
 GETOBJ(class,type,t);\
 RETINT(t->name(*r1)); \
}

#define B_WRAP_IS(class,type,name) WRAP(type,name)\
{ \
 if (argc != 2) ERR_COUNT(type,name); \
 int32 v0;\
 if (!TOINT(0,v0)) ERR_TYPE(type,name,1,integer); \
 if (!ISSTR(1)) ERR_TYPE(type,name,2,string); \
 GETOBJ(class,type,t); \
 RETBOOL(t->name(v0,STR(1))); \
}

#define I_WRAP_SS(class,type,name) WRAP(type,name)\
{ \
 if (argc != 2) ERR_COUNT(type,name); \
 int32 v1;\
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 if (!ISSTR(1)) ERR_TYPE(type,name,2,string); \
 GETOBJ(class,type,t); \
 RETINT(t->name(STR(0),STR(1))); \
}

#define V_WRAP_SS(class,type,name)WRAP(type,name)\
{ \
 if (argc != 2) ERR_COUNT(type,name); \
 int32 v1;\
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 if (!ISSTR(1)) ERR_TYPE(type,name,2,string); \
 GETOBJ(class,type,t); \
 t->name(STR(0),STR(1)); \
 RETBOOL(true); \
}

#define V_WRAP_II(class,type,name) WRAP(type,name)\
{ \
 if (argc != 2) ERR_COUNT(type,name); \
 int32 v1;\
 if (!ISINT(0)) ERR_TYPE(type,name,1,integer); \
 if (!ISINT(1)) ERR_TYPE(type,name,2,integer); \
 GETOBJ(class,type,t); \
 t->name(INT(0),INT(1)); \
 RETBOOL(true); \
}

#define V_WRAP_I(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 int32 v1;\
 if (!ISINT(0)) ERR_TYPE(type,name,1,integer); \
 GETOBJ(class,type,t); \
 t->name(INT(0)); \
 RETBOOL(true); \
}

#define V_WRAP_S(class,type,name) WRAP(type,name)\
{ \
 if (argc != 1) ERR_COUNT(type,name); \
 int32 v1;\
 if (!ISSTR(0)) ERR_TYPE(type,name,1,string); \
 GETOBJ(class,type,t); \
 t->name(STR(0)); \
 RETBOOL(true); \
}

#define B_WRAP_I(class,type,name) WRAP(type,name)\
{ \
 if (argc == 0) ERR_COUNT(type,name); \
 int32 v1;\
 if (!TOINT(0,v1)) ERR_TYPE(type,name,1,integer); \
 GETOBJ(class,type,t); \
 RETBOOL(t->name(v1)); \
}

#define S_WRAP_I(class,type,name) WRAP(type,name)\
{ \
 if (argc == 0)  ERR_COUNT(type,name); \
 int32 v1; if (!TOINT(0,v1)) ERR_TYPE(type,name,1,integer); \
 GETOBJ(class,type,t); \
 RETSTRW(WStr(t->name(v1))); \
}


#define S_WRAP_II(class,type,name) WRAP(type,name)\
{ \
 if (argc == 0)  ERR_COUNT(type,name); \
 int32 v1; if (!TOINT(0,v1)) ERR_TYPE(type,name,1,integer); \
 int32 v2; if (!TOINT(0,v2)) ERR_TYPE(type,name,2,integer); \
 GETOBJ(class,type,t); \
 RETSTRW(TStr(t->name(v1,v2))); \
}

#if 0
#define S_WRAP_II(class,type,name) \
static JSBool type ## _ ## name(JSContext *cx, \
 JSObject *obj, uintN argc, jsval *argv, jsval *rval) \
{ \
 if (argc == 0)  ERR_COUNT(type,name); \
 int32 v1; if (!TOINT(0,v1)) ERR_TYPE(type,name,1,integer); \
 int32 v2; if (!TOINT(0,v2)) ERR_TYPE(type,name,2,integer); \
 GETOBJ(class,type,t); \
 RETSTR(t->name(v1,v2)); \
}
#endif


#define I_WRAP_I(class,type,name) WRAP(type,name)\
{ \
 if (argc == 0) ERR_COUNT(type,name); \
 int32 v1; if (!TOINT(0,v1)) ERR_TYPE(type,name,1,integer); \
 GETOBJ(class,type,t); \
 RETINT(t->name(v1)); \
}


#define I_WRAP_V(class,type,name) WRAP(type,name)\
{ \
 if (argc != 0) ERR_COUNT(type,name); \
 GETOBJ(class,type,t); \
 RETINT(t->name()); \
}

#if 0 //never used
#define S_WRAP_V(class,type,name) WRAP(type,name)\
{ \
 GETOBJ(class,type,t); \
 RETSTRW(TStr(t->name())); \
}

#define B_WRAP_V(class,type,name) WRAP(type,name)\
{ \
 GETOBJ(class,type,t); \
 RETBOOL(t->name()); \
}
#endif

#define V_WRAP_V(class,type,name) WRAP(type,name)\
{ \
 GETOBJ(class,type,t); \
 t->name(); \
 RETBOOL(true); \
}

#define WRAP_HELP(class,text) WRAP(class,HELP)\
{ RETSTRW(text); }

