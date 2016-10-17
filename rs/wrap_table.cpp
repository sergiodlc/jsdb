#pragma hdrfile "jsdb.csm"
#include "rslib.h"
#include "jsdb.h"
#include "rs/wrap_jsdb.h"
#pragma hdrstop

#include "rs/tbl_sql.h"

I_WRAP_S(Table,DataTable,FindColumn)
//S_WRAP_V(DataTable,GetLastError)
S_WRAP_I(Table,DataTable,ColumnTitle)
B_WRAP_IS(Table,DataTable,SetTitle)
I_WRAP_I(Table,DataTable,GetColWidth)
I_WRAP_S(Table,DataTable,AddCol)
B_WRAP_I(Table,DataTable,DelCol)

/*
  Table.create('filename.dbf','fieldname [type] length, fieldname [type] length, ...')
  Table.update('filename.dbf',...)
  type can be C N T D
*/

WRAP(Table,Create)
{
 if (argc < 2) ERR_COUNT(Table,Create);
 if (!ISSTR(0)) ERR_TYPE(Table,Create,0,String);
 if (!ISSTR(1)) ERR_TYPE(Table,Create,1,String);

 GETUTF8(0);
 GETUTF8(1);

 if (FileExists(u0)) ERR_MSG(Table,Create,"File already exists");
 TStringList fields(u1,",");
 if (fields.Count() == 0) RETBOOL(false);

 DBFHeader header(DBFHeader::dBase);
 char *name; // "fieldame C 45"
 char *type;
 char *length;
 int l;

 FOREACH(char *name, fields)
  name = StripCharsFB(name," \t\r\n");
  type = strchr(name,' ');
  l = 255;
  if (type)
  {
   while (*type == ' ')
     *type++ = 0;

   length = strchr(type,' ');

   if (length)
    while (*length == ' ')
     *length++ = 0;
   else
    length = type;

   l = atoi(length);
  }
  else
  {
   type=(char*)"C";
  }

  if (!l) l = 255;
  name = StripCharsFB(name," \t\r\n");
  type = StripCharsFB(type," \t\r\n");

  EDBFieldType t;
  switch(toupper(type[0]))
  {
    case 'D': t = db_ft_Date; break;
    case 'T': t = db_ft_Time; break;
    case 'N': t = db_ft_Number;break;
    default: t = db_ft_Char;
  }
  header.AddField(name,l,t);
 DONEFOREACH

 if (header.FieldCount())
  RETBOOL(header.CreateFile(u0)); //writes a dbf header

 RETBOOL(false);
}

int TransferData(DataTable& D,DataTable& S,JSContext *cx,JSObject* obj,jsval Callback,int argc,jsval *argv)
{
 if (*S.Filename && S.Filename == D.Filename) return false;
 if ((void*)&S == (void*)&D) return false;
  //both refer to the same table

#ifndef TBL_NO_SQL
 ODBCTable * odbc = TYPESAFE_DOWNCAST(&D,ODBCTable);
 if (odbc) odbc->GetDataC(0,0);
#endif

 TParameterList P;
 count_t casenum = 1;
 jsval status;

 jsval* params = NULL;
 TList<JSRoot> __Roots;

 if (Callback != JSVAL_NULL)
 {
     params = new jsval[argc + 1];
     for (int i=0; i<argc; i++)
     {
         __Roots.Add(new JSRoot(cx,argv[i]));
         params[i+1] = argv[i];
     }
     argc++;
 }
 jsval Jfalse = BOOLEAN_TO_JSVAL(false);

 while (S.GetRow(casenum,P))
 {
     if (Callback)
     {
         params[0] = INT_TO_JSVAL(casenum);
         JS_CallFunctionValue(cx, obj, Callback, argc, params, &status);

         if (status == Jfalse)
           break;
     }
     D.AddRow(P);
     P.Clear();
     casenum++;
 }

 if (params) delete[] params;
 D.Save();
 return true;
}

//Table.append(source, callback, opaque, ...)
//callback(#,opaque, ...)
WRAP(Table,Append)
{
    if (argc == 0) ERR_COUNT(Table,Append)
    GETOBJ(Table,DataTable,d);

    DataTable *s = NULL;
    JSObject* js = JSVAL_IS_OBJECT(argv[0])? JSVAL_TO_OBJECT(argv[0]) : NULL;
    if (js)
      if (JS_InstanceOf(cx,js,Table_Class(),0))
       s = GETPRIVATE(DataTable,js);

    if (!s) ERR_TYPE(Table,Append,1,Table);

    jsval callback = JSVAL_NULL;
    if (argc > 1 && JSVAL_IS_OBJECT(argv[1]))
      if (JS_ObjectIsFunction(cx,JSVAL_TO_OBJECT(argv[1])))
        callback = argv[1];

    JSRoot rs(cx,js);
    JSRoot rc(cx,argv[1]);

    RETBOOL(TransferData(*d,*s,cx,obj,callback,argc-2, argv+2));
}

