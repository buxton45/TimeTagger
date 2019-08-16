#include "TimeTaggerEvent.h"

ClassImp(TimeTaggerEvent)


TimeTaggerEvent::TimeTaggerEvent(int aGulpNumber, int EventNumber){
  mGulpNum = aGulpNumber;
  mEvNum = EventNumber;
  mContainsErr = false;
  for (int i=0; i<_Ndetectors; i++) mTL[i]= new TArrayF;
}

TimeTaggerEvent::~TimeTaggerEvent(){
  for (int i=0; i<_Ndetectors; i++) delete mTL[i];
}

void TimeTaggerEvent::SetTimeList(int i, int n, const Float_t* array){
  if (i>=0 && i<_Ndetectors) mTL[i]->Set(n,array);
}

void TimeTaggerEvent::Reset(){
  for (int i=0; i<_Ndetectors; i++){
    mTL[i]->Reset(); 
    mTL[i]->Set(0);
  }
  mGulpNum=0;
  mEvNum=0;
  mContainsErr = false;
}
