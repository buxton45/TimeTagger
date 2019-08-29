#ifndef _TimeTaggerEvent
#define _TimeTaggerEvent

/***********************************************
 * TimeTaggerEvent - Mike Lisa 13aug2019
 * Very simple class that stores list of times
 * from the time-tagger as TArrayF objects
 * 
 * Important: Unit of time is SECONDS
 ***********************************************/


#include "TObject.h"
#include "TArrayF.h"
//#include <stdio.h>
//#include <iostream>

#define _Ndetectors 16

class TimeTaggerEvent : public TObject {
 public:
  TimeTaggerEvent(int aGulpNumber=0, int EventNumber=0);
  ~TimeTaggerEvent();

  TArrayF* TimeList(int i){return ((i>=0)&&(i<_Ndetectors))?mTL[i]:0;}
  int EventNumber(){return mEvNum;}
  int GulpNumber(){return mGulpNum;}
  bool ContainsError(){return mContainsErr;}

  void SetTimeList(int i, int n, const Float_t* array);  // i=detector#, n=#time values, array=times
  void SetEventNumber(int i){mEvNum=i;}
  void SetGulpNumber(int i){mGulpNum=i;}  
  void SetContainsError(bool aErr){mContainsErr=aErr;}

  void Reset();   // probably useless

 private:
  int mEvNum;
  int mGulpNum;
  bool mContainsErr;
  TArrayF* mTL[_Ndetectors];
  
  ClassDef(TimeTaggerEvent,_Ndetectors)  // this way, if I include more detectors, it is recorded

};



#endif