WRAP(Table,GetWhere)
{
#ifndef TBL_NO_SQL
 GETOBJ(Table,DataTable,t);
 ODBCTable * odbc = TYPESAFE_DOWNCAST(t,ODBCTable);
 if (odbc)
 {
    int32 row;
    if (!TOINT(0,row)) row=1;
    MemoryStream m;
    odbc->GetWhere(m,row);
    RETSTR(m);
 }
#else
 ERR_MSG(Table,GetWhere,"JSDB was compiled without ODBC support")
#endif
}

WRAP(Table,GetRow)
{
 if (argc == 1)
  {
   int32 v0;
   if (!TOINT(0,v0)) ERR_TYPE(Table,getRow,1,integer);
   GETOBJ(Table,DataTable,t);
   TParameterList * r1 = new TParameterList;
   if (!t->GetRow(v0,*r1))
   {
    delete r1;
    ERR_MSG(Table,"Row not available",itos(v0));
   }
   RETOBJ(Record_Object(cx,r1,true,NULL));
  }
 else if (argc == 2)
  {
   int32 v0;
   TNameValueList * r1;
   if (!TOINT(0,v0)) ERR_TYPE(Table,getRow,1,integer);
   GETREC1(1,r1); if (!r1) ERR_TYPE(Table,getRow,2,record);
   GETOBJ(Table,DataTable,t);
   if (!t->GetRow(v0,*r1))
   {
    ERR_MSG(Table,"Row not available",itos(v0));
   }
  }
 else
  ERR_COUNT(Table,getRow);
 RETOBJ(NULL);
}

B_WRAP_IR(Table,DataTable,SetRow)
//I_WRAP_R(DataTable,AddRow)

WRAP(Table,AddRow)
{
 if (argc != 1) ERR_COUNT(Table,name);
 GETOBJ(Table,DataTable,t);

 TNameValueList * r1;
 TPointer<TParameterList> r1AutoDelete;
 GETREC(0,r1);

 if (r1)
  RETINT(t->AddRow(*r1));

 if (ISSTR(0))
 {
  GETUTF8(0);
  GETUTF8(1);
  if (!u1 || !*u1) u1 = ",";
  TParameterList r(u0,u1[0]);
  RETINT(t->AddRow(r));
 }

if (JSVAL_IS_OBJECT(argv[0]))
 {
  TParameterList dt;
  JSObject *o = JSVAL_TO_OBJECT(argv[0]);
  for (int index = 1; index <= t->ColumnCount(); index++)
   {
       jsval v;
       const char* name = t->Name(index);
       if (!JS_GetProperty(cx,o,name,&v)) continue;
       JSString * value = JS_ValueToString(cx,v);
       TStr sval( (value) ? JS_GetStringChars(value) : (jschar*)0,
             (value) ? JS_GetStringLength(value) : 0); //automatic conversion to UTF-8
       dt.Set(name,sval);
   }
   RETINT(t->AddRow(dt));
 }

 RETBOOL(false);
}

WRAP(Table,FindRow)
{
 int32 v1 = 0, v2=1;
 TNameValueList * r0;

 if (argc == 0 || argc > 3) ERR_COUNT(Table,FindRow)

 TPointer<TParameterList> r0AutoDelete;
 GETREC(0,r0);
 if (!r0) ERR_TYPE(Table,FindRow,1,record);

 if (argc == 2)
 if (!TOINT(1,v1)) ERR_TYPE(Table,FindRow,2,integer);

 if (argc == 3)
 {
  if (!TOINT(2,v2)) ERR_TYPE(Table,FindRow,3,integer);
 }

 if (v2 != 1 && v2 != -1) v2 = 1;

 GETOBJ(Table,DataTable,t);
 int32 row = t->FindRow(*r0,v1,v2);
 if (row)
 {
  *rval = INT_TO_JSVAL(v2);
  JS_SetProperty(cx, obj,"searchDirection",rval);
 }

 *rval = INT_TO_JSVAL(row);

 JS_SetProperty(cx, obj,"lastFind",rval);

 RETINT(row);
}

