// Do NOT change. Changes will be lost next time file is generated

#define R__DICTIONARY_FILENAME rootdict

/*******************************************************************/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define G__DICTIONARY
#include "RConfig.h"
#include "TClass.h"
#include "TDictAttributeMap.h"
#include "TInterpreter.h"
#include "TROOT.h"
#include "TBuffer.h"
#include "TMemberInspector.h"
#include "TInterpreter.h"
#include "TVirtualMutex.h"
#include "TError.h"

#ifndef G__ROOT
#define G__ROOT
#endif

#include "RtypesImp.h"
#include "TIsAProxy.h"
#include "TFileMergeInfo.h"
#include <algorithm>
#include "TCollectionProxyInfo.h"
/*******************************************************************/

#include "TDataMember.h"

// Since CINT ignores the std namespace, we need to do so in this file.
namespace std {} using namespace std;

// Header files passed as explicit arguments
#include "/home/jesse/Analysis/TimeTagger/V1190Readout/src/AddOn/TimeTaggerEvent.h"

// Header files passed via #pragma extra_include

namespace ROOT {
   static void *new_TimeTaggerEvent(void *p = 0);
   static void *newArray_TimeTaggerEvent(Long_t size, void *p);
   static void delete_TimeTaggerEvent(void *p);
   static void deleteArray_TimeTaggerEvent(void *p);
   static void destruct_TimeTaggerEvent(void *p);
   static void streamer_TimeTaggerEvent(TBuffer &buf, void *obj);

   // Function generating the singleton type initializer
   static TGenericClassInfo *GenerateInitInstanceLocal(const ::TimeTaggerEvent*)
   {
      ::TimeTaggerEvent *ptr = 0;
      static ::TVirtualIsAProxy* isa_proxy = new ::TInstrumentedIsAProxy< ::TimeTaggerEvent >(0);
      static ::ROOT::TGenericClassInfo 
         instance("TimeTaggerEvent", ::TimeTaggerEvent::Class_Version(), "TimeTaggerEvent.h", 20,
                  typeid(::TimeTaggerEvent), ::ROOT::Internal::DefineBehavior(ptr, ptr),
                  &::TimeTaggerEvent::Dictionary, isa_proxy, 16,
                  sizeof(::TimeTaggerEvent) );
      instance.SetNew(&new_TimeTaggerEvent);
      instance.SetNewArray(&newArray_TimeTaggerEvent);
      instance.SetDelete(&delete_TimeTaggerEvent);
      instance.SetDeleteArray(&deleteArray_TimeTaggerEvent);
      instance.SetDestructor(&destruct_TimeTaggerEvent);
      instance.SetStreamerFunc(&streamer_TimeTaggerEvent);
      return &instance;
   }
   TGenericClassInfo *GenerateInitInstance(const ::TimeTaggerEvent*)
   {
      return GenerateInitInstanceLocal((::TimeTaggerEvent*)0);
   }
   // Static variable to force the class initialization
   static ::ROOT::TGenericClassInfo *_R__UNIQUE_DICT_(Init) = GenerateInitInstanceLocal((const ::TimeTaggerEvent*)0x0); R__UseDummy(_R__UNIQUE_DICT_(Init));
} // end of namespace ROOT

//______________________________________________________________________________
atomic_TClass_ptr TimeTaggerEvent::fgIsA(0);  // static to hold class pointer

//______________________________________________________________________________
const char *TimeTaggerEvent::Class_Name()
{
   return "TimeTaggerEvent";
}

//______________________________________________________________________________
const char *TimeTaggerEvent::ImplFileName()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TimeTaggerEvent*)0x0)->GetImplFileName();
}

//______________________________________________________________________________
int TimeTaggerEvent::ImplFileLine()
{
   return ::ROOT::GenerateInitInstanceLocal((const ::TimeTaggerEvent*)0x0)->GetImplFileLine();
}

//______________________________________________________________________________
TClass *TimeTaggerEvent::Dictionary()
{
   fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TimeTaggerEvent*)0x0)->GetClass();
   return fgIsA;
}

//______________________________________________________________________________
TClass *TimeTaggerEvent::Class()
{
   if (!fgIsA.load()) { R__LOCKGUARD2(gInterpreterMutex); fgIsA = ::ROOT::GenerateInitInstanceLocal((const ::TimeTaggerEvent*)0x0)->GetClass(); }
   return fgIsA;
}

