R__LOAD_LIBRARY(TimeTaggerEvent_cxx.so)

#include <iostream>
#include <vector>
using std::cout;
using std::endl;
using std::vector;

#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"

#include "TimeTaggerEvent.h"

void AnalyzeCorrelations(){
//int main(int argc, char **argv){

  float tPrecision = 25e-12;
  TH1D* autoCF[2];
  for (int idet=0; idet<2; idet++){
    autoCF[idet] = new TH1D(Form("autoCF%d",idet+1),Form("autoCF%d",idet+1), 2000, -2500000*tPrecision, 2500000*tPrecision);
  }
  TH1D* corr = new TH1D("CorrFctn","CorrFctn", 2000, -2500000*tPrecision, 2500000*tPrecision);

  TFile* theFile = new TFile("/home/jesse/Analysis/TimeTagger/V1190Readout/tTreeFile.root","READ");
  TTree* tr = (TTree*)theFile->Get("tTTETree"); 

  TimeTaggerEvent* tte=new TimeTaggerEvent;
  TBranch* tBr = tr->GetBranch("TimeBranch");
  tBr->SetAddress(&tte);

  int nev = tr->GetEntries();
  cout << "There are " << nev << " events in the tree\n";

  for (int i=0; i<nev; i++){
    tr->GetEvent(i);
    cout << "Event " << i
	 << " : detector 1 has " << tte->TimeList(0)->GetSize()
	 << " | detector 2 has " << tte->TimeList(1)->GetSize()
	 << endl;
    // auto correlations...
    for (int idet=0; idet<2; idet++){
      float* times = tte->TimeList(idet)->fArray;
      for (int t1=0; t1<tte->TimeList(idet)->GetSize()-1; t1++){
	for (int t2=t1+1; t2<tte->TimeList(idet)->GetSize(); t2++){
	  autoCF[idet]->Fill(tPrecision*(times[t1]-times[t2]));
	}
      }
    }
    // 2-detector correlation function
    float* times1 = tte->TimeList(0)->fArray;
    float* times2 = tte->TimeList(1)->fArray;
    for (int t1=0; t1<tte->TimeList(0)->GetSize(); t1++){
      for (int t2=0; t2<tte->TimeList(1)->GetSize(); t2++){
	corr->Fill(tPrecision*(times1[t1]-times2[t2]));
      }
    }
  }
  theFile->Close();
  cout << "DONE" << endl;
  return 0;
}