WRAP(Table,FindNext)
{
 if (argc != 1) ERR_COUNT(Table,FindNext);

 TNameValueList* r0;
 TPointer<TParameterList> r0AutoDelete;
 GETREC(0,r0);
 if (!r0) ERR_TYPE(Table,FindNext,1,record);

 if (!JS_GetProperty(cx,obj,"lastFind",rval))
   ERR_MSG(Table,FindNext,"you must call find() before findNext");

 int32 v1 = JSVAL_TO_INT(*rval);
 if (!v1) RETBOOL(false);
 v1++;

 if (!JS_GetProperty(cx,obj,"searchDirection",rval))
   ERR_MSG(Table,FindNext,"you must call find() before findNext");
 int32 v2 = JSVAL_TO_INT(*rval);

 if (v2 != 1 && v2 != -1) v2 = 1;

 GETOBJ(Table,DataTable,t);

 int32 row = t->FindRow(*r0,v1,v2);
 if (row)
 {
  *rval = INT_TO_JSVAL(v2);
  JS_SetProperty(cx, obj,"searchDirection",rval);
 }

 *rval = INT_TO_JSVAL(row);

 JS_SetProperty(cx, obj,"lastFind",rval);

 RETINT(row);
}

WRAP_HELP(DataTable,
 L"data(row,col/name) or get(row,col/name)\n"
 L"set(row,col/name,text)\nsetN(row,col/name,number)\n"
 L"save() or save(filename) memory and ASCII tables may be saved with a new name.\n"
 L"column(name)\n"
 L"title(col)\n"
 L"type(index/name,index/name,...) may return an array of values\n"
 L"title(index)\n"
 L"setTitle(index,name)\n"
 L"width(n)\n"
 L"addColumn(name)\n"
 L"deleteColumn(index)\n"
 L"getRow(index,[record])\n"
 L"setRow(index,record)\n"
 L"find(record,[start],[direction])\n"
 L"findNext(record)\n"
 L"add(record)\n"
 L"addRow(record)\n"
 L"delete(row) do not delete a row while looping over rowCount\n"
 L"toString(true) converts the table to tab-delimited text\n"
 L"getText(index) for mail databases only\n"
 L"getHTML(index) for mail databases only\n"
 L"index(fieldname,fieldname,...) returns an object with a find() function\n"
 L"help()\n")


WRAP(Table,DelRow)
{
 if (argc == 0) ERR_COUNT(Table,DelRow);
 GETOBJ(Table,DataTable,t);

 char c[2];
 c[1]=0;

 for (int i =0 ; i < argc; i++)
  {
   int32 row;
   if (!TOINT(i,row))
//   if (ISINT(i)) row = INT(i);
//   else
    ERR_TYPE(Table,delete,i,integer);

   if (!t->SetDataC(row,0,"Delete")) RETBOOL(false);
  }

 RETBOOL(true);
}

WRAP(Table,FieldType)
{
 if (argc == 0) ERR_COUNT(Table,FieldType);
 ENTERNATIVE(cx);
 GETOBJ(Table,DataTable,t);

 jsval * arr = new jsval[argc];

 char c[2];
 c[1]=0;

 for (int i =0 ; i < argc; i++)
  {
   int32 col;
   if (!TOINT(i,col)) //col = INT(i); else
     col = t->FindColumn(STR(i));
   c[0] = t->GetColStatus(col);
   if (!c[0]) c[0] = 'C';
   arr[i] = STRING_TO_JSVAL(ROOT(JS_NewStringCopyZ(cx,c)));
  }

 JSObject * ret = JS_NewArrayObject(cx,argc,arr);
 ROOT(ret);

 delete [] arr;
 RETOBJ(ret);
}

WRAP(Table,SetDataC)
{
 if (argc != 3) ERR_COUNT(Table,SetDataC);
 if (!ISINT(0)) ERR_TYPE(Table,set,1,integer);

 GETOBJ(Table,DataTable,t);

 int32 j = 0;
 if (ISINT(1)) j = INT(1);
 else if (ISSTR(1)) j = t->FindColumn(STR(1));
 if (!j) ERR_TYPE(Table,set,2,column id);

 GETUTF8(2);

 bool ret = t->SetDataC(INT(0),j,u2);

 RETBOOL(ret);
}

WRAP(Table,SetDataD)
{
 if (argc != 3) ERR_COUNT(Table,SetDataD);
 if (!ISINT(0)) ERR_TYPE(Table,SetDataD,1,Integer);
 jsdouble d;

 GETOBJ(Table,DataTable,t);

 int32 j = 0;
 if (ISINT(1)) j = INT(1);
 else if (ISSTR(1)) j = t->FindColumn(STR(1));
 if (!j) return JS_FALSE;

 if (!TODBL(2,d)) return JS_FALSE;

 bool ret = t->SetDataD(INT(0),j,d);

 RETBOOL(ret);
}

