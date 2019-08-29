R__LOAD_LIBRARY(TimeTaggerEvent_cxx.so)

#include <iostream>
#include <bitset>
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

 // gROOT->ProcessLine(".L TimeTaggerEvent.cxx+");

  vector<int> tDetectors{1, 8};

  float tPrecision = 25e-12;
  TH1D* autoCF[tDetectors.size()];
  for (int idet=0; idet<tDetectors.size(); idet++){
    autoCF[idet] = new TH1D(Form("autoCF%d",tDetectors[idet]),Form("autoCF%d",tDetectors[idet]), 12500, -2500000*tPrecision, 2500000*tPrecision);
  }
  TH1D* corr = new TH1D(TString::Format("CorrFctn_%d%d", tDetectors[0], tDetectors[1]),
                        TString::Format("CorrFctn_%d%d", tDetectors[0], tDetectors[1]), 
                        12500, -2500000*tPrecision, 2500000*tPrecision);

//  TFile* theFile = new TFile("/home/jesse/Analysis/TimeTagger/Data/tTreeFile_20190816_1356.root","READ");
  TFile* theFile = new TFile("/home/jesse/Analysis/TimeTagger/Data/Run1908291443.root","READ");
  TTree* tr = (TTree*)theFile->Get("tTTETree"); 
  TTree* tInfoTree = (TTree*)theFile->Get("tInfoTree");

  TObjString* tCommentObjString = new TObjString();
  TBranch* tCommentBr = tInfoTree->GetBranch("Comment");
  tCommentBr->SetAddress(&tCommentObjString);

  UInt_t tChMask;
  TBranch* tChMaskBr = tInfoTree->GetBranch("ChannelMask");
  tChMaskBr->SetAddress(&tChMask);

  tInfoTree->GetEvent(0);
  cout << "tCommentObjString->GetString() = " << tCommentObjString->GetString() << endl << endl;
  cout << "tChMask = " << tChMask << endl;
  std::string tChMask_binary = std::bitset<16>(tChMask).to_string();
  cout << "tChMask_binary = " << tChMask_binary << endl;

  TimeTaggerEvent* tte=new TimeTaggerEvent;
  TBranch* tBr = tr->GetBranch("TimeBranch");
  tBr->SetAddress(&tte);

  int nev = tr->GetEntries();
  cout << "There are " << nev << " events in the tree\n";

  for (int i=0; i<nev; i++){
    tr->GetEvent(i);
    cout << "Event " << i
	 << " : detector " << tDetectors[0] << " has " << tte->TimeList(tDetectors[0])->GetSize()
	 << " | detector " << tDetectors[1] << " has " << tte->TimeList(tDetectors[1])->GetSize()
	 << endl;
    // auto correlations...
    for (int idet=0; idet<tDetectors.size(); idet++){
      float* times = tte->TimeList(tDetectors[idet])->fArray;
      for (int t1=0; t1<tte->TimeList(tDetectors[idet])->GetSize()-1; t1++){
	for (int t2=t1+1; t2<tte->TimeList(tDetectors[idet])->GetSize(); t2++){
	  autoCF[idet]->Fill(times[t1]-times[t2]);
	}
      }
    }
    // 2-detector correlation function
    float* times1 = tte->TimeList(tDetectors[0])->fArray;
    float* times2 = tte->TimeList(tDetectors[1])->fArray;
    for (int t1=0; t1<tte->TimeList(tDetectors[0])->GetSize(); t1++){
      for (int t2=0; t2<tte->TimeList(tDetectors[1])->GetSize(); t2++){
	corr->Fill(times1[t1]-times2[t2]);
      }
    }
  }
  theFile->Close();
  cout << "DONE" << endl;
  return 0;
}