//______________________________________________________________________________
void TimeTaggerEvent::Streamer(TBuffer &R__b)
{
   // Stream an object of class TimeTaggerEvent.

   UInt_t R__s, R__c;
   if (R__b.IsReading()) {
      Version_t R__v = R__b.ReadVersion(&R__s, &R__c); if (R__v) { }
      TObject::Streamer(R__b);
      R__b >> mEvNum;
      R__b >> mGulpNum;
      R__b >> mContainsErr;
      int R__i;
      for (R__i = 0; R__i < 2; R__i++)
         R__b >> mTL[R__i];
      R__b.CheckByteCount(R__s, R__c, TimeTaggerEvent::IsA());
   } else {
      R__c = R__b.WriteVersion(TimeTaggerEvent::IsA(), kTRUE);
      TObject::Streamer(R__b);
      R__b << mEvNum;
      R__b << mGulpNum;
      R__b << mContainsErr;
      int R__i;
      for (R__i = 0; R__i < 2; R__i++)
         R__b << mTL[R__i];
      R__b.SetByteCount(R__c, kTRUE);
   }
}

namespace ROOT {
   // Wrappers around operator new
   static void *new_TimeTaggerEvent(void *p) {
      return  p ? new(p) ::TimeTaggerEvent : new ::TimeTaggerEvent;
   }
   static void *newArray_TimeTaggerEvent(Long_t nElements, void *p) {
      return p ? new(p) ::TimeTaggerEvent[nElements] : new ::TimeTaggerEvent[nElements];
   }
   // Wrapper around operator delete
   static void delete_TimeTaggerEvent(void *p) {
      delete ((::TimeTaggerEvent*)p);
   }
   static void deleteArray_TimeTaggerEvent(void *p) {
      delete [] ((::TimeTaggerEvent*)p);
   }
   static void destruct_TimeTaggerEvent(void *p) {
      typedef ::TimeTaggerEvent current_t;
      ((current_t*)p)->~current_t();
   }
   // Wrapper around a custom streamer member function.
   static void streamer_TimeTaggerEvent(TBuffer &buf, void *obj) {
      ((::TimeTaggerEvent*)obj)->::TimeTaggerEvent::Streamer(buf);
   }
} // end of namespace ROOT for class ::TimeTaggerEvent

namespace {
  void TriggerDictionaryInitialization_rootdict_Impl() {
    static const char* headers[] = {
"/home/jesse/Analysis/TimeTagger/V1190Readout/src/AddOn/TimeTaggerEvent.h",
0
    };
    static const char* includePaths[] = {
"/home/jesse/alice/sw/ubuntu1604_x86-64/ROOT/v6-10-08-7/include",
"/home/jesse/Analysis/TimeTagger/V1190Readout/src/AddOn",
"/HDDhome/alice/sw/ubuntu1604_x86-64/ROOT/v6-10-08-7/include",
"/HDDhome/Analysis/TimeTagger/V1190Readout/",
0
    };
    static const char* fwdDeclCode = R"DICTFWDDCLS(
#line 1 "rootdict dictionary forward declarations' payload"
#pragma clang diagnostic ignored "-Wkeyword-compat"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
extern int __Cling_Autoloading_Map;
class __attribute__((annotate(R"ATTRDUMP(this way, if I include more detectors, it is recorded)ATTRDUMP"))) __attribute__((annotate("$clingAutoload$/home/jesse/Analysis/TimeTagger/V1190Readout/src/AddOn/TimeTaggerEvent.h")))  TimeTaggerEvent;
)DICTFWDDCLS";
    static const char* payloadCode = R"DICTPAYLOAD(
#line 1 "rootdict dictionary payload"

#ifndef G__VECTOR_HAS_CLASS_ITERATOR
  #define G__VECTOR_HAS_CLASS_ITERATOR 1
#endif

#define _BACKWARD_BACKWARD_WARNING_H
#include "/home/jesse/Analysis/TimeTagger/V1190Readout/src/AddOn/TimeTaggerEvent.h"

#undef  _BACKWARD_BACKWARD_WARNING_H
)DICTPAYLOAD";
    static const char* classesHeaders[]={
"TimeTaggerEvent", payloadCode, "@",
nullptr};

    static bool isInitialized = false;
    if (!isInitialized) {
      TROOT::RegisterModule("rootdict",
        headers, includePaths, payloadCode, fwdDeclCode,
        TriggerDictionaryInitialization_rootdict_Impl, {}, classesHeaders);
      isInitialized = true;
    }
  }
  static struct DictInit {
    DictInit() {
      TriggerDictionaryInitialization_rootdict_Impl();
    }
  } __TheDictionaryInitializer;
}
void TriggerDictionaryInitialization_rootdict() {
  TriggerDictionaryInitialization_rootdict_Impl();
}