WRAP(Table,GetData)
{
 if (argc == 1)
 {
  int32 j = 0;
  TOINT(0,j);
  if (j == 0) RETOBJ(NULL);
  GETOBJ(Table,DataTable,t);
  TParameterList r1;
  if (!t->GetRow(j,r1))
    RETOBJ(NULL);
  JSObject * o = JS_NewObject(cx, NULL, NULL, NULL);
  ListToObject(cx,r1,o);
  RETOBJ(o);
 }

 if (argc != 2) ERR_COUNT(Table,GetDataC);
 if (!ISINT(0)) ERR_TYPE(Table,GetDataC,1,Integer);

 GETOBJ(Table,DataTable,t);

 int32 j = 0;
 if (ISINT(1)) j = INT(1);
 else if (ISSTR(1)) j = t->FindColumn(STR(1));
 if (!j) ERR_MSG(Table,"Column unavailable","");

 const char * c = t->GetDataC(INT(0),j);

 if (c) RETSTR(c);
 ERR_MSG(Table,"Data unavailable","");
}

WRAP(Table,GetMessage)
{
 GETOBJ(Table,DataTable,t);
 TMailList * m = TYPESAFE_DOWNCAST(t,TMailList);
 if (!m) RETSTRW(L"");
 if (!argc) ERR_COUNT(Table,GetMessage);
 if (!ISINT(0)) ERR_TYPE(Table,GetMessage,1,Number);

 MailMessage * g = (*m)[INT(0)-1];
 if (!g) RETSTRW(L"");
 RETSTR(g->MsgText());
}

WRAP(Table,GetMessage2)
{
 GETOBJ(Table,DataTable,t);
 TMailList * m = TYPESAFE_DOWNCAST(t,TMailList);
 if (!m) RETSTRW(L"");
 if (!argc) ERR_COUNT(Table,GetMessage);
 if (!ISINT(0)) ERR_TYPE(Table,GetMessage,1,Number);

 MailMessage * g = (*m)[INT(0)-1];
 if (!g) RETSTRW(L"");
 RETSTR(g->HTMLText());
}

WRAP(Table,Save)
{
 GETOBJ(Table,DataTable,t);
 Stream* f;
 SpreadsheetTable * s = TYPESAFE_DOWNCAST(t,SpreadsheetTable);
 JSBool titles = true;
 if (argc > 2) TOBOOL(2,titles);

 if (s && argc >0 )
 {
  GETFILE(0,f);
  GETUTF8(1);
  if (f)
  {
   try
   {
    //FileStream out(STR(0),Stream::OMText,Stream::IOWrite);
    //RETBOOL(s->SaveToStream(out,u1,titles));
    RETBOOL(s->SaveToStream(*f,u1,titles));
   } catch(...)
   {
    ERR_MSG(Table,"File write failure",STR(0));
   }
  }
  if (ISSTR(0))
  {
   try
   {
       GETUTF8(0);
       FileStream out(u0,Stream::OMText,Stream::IOWrite);
       RETBOOL(s->SaveToStream(out,u1,titles));
   } catch(...)
   {
    ERR_MSG(Table,"File write failure",STR(0));
   }
  }
 }
 RETBOOL(t->Save());
}

JSBool
Table_JSGet(JSContext *cx, JSObject *obj, jsval id, jsval *rval)
{
 const char * c = 0;
 count_t i = 0;

 GETOBJ(Table,DataTable,t);

 int x = JSVAL_TO_INT(id);

 if (JSVAL_IS_INT(id))
  switch (x)
  {
   case 0: c = t->Filename; break;
   case 1: i = t->RowCount(); break;
   case 2: i = t->ColumnCount(); break;
   case 3: RETSTRW(L"Table");
   case 4: RETSTR(t->GetLastError());
   case 5:
   {
#ifndef TBL_NO_SQL
     ODBCTable * o = TYPESAFE_DOWNCAST(t,ODBCTable);
     if (o)
     {
      RETSTR(o->DriverName());
     }
     else
#endif
     {
      RETSTRW(L"");
     }
   }
   default: return JS_FALSE;
  }
 else return JS_FALSE;

 if (c) RETSTR(c);
 RETINT(i);
}


WRAP(Table,ToString)
{
 GETOBJ(Table,DataTable,t);
 if (argc == 0) RETSTR(t->filename());

 MemoryStream out;

 for (count_t i = 0; i <= t->RowCount(); i++)
 {
  for (count_t j=1; j <= t->ColumnCount(); j++)
  {
   if (j > 1) out << "\t";
   out << t->GetDataC(i,j);
  }
  out << "\n";
 }
 RETSTR(out);
}

WRAP(Table,Index)
{
 if (argc == 0)
  ERR_COUNT(Table,Index);

 GETENV;
 if (!Env->oIndex)
  RETOBJ(0);

 GETOBJ(Table,DataTable,t);

 DataIndex*d;
 if (argc == 1 && ISINT(0))
 {
  d = new DataIndex(*t,INT(0));
 }
 else
 {
  TStringList list;
  for (int i=0; i< argc; i++)
  {
      GETUTF8(i);
      if (j8i) list.Add(ui);
   //JSString* j = JS_ValueToString(cx,argv[i]);
   //if (!j) continue;
   //list.Add(JS_GetStringBytes(j));
  }
  d = new DataIndex(*t,list);
 }
 d->BuildIndex(0,0);
 RETOBJ(Index_Object(cx,d,true,GETPOINTER));
}

WRAP(Table,Close)
{
 CLOSEPRIVATE(Table,DataTable);
 RETBOOL(true);
}

void Table_JSFinalize(JSContext *cx, JSObject *obj)
{
// if (JS_GetParent(cx,obj)) return;
 DELPRIVATE(DataTable);
// JS_SetPrivate(cx,obj,NULL);
}

JSBool
Table_Table(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
    jsval *rval)
{
 GETENV;
 if (!Env) return JS_FALSE;

 DataTable * dt = NULL;

 try {
 if (argc == 1 && ISSTR(0))
  dt = OpenTable(STR(0),Env->TableEnv);
 else if (argc == 0)
  dt = new SpreadsheetTable;
 } catch (...) {dt = NULL;}

 if (!dt)
 {
  if (Env->errorOnFailure)
    return JS_FALSE;
  ERR_MSG(Table,"Open table failed","");
 }

 if (dt)
 {
  SETPRIVATE(obj,DataTable,dt,true,NULL);
 }
// JS_SetPrivate(cx,obj,dt);

 return JS_TRUE;
}

static JSPropertySpec Table_properties[] = {
    {"name",      0,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"rowCount",  1,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"colCount",  2,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"count",  1,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"length",  1,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"className",3, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"error",4,JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {"driverName",5,JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT,Table_JSGet},
    {0}
};

static JSFunctionSpec Table_functions[] = {
    {"data",     Table_GetData,      2},
    {"get",     Table_GetData,      2},
    {"set",     Table_SetDataC,      3},
    {"setN",     Table_SetDataD,      3},
    {"save",    Table_Save,          0},
    {"type",    Table_FieldType,         1},
    {"column",  DataTable_FindColumn,    1},
//    {"error",  DataTable_GetLastError,    0},
    {"title",  DataTable_ColumnTitle,    1},
    {"setTitle",  DataTable_SetTitle,    2},
    {"width",  DataTable_GetColWidth,    1},
    {"deleteColumn",  DataTable_DelCol,    1},
    {"addColumn",  DataTable_AddCol,    1},
    {"add",Table_AddRow ,1},
    {"addRow",Table_AddRow ,1},
    {"del",Table_DelRow,1},
    {"deleteRow",Table_DelRow,1},
    {"delRow",Table_DelRow,1},
    {"find",Table_FindRow,1},
    {"index",Table_Index,10},
    {"findNext",Table_FindNext,1},
    {"getRow",Table_GetRow,1},
    {"setRow",DataTable_SetRow,2},
    {"toString",Table_ToString,0},
    {"close",Table_Close,0},
    {"getMessage",Table_GetMessage,1},
    {"getHTML",Table_GetMessage2,1},
    {"getWhere", Table_GetWhere, 1},
    {"append",Table_Append,3},
    {0}
};

static JSFunctionSpec Table_fnstatic[] = {
    {"create",Table_Create,2},
    {"help",  DataTable_HELP,    0},
    {0}
};

static JSClass Table_class = {
    "Table", JSCLASS_HAS_PRIVATE,         //Table_JSGet
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,Table_JSFinalize
};

JSObject*
Table_Object(JSContext *cx, DataTable* t,bool autodelete,JSPointerBase* Parent)
{
 JSObject* obj;
 GETENV;
 ENTERNATIVE(cx);
 MAKENEW(Table);
 SETPRIVATE(obj,DataTable,t,autodelete,Parent);

 return obj;
}

void Table_InitClass(JSContext *cx, JSObject *obj)
{
 GETENV;
 INITCLASS(Table);
}

JSClass* Table_Class() {return &Table_class;}

