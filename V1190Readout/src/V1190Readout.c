/******************************************************************************

  CAEN SpA - Viareggio
  www.caen.it

  Program: V1190/V1290 Readout Program
 
  ------------------------------------------------------------------------

  Description:
  -----------------------------------------------------------------------------
  This program reads CAEN's MutiHit TDC boards (both V1190 and V1290)
  and collects the data into a memory buffer allocated for this purpose.
  The configuration of the TDC boards (registers setting) is done by means of
  a configuration file that contains a list of generic write accesses to the
  registers of the board, incuding opcodes for the micro controller.
  The configuartion file contains also some other parameters (declared with
  specific keywords) that define the operating mode of the Readout program.
  This program is based on the CAENVMELib library for the VME access functions
  and it can work with the V1718 and V2718 bridges. If you have a different VME
  controller, you must change the CAENVME functions for the VME access or
  provide a wrapper library. The following CAENVME functions are used by this
  program:
     .) CAENVME_Init()
     .) CAENVME_ReadCycle()
     .) CAENVME_WriteCycle()
     .) CAENVME_FIFOBLTReadCycle()
     .) CAENVME_End()


  -----------------------------------------------------------------------------
  Syntax: V1190Readout [ConfigFile]
  Default config file is "V1190Config.txt"

  -----------------------------------------------------------------------------
  Keyword list and syntax for the configuration file:

  LINK  type  bdnum  linknum
    Description: define the VME controller
    Parameters:  type = V1728 or V2718

  TIME_REF ch
    Description: Define the time reference, i.e. the channel respect to which 
                 all time measurement are calculated (Time[i] = T[i] - Tref)
    Parameters:  ch is the channel for which the histogram is calculated

  WRITE_REGISTER A D
    Description: write a generic register
    Parameters:  A is the register offset (16 bit hex number). The actual VME
                 address will be BASE_ADDRESS+A
                 D is the data to write (16 bit hex number)

  WRITE_OPCODE n OP D1 D2 ...
    Description: write an opcode (and the parameter) to the MicroController
    Parameters:  n is the number of words to write (including the opcode)
                 OP is the opcode (16 bit hex number)
                 D1, D2 are the parameters (16 bit hex number)


******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef WIN32
    #include <time.h>
    #include <sys/timeb.h>
	#include <process.h>
    #include <conio.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
	#include <ctype.h>    /* toupper() */
	#define Sleep(x) usleep((x)*1000)
	#define _popen(x,y) popen(x,y)
	#define _pclose(x) pclose(x)
#endif

#include "keyb.h"
#include "CAENVMElib.h"
#include "V1190.h"

#include "AddOn/TimeTaggerEvent.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
#include "TObjArray.h"
#include "TCanvas.h"
#include "TApplication.h"
#include <assert.h>
#include <vector>
#include <ctime>
#include <iostream>
#include <string>
using std::vector;
using std::cout;
using std::endl;




//***************************************************************************
#define SWRELEASE     "1.1"
//***************************************************************************

#define NUM_CHANNELS   32
#define OUTFILE_NAME   "V1190EventList.txt"

// ###########################################################################
// Global Variables
// ###########################################################################
int handle=-1;
unsigned int BaseAddress = 0xEE000000;
int VMEerror = 0;
FILE *gnuplot=NULL;
int PlotError=0;
int RawData=0;
char path[128];

// ###########################################################################
// Functions
// ###########################################################################

// ---------------------------------------------------------------------------
// get time in milliseconds
// ---------------------------------------------------------------------------
int get_time()
{
int time_ms;

#ifdef WIN32
    struct _timeb timebuffer;

    _ftime( &timebuffer );
    time_ms = (int)timebuffer.time * 1000 + (int)timebuffer.millitm;
#else
    struct timeval t1;
    struct timezone tz;

    gettimeofday(&t1, &tz);
    time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
#endif

    return time_ms;
}

// ---------------------------------------------------------------------------
// Read a 16 bit register of the V1190
// ---------------------------------------------------------------------------
unsigned short V1190ReadRegister(unsigned short RegAddr)
{
	unsigned short reg;
	VMEerror |= CAENVME_ReadCycle(handle, BaseAddress + RegAddr, &reg, cvA32_U_DATA, cvD16);
	return reg;
}

// ---------------------------------------------------------------------------
// Write a 16 bit register of the V1190
// ---------------------------------------------------------------------------
void V1190WriteRegister(unsigned short RegAddr, unsigned short RegData)
{
	unsigned short reg = RegData;
	VMEerror |= CAENVME_WriteCycle(handle, BaseAddress + RegAddr, &reg, cvA32_U_DATA, cvD16);
}

// ---------------------------------------------------------------------------
// Write Opcode
// ---------------------------------------------------------------------------
int V1190WriteOpcode(int nw, unsigned short *Data)
{
    int i, timeout=0;
	unsigned short hs;
    
    for(i=0; i<nw; i++) {        
        do {
            hs = V1190ReadRegister(PROG_HS);
            timeout++;
            Sleep(1);
        }
        while (!VMEerror && ((hs & 0x01)==0) && (timeout < 3000)); /* wait to write */
        if ((timeout == 3000) || VMEerror)
            return 1;
        V1190WriteRegister(OPCODE, Data[i]);
        if (VMEerror) {
            return 1;
		}
    }
	return 0;
}


// ---------------------------------------------------------------------------
// Save the histograms to output files
// ---------------------------------------------------------------------------
void SaveHistograms(unsigned int *Histo[NUM_CHANNELS], int HistoNbin, int ChMask)
{
    FILE *fh;
    int j,i;
    char fname[100];
    for(i=0; i<NUM_CHANNELS; i++) {
		if ((1<<i) & ChMask) {
			sprintf(fname, "%s\\Histo_%d.txt", path, i);
			fh=fopen(fname, "w");
			if(fh==NULL) break;
			for(j=0; j<HistoNbin; j++)
				fprintf(fh, "%d\n", Histo[i][j]);
			fclose(fh);
		}
    }
    printf("Histogram Files saved\n");
}


// ---------------------------------------------------------------------------
// Plot the histograms 
// ---------------------------------------------------------------------------
void PlotHistograms(unsigned int *Histo[NUM_CHANNELS], int HistoNbin, int ChToPlot)
{
    FILE *fh;
    int j;
    
    if (PlotError)
        return;
    if (gnuplot==NULL)
        gnuplot = _popen("pgnuplot.exe", "w");
    if (gnuplot==NULL) {
        PlotError = 1;
        return;
    }
    fh=fopen("plotdata.txt", "w");
    for(j=0; j<HistoNbin; j++)
        fprintf(fh, "%d\n", Histo[ChToPlot][j]);
    fclose(fh);
    fprintf(gnuplot, "set title 'Channel %d\n", ChToPlot);
    fprintf(gnuplot, "plot 'plotdata.txt' with step\n");
	fflush(gnuplot);
}

// ---------------------------------------------------------------------------
void FillAutoCF(TH1D* aAutoCF, vector<float> &aData, TH1D* aTimes)
{
  for(unsigned int i=0; i<aData.size(); i++)
  {
    aTimes->Fill(aData[i]);
    for(unsigned int j=i+1; j<aData.size(); j++)
    {
      aAutoCF->Fill(aData[j]-aData[i]);
    }
  }
}

// ---------------------------------------------------------------------------
void FillCF(TH1D* aCF, vector<float> &aData1, vector<float> &aData2)
{
  for(unsigned int i1=0; i1<aData1.size(); i1++)
  {
    for(unsigned int i2=0; i2<aData2.size(); i2++)
    {
      aCF->Fill(aData2[i2]-aData1[i1]);
    }
  }
}

// ---------------------------------------------------------------------------
void ExtractEventsAndLoadTree(const double aRes, int aGulp, int aNb, unsigned int *aBuff, TimeTaggerEvent* aTTE, TTree* aTTETree, TObjArray* aRealTimeCFs, TObjArray* aRealTimeCanvases, TH1D* aTimes1, TH1D* aTimes2, TCanvas* aGCan, TH1D* aPhaseSpace, unsigned int aChRealTime1=1, unsigned int aChRealTime2=8)
{
  int tNChannels=16;
  vector<vector<float> > tData(tNChannels, vector<float>(0));
  for(unsigned int iCh=0; iCh<tData.size(); iCh++) tData[iCh].reserve(aNb);
  int tEventNumber = -1;
  
  for(int i=0; i<aNb/4; i++)
  {
    if(IS_GLOBAL_HEADER(aBuff[i]))
    {
      for(unsigned int iCh=0; iCh<tData.size(); iCh++) assert(tData[iCh].size()==0);  //TODO not vital, just safeguard         
      tEventNumber = DATA_EVENT_COUNTER(aBuff[i]);
      aTTE->SetEventNumber(DATA_EVENT_COUNTER(aBuff[i])); 
      aTTE->SetGulpNumber(aGulp);
    }
    else if(IS_GLOBAL_TRAILER(aBuff[i]))
    {
      if(((aBuff[i]>>24) & 0x7) != 0) aTTE->SetContainsError(true);
      for(unsigned int iCh=0; iCh<tData.size(); iCh++) aTTE->SetTimeList(iCh, tData[iCh].size(), tData[iCh].data());
      if(!aTTE->ContainsError()) aTTETree->Fill();
      //-------------------------------------------
      //Fill real-time histograms
      FillAutoCF((TH1D*)aRealTimeCFs->At(0), tData[aChRealTime1], aTimes1);
      FillAutoCF((TH1D*)aRealTimeCFs->At(1), tData[aChRealTime2], aTimes2);
      FillCF((TH1D*)aRealTimeCFs->At(2), tData[aChRealTime1], tData[aChRealTime2]);  
      
      //Draw real-time histograms if tEventNumber%1000==0 
      if(tEventNumber%1000==0 && tEventNumber>0)
      {
        for(int i=0; i<aRealTimeCanvases->GetEntries(); i++)
        {
          TH1D* tNorm = (TH1D*)((TH1D*)aRealTimeCFs->At(i))->Clone(TString::Format("tNorm_%d", i));
          tNorm->Divide(aPhaseSpace);

          ((TCanvas*)aRealTimeCanvases->At(i))->cd(1);
          tNorm->DrawCopy();
          tNorm->GetXaxis()->SetRangeUser(-0.000005, 0.000005);
          ((TCanvas*)aRealTimeCanvases->At(i))->cd(2);
          tNorm->DrawCopy();
          ((TCanvas*)aRealTimeCanvases->At(i))->Update();
          tNorm->Delete();
        }     
        aGCan->cd(1);
        aTimes1->DrawCopy();
        aGCan->cd(2);
        aTimes2->DrawCopy();     
        aGCan->Update();    
      }
      
      for(unsigned int iCh=0; iCh<tData.size(); iCh++) tData[iCh].clear();
      aTTE->Reset();
    }    
    else if(IS_TDC_DATA(aBuff[i]))
    {
      int tChannel = DATA_CH_25(aBuff[i]);
      assert(tChannel>=0 && tChannel<16);  //TODO not vital, just safeguard
      tData[tChannel].push_back(aRes*DATA_MEAS_25(aBuff[i]));  
    }
    else if(IS_TDC_ERROR(aBuff[i])) aTTE->SetContainsError(true);
    else continue;
  }


}

// ---------------------------------------------------------------------------
TObjString* GetComments()
{
  std::string line;
  TString tString;
  std::string endString ("end");

  cout << "Insert comments, return character is fine, " << 
          "enter 'end', or enter empty comment, when finished" << endl;

  while (std::getline(std::cin, line))
  {
    if(!line.compare("end") || line.length()==0) break;
    tString.Append(TString::Format("%s\n", line.c_str()));
  }
  TObjString* tReturn = new TObjString(tString);
  return tReturn;
}

// ###########################################################################
// MAIN
// ###########################################################################
int main(int argc, char *argv[])
{
    TApplication theApp("App", &argc, argv);

    TObjString* tCommentString = GetComments();

    unsigned short fwrev, ctrl, ErrorFlags=0, opcd[10];
	int sn, BoardType;
    int TDCerrors=0, Ovf=0, TrgLost=0;
    int Write2File=1, HistoNbin=12, HistoBinSize=1, TMWwidth=40, TMWoffs=-20;
	int Quit=0, BufferSize, i, WordPnt, totnb=0, ChFound[NUM_CHANNELS], TimeAbs[NUM_CHANNELS];
	unsigned int *Histo[NUM_CHANNELS], ChToPlot=1, ChMask;
	int ChTref=0;
    unsigned int *buff;
    int CurrentTime, PreviousTime, ElapsedTime;
	char tmpConfigFileName[100] = "V1190Config.txt";	// configuration file name
	char ConfigFileName[255] = "V1190Config.txt";	// configuration file name
    char tmp[100], str[100];
    int ch, Rpnt, nb, ret, ErrorLevel=-1, time, Header=1;
	int timerel;
	int r;
	
	//----------------------
	int tNGulps=0;
	int tNErrs=0;

	
	char tSaveDirBase[1000];
  TFile* tTreeFile;
  TTree* tTTETree;
  TTree* tInfoTree;
  TimeTaggerEvent* tTTE;
  
	
	unsigned int EnSubTrg, SetRes;
	double tRes;
	
  TObjArray* tRealTimeCFs = new TObjArray();
  TObjArray* tRealTimeCanvases = new TObjArray();

  TH1D* tTimes1 = new TH1D("tTimes1", "tTimes1", 2000, 0., 0.000055);
  TH1D* tTimes2 = new TH1D("tTimes2", "tTimes2", 2000, 0., 0.000055);
  TCanvas* tGCan = new TCanvas("tGCan", "tGCan");
  tGCan->Divide(2,1);

  TH1D* tPhaseSpace;
	//----------------------

    //int Tref;
    double TPrate, TRGrate;
    FILE *f_ini, *fout, *fr;
    unsigned long long TrgCnt=0, PrevTrgCnt=0, DiscardCnt[NUM_CHANNELS], HitCnt[NUM_CHANNELS],NegCnt[NUM_CHANNELS];
	double mean[128], stddev[128];
	int nstat[128];

    printf("\n");
    printf("**************************************************************\n");
    printf("                      V1190Readout %s                         \n", SWRELEASE);
    printf("**************************************************************\n");

    buff = NULL;
    for(i=0; i<NUM_CHANNELS; i++)
        Histo[i] = NULL;

    // ************************************************************************
    // Read configuration file
    // ************************************************************************

	if (argc > 1)
                strcpy(tmpConfigFileName, argv[1]);

#ifdef  WIN32
	sprintf(path, "%s\\VME\\", getenv("USERPROFILE"));
	_mkdir(path);
	sprintf(ConfigFileName, "%s\\%s", path, tmpConfigFileName);
#else
	sprintf(path, ".");
	sprintf(ConfigFileName, "%s/%s", path, tmpConfigFileName);
#endif

    if ( (f_ini = fopen(ConfigFileName, "r")) == NULL ) {
        printf("Can't open Configuration File %s\n", ConfigFileName);
        goto exit_prog;
    }

	    
    printf("Reading Configuration File %s\n", ConfigFileName);
    while(!feof(f_ini)) {
		int data, addr;
		int nwopc, val;
		unsigned short opcd[10];
		int app;
		
        str[0] = '#';
        fscanf(f_ini, "%s", str);
        if (str[0] == '#')
            fgets(str, 1000, f_ini);
        else {
            // LINK: Open VME master
            if (strstr(str, "LINK")!=NULL) {
			    CVBoardTypes BType;
				int link=0, bdnum=0;
				
                fscanf(f_ini, "%s", tmp);
                if (strstr(tmp, "V1718")!=NULL)
                    BType = cvV1718;
                else if (strstr(tmp, "V2718")!=NULL)
                    BType = cvV2718;
                else {
                    printf("Invalid VME Bridge Type\n");
                    goto exit_prog;
                }
                fscanf(f_ini, "%d", &link);
                if (BType == cvV2718)
                    fscanf(f_ini, "%d", &bdnum);
                if (CAENVME_Init(BType, link, bdnum, &handle) != cvSuccess) {
                    printf("VME Bridge init failure\n");
                    goto exit_prog;
                }
            }

            // Base Address
            if (strstr(str, "BASE_ADDRESS")!=NULL)
                fscanf(f_ini, "%x", (int *)&BaseAddress);

            // Raw Data
            if (strstr(str, "RAW_DATA")!=NULL)
                fscanf(f_ini, "%d", (int *)&RawData);
				
            // ChannelMask
            if (strstr(str, "CHANNEL_MASK")!=NULL)
                fscanf(f_ini, "%x", (int *)&ChMask);

            // Time Reference
            if (strstr(str, "TIME_REF")!=NULL)
                fscanf(f_ini, "%d", &ChTref);

            // Trigger Matching Window
            if (strstr(str, "TRIGGER_WINDOW")!=NULL) {
                fscanf(f_ini, "%d", &val);
                TMWwidth = (unsigned short)val;
                fscanf(f_ini, "%d", &val);
                TMWoffs = (unsigned short)val;
            }

            // Histo Num Bit
            if (strstr(str, "HISTO_CHANNELS")!=NULL) {
                fscanf(f_ini, "%d", &HistoNbin);
                fscanf(f_ini, "%d", &HistoBinSize);
            }
                
            // Write File
            if (strstr(str, "WRITE_EVENT_FILE")!=NULL)
                fscanf(f_ini, "%d", &Write2File);

            // Generic VME Write
            if (strstr(str, "WRITE_REGISTER")!=NULL) {
                fscanf(f_ini, "%x", &addr);
                fscanf(f_ini, "%x", &data);
				V1190WriteRegister((unsigned short)addr, (unsigned short)data);
                if (VMEerror) {
                    printf("VME Write failure at address %08X\n", BaseAddress + addr);
                    goto exit_prog;
                }
            }

            // Write Opcode
            if (strstr(str, "WRITE_OPCODE")!=NULL) {
                fscanf(f_ini, "%d", &nwopc);
                for(i=0; i<nwopc; i++) {
                    //fscanf(f_ini, "%x", (int *)&opcd[i]);
					fscanf(f_ini, "%x", &app);
					opcd[i] = (unsigned short) app;
                }
                if(V1190WriteOpcode(nwopc, opcd)) {
                    printf("Opcode Write failure (nwopc=%d, opcd=%x)\n", nwopc, opcd[0]);
                    goto exit_prog;
                }
                    
            }
            
            // Trigger subtraction
            if (strstr(str, "EN_SUB_TRG")!=NULL)
                fscanf(f_ini, "%d", &EnSubTrg);
                
            // Resolution setting
            if (strstr(str, "SET_RES")!=NULL)
                fscanf(f_ini, "%d", &SetRes);                
             
            // Save directory base
            if (strstr(str, "DATA_SAVE_BASE")!=NULL)
                fscanf(f_ini, "%s", tSaveDirBase);                               
        }
    }
    fclose (f_ini);


	if (RawData == 1){
		fr = fopen("Raw_Data.txt", "w");
		
		time_t now = std::time(0);
		tm *ltm = std::localtime(&now);
		TString tTreeFileName = TString::Format("%stTreeFile_%04d%02d%02d_%02d%02d.root",
		                                         tSaveDirBase, 
		                                         ltm->tm_year+1900, ltm->tm_mon+1, ltm->tm_mday,
		                                         ltm->tm_hour, ltm->tm_min);
		tTreeFile = new TFile(tTreeFileName, "RECREATE");	

	  tTTE = new TimeTaggerEvent();

	  tTTETree = new TTree("tTTETree", "tTTETree");
	  tTTETree->Branch("TimeBranch", &tTTE);
		
		
	}


    // ************************************************************************
    // Initialize the board and the variables for the acquisition
    // ************************************************************************
    // Read Board Type, Firmware Revisions and Serial Number
	fwrev = V1190ReadRegister(FW_REVISION);
	sn = (int)V1190ReadRegister(CR_SERNUM0);
	sn |= (int)V1190ReadRegister(CR_SERNUM1) << 8;
    BoardType = (int)V1190ReadRegister(CR_BOARDID0);
    BoardType |= (int)V1190ReadRegister(CR_BOARDID1) << 8;
    BoardType |= (int)V1190ReadRegister(CR_BOARDID2) << 16;
	if (VMEerror) {
		printf("Can't read the configuration ROM\n");
		goto exit_prog;
	}
	printf("Board Type: V%d: SerNum = %d, Fw Revision = %d.%d\n", BoardType, sn, (fwrev >> 8) & 0xFF, fwrev & 0xFF);
    
    // Write Control Register1 (enable BERR and Align64)
    ctrl = V1190ReadRegister(CONTROL);
    V1190WriteRegister(CONTROL, ctrl | 0x11);

    V1190WriteRegister(BLT_EVNUM, 0xFF);
    

    opcd[0]=0x0000; 
    V1190WriteOpcode(1, opcd);  // Enable Trigger Matching

//   opcd[0]=0x3100; 
//    V1190WriteOpcode(1, opcd);  // Disable TDC Header/Trailer
    
    opcd[0]=0x1000; opcd[1]=TMWwidth; 
    V1190WriteOpcode(2, opcd);  // Set Trigger Matching Window Width

    opcd[0]=0x1100; opcd[1]=TMWoffs; 
    V1190WriteOpcode(2, opcd);  // Set Trigger Matching Window Offset
    
    opcd[0]=0x4400; 
	opcd[1]=(unsigned short)(ChMask & 0xFFFF); 
	opcd[2]=(unsigned short)((ChMask>>16) & 0xFFFF); 
    opcd[3]=0x0000;
	opcd[4]=0x0000;
	opcd[5]=0x0000;
	opcd[6]=0x0000;
	opcd[7]=0x0000;
	opcd[8]=0x0000;

    if (BoardType == 1190)
		V1190WriteOpcode(9, opcd);  // Enable Channels
	else
		V1190WriteOpcode(3, opcd);  // Enable Channels


  //--------Set resolution
  opcd[0]=0x2400;
  opcd[1]=SetRes;
  V1190WriteOpcode(2, opcd);

  //--------Read resolution to be sure
  opcd[0]=0x2600;
  V1190WriteOpcode(1, opcd);
  unsigned int tReturn;
  tReturn = V1190ReadRegister(OPCODE);
  assert(tReturn==SetRes);
  printf("SetRes = %8x\n", tReturn);
  if     (SetRes==0) tRes = 800e-12;
  else if(SetRes==1) tRes = 200e-12;
  else if(SetRes==2) tRes = 100e-12;
  else if(SetRes==3) tRes =  25e-12;   
  else assert(0); 
  printf("tRes = %e s\n", tRes);
  //-------Set subtraction mode
  if(EnSubTrg)
  {
    opcd[0]=0x1400;
    V1190WriteOpcode(1, opcd);
  }
  
  //----- make tInfoTree -------------------------------------------------
  if(1)  //Dumb workaround for issued caused by goto exit_prog throughout code
  {
  tInfoTree = new TTree("tInfoTree", "tInfoTree");  
  
  Float_t tTMWwidthInSec = TMWwidth*25e-9;  //widht given in number of clock cycles
  Float_t tPrecision = tRes;
  
  time_t now = std::time(0);
  tm *ltm = std::localtime(&now);
  TString tTimeTagString = TString::Format("%02d%02d%02d%02d%02d%02d",
                                            ltm->tm_year-100,
                                            ltm->tm_mon+1, 
                                            ltm->tm_mday, 
                                            ltm->tm_hour, 
                                            ltm->tm_min, 
                                            ltm->tm_sec);
  ULong_t tTimeTagLong = tTimeTagString.Atoll();
  tTimeTagString.Remove(tTimeTagString.Length()-2);
  UInt_t  tTimeTagInt = tTimeTagString.Atoi();
  
  //-----
  
  tInfoTree->Branch("Comment", &tCommentString);
  tInfoTree->Branch("TimeWindowSize", &tTMWwidthInSec, "TimeWindowSize/F");
  tInfoTree->Branch("Precision", &tPrecision, "Precision/F");
  tInfoTree->Branch("TimeStampLong", &tTimeTagLong, "TimeStampLong/l");
  tInfoTree->Branch("TimeStamp", &tTimeTagInt, "TimeStamp/i");
  
  tInfoTree->Fill();
  }
  //----------------------------------------------------------------------  
  
  //-------Initialize histograms for real-time monitoring
  tRealTimeCFs->Add(new TH1D("tAutoCF1", "tAutoCF1", 2000, -2500000*tRes, 2500000*tRes));
  tRealTimeCFs->Add(new TH1D("tAutoCF2", "tAutoCF2", 2000, -2500000*tRes, 2500000*tRes));
  tRealTimeCFs->Add(new TH1D("tCF","tCF", 2000, -2500000*tRes, 2500000*tRes));    

  tRealTimeCanvases->Add(new TCanvas("tCan_AutoCF1", "tCan_AutoCF1"));
  tRealTimeCanvases->Add(new TCanvas("tCan_AutoCF2", "tCan_AutoCF2"));
  tRealTimeCanvases->Add(new TCanvas("tCan_CF", "tCan_CF"));  

  tPhaseSpace = (TH1D*)((TH1D*)tRealTimeCFs->At(0))->Clone("tPhaseSpace");
  for(int i=1; i<tPhaseSpace->GetNbinsX()+1; i++)
  {
    if(fabs(tPhaseSpace->GetXaxis()->GetBinCenter(i)) > tRes*pow(2., 21)) tPhaseSpace->SetBinContent(i, 0.);
    else
    {
      tPhaseSpace->SetBinContent(i, 1.0-fabs(tPhaseSpace->GetXaxis()->GetBinCenter(i)/(tRes*pow(2., 21))));
    }
  }
  
  for(int i=0; i<tRealTimeCanvases->GetEntries(); i++)
  {  
    ((TCanvas*)tRealTimeCanvases->At(i))->Divide(2,1);
    ((TCanvas*)tRealTimeCanvases->At(i))->Draw();
  }
  //--------------------------------------------------------------------------------
    
    printf("Board Ready. Press a key to start the acquisition ('q' to quit)\n");
    if (getch()=='q')
        goto exit_prog;
        
    V1190WriteRegister(SW_CLEAR, 0);        

    // Set maximum buffer size for event readout
    BufferSize =  1024 * 1024;
    if ( (buff = (unsigned int *)malloc(BufferSize)) == NULL) {
        printf("Can't allocate memory buffer of %d KB\n", BufferSize/1024);
        goto exit_prog;
    }
    for(i=0; i<NUM_CHANNELS; i++) {
        Histo[i] = (unsigned int *)malloc(4*HistoNbin);
		memset(Histo[i], 0, 4*HistoNbin);
        DiscardCnt[i] = 0;
        HitCnt[i] = 0;
        NegCnt[i] = 0;
		mean[i]=0;
		stddev[i]=0;
		nstat[i]=0;
    }
    TrgCnt = 0;

    if (Write2File)
        fout = fopen(OUTFILE_NAME, "w");
    // ************************************************************************
    // Readout
    // ************************************************************************
    printf("\nReadout started.\n");
    PreviousTime = get_time();
    // Readout Loop
    while(!Quit) {
	
		if (kbhit()) {
			char c;
			c = getch();
			if (c == 'q')
				Quit = 1;
			if (c == 'r') {
				printf("Statistics and Histograms have been cleared\n");
				TrgCnt=0; PrevTrgCnt=0;
                for(i=0; i<NUM_CHANNELS; i++) {
                    memset(Histo[i], 0, 4*HistoNbin);
                    DiscardCnt[i] = 0;
                    HitCnt[i] = 0;
                    NegCnt[i] = 0;
                }
            }
            if (c == 'h')
                SaveHistograms(Histo, HistoNbin, ChMask);
            if (c == 'p') 
				if ((ChMask >> ChToPlot) & 0x1)
					PlotHistograms(Histo, HistoNbin, ChToPlot);
            if (c == 'c') {
				printf("Enter Channel to Plot: ");
				scanf("%d", &ChToPlot);
			}
            if (c == ' ') {
                printf("\n\n[q] Quit\n");
                printf("[r] Reset Statistics\n");
                printf("[h] Save Histograms\n");
                printf("[p] Plot Histogram\n\n");
            }

		}

		ret = CAENVME_FIFOBLTReadCycle(handle, BaseAddress, (unsigned char *)buff, BufferSize, cvA32_U_MBLT, cvD64, &nb);
		if ((ret != cvSuccess) && (ret != cvBusError)) {
			printf("Readout Error\n");
			goto exit_prog;
		}
		
		// ---------------------------------------------------------------------------
		// Save raw data to output files
		// ---------------------------------------------------------------------------
//		int r;
                tNGulps++;
		if (RawData == 1){
                        ExtractEventsAndLoadTree(tRes, tNGulps, nb, buff, tTTE, tTTETree, tRealTimeCFs, tRealTimeCanvases, tTimes1, tTimes2, tGCan, tPhaseSpace, 1, 8);
			for(r=0; r<nb/4; r++)
			{
				fprintf(fr, "%8x\n", buff[r]);

				if(IS_TDC_ERROR(buff[r])) tNErrs++;
				if(IS_GLOBAL_TRAILER(buff[r]) && ((buff[r]>>24) & 0x7) != 0) tNErrs++;
		  }
		}
		
        // --------------------------------------------------------------------
        // Calculate throughput rate (every second)
        // --------------------------------------------------------------------
		CurrentTime = get_time(); // Time in milliseconds
		ElapsedTime = CurrentTime - PreviousTime;
		if (ElapsedTime > 1000) {
			TPrate = ((float)(totnb) / ElapsedTime)*1000.0;     // Bytes/second
			TRGrate = ((float)(TrgCnt - PrevTrgCnt) / ElapsedTime)*1000.0;  // Triggers/second
			if (totnb==0)   printf("No data\n");
			else			printf("Readout Rate=%.2fMB/s. TrgRate=%.4f Hz\n", TPrate/1048576, TRGrate);
            printf("TDC_ErrorFlags = %04x; Ovf=%d; TrgLost=%d\n", TDCerrors, Ovf, TrgLost);
			if (TrgCnt==0) {
				printf("No Trigger\n");
			} else {
				printf("%lld triggers processed\n", TrgCnt);
				printf("CH\tFound\tDiscarded\tNegative\n");
				
				for(i=0; i<(NUM_CHANNELS); i++) {
//					printf("%d\t%6.2f%%\t%6.2f%%\n", i, (float)HitCnt[i]*100.0/TrgCnt, (float)DiscardCnt[i]*100.0/TrgCnt);
					if (nstat[i] > 0) {
						mean[i] = mean[i] / nstat[i];
						stddev[i] = sqrt(stddev[i]/nstat[i] - mean[i]*mean[i]);
						if (BoardType == 1190) 
						  printf("%d\t%6.2f%%\t%6.2f%%\t\t%6.2f%%\t  nstat=%d m=%.3f (ps)  s=%.3f (ps)\n", i, (float)HitCnt[i]*100.0/TrgCnt, (float)DiscardCnt[i]*100.0/TrgCnt,(float)NegCnt[i]*100.0/TrgCnt, nstat[i], 100*mean[i], 100*stddev[i]);
						else
						  printf("%d\t%6.2f%%\t%6.2f%%\t\t%6.2f%%\t  nstat=%d m=%.3f (ps)  s=%.3f (ps)\n", i, (float)HitCnt[i]*100.0/TrgCnt, (float)DiscardCnt[i]*100.0/TrgCnt,(float)NegCnt[i]*100.0/TrgCnt, nstat[i], 25*mean[i], 25*stddev[i]);
					} else {
						printf("%d\t%6.2f%%\t%6.2f%%\t\t%6.2f%%\t   ---\n", i, (float)HitCnt[i]*100.0/TrgCnt, (float)DiscardCnt[i]*100.0/TrgCnt,(float)NegCnt[i]*100.0/TrgCnt);
					
					}

					mean[i]=0;
					stddev[i]=0;
					nstat[i]=0;
                }
			}
			printf("\n\n");
			totnb=0; TDCerrors=0; Ovf=0; TrgLost=0;
			PrevTrgCnt = TrgCnt;
			PreviousTime = CurrentTime;
		}
		
		if (nb == 0)
			continue;
        totnb += nb;
		

        // --------------------------------------------------------------------
        // Data Analysis
        // --------------------------------------------------------------------
		Rpnt = 0;
		while (Rpnt < (nb/4)) {
			if(IS_GLOBAL_HEADER(buff[Rpnt])) {
                if (!Header)
                    printf("Unexpected Header (word n. %d)\n", Rpnt);
                if (Write2File)
                    fprintf(fout, "Event Counter = %d (Ev.n.%lld)\n", DATA_EVENT_COUNTER(buff[Rpnt]), TrgCnt);
                Header=0;
                WordPnt = 1;
                TrgCnt++;
                ErrorFlags=0;
                memset(ChFound, 0, NUM_CHANNELS*sizeof(ChFound[0]));
            } else if (IS_GLOBAL_TRAILER(buff[Rpnt])) {
                //nw = DATA_TDC_WORD_CNT(buff[Rpnt]);
                Ovf = buff[Rpnt]>>25&1;
                TrgLost = buff[Rpnt]>>26&1;
                /*if (nw != (WordPnt+1)) {
                    printf("Wrong Event size: Word counter in trailer is %d, while actual is %d\n", nw, WordPnt);
                }*/                
                if (Write2File) {
                    fprintf(fout, "Status: ErrorFlags = %04x; Ovf=%d; TrgLost=%d\n\n", ErrorFlags, buff[Rpnt]>>25&1, buff[Rpnt]>>26&1);
				}
				if (ChFound[ChTref]) {
					fprintf(fout, "Tref(Ch %d) = %d\n", ChTref, TimeAbs[ChTref]);
					for (i=0; i<NUM_CHANNELS; i++) {
						if ((ChFound[i]) && (i != ChTref)) {
  						    timerel = TimeAbs[i] - TimeAbs[ChTref]; 
							if (timerel > 0) {
								nstat[i]++;
								mean[i] += (double)timerel;
								stddev[i] += (double)(timerel*timerel);
								if (Write2File)
									fprintf(fout, "Ch %d = %d\n", i, timerel);
								if ((timerel > 0) && ((timerel/HistoBinSize) < HistoNbin))
									Histo[i][(timerel/HistoBinSize)]++;
							}
							else
								NegCnt[i]++;
						}
					}
				}
                Header=1;    
            } else if (IS_TDC_ERROR(buff[Rpnt])) {
                ErrorFlags = buff[Rpnt] & 0x7FFF;
                TDCerrors |= ErrorFlags;
            } else if (IS_TDC_DATA(buff[Rpnt])) {                
                if (Header)
                    printf("Missing Header (word n. %d)\n", Rpnt);
                if (BoardType == 1190) {
                    ch = DATA_CH(buff[Rpnt]);
                    time = DATA_MEAS(buff[Rpnt]);
                } else {
                    ch = DATA_CH_25(buff[Rpnt]);
                    time = DATA_MEAS_25(buff[Rpnt]);
                }

                if (ch < NUM_CHANNELS) {
					if (!ChFound[ch]) {
						ChFound[ch] = 1;
						TimeAbs[ch] = time;
//						if (ch == ChTref) 
//							Tref = time;
					} else {
						DiscardCnt[ch]++;
					}
                    HitCnt[ch]++;                    
                }
            } else if (IS_FILLER(buff[Rpnt])) {
                if (Rpnt < ((nb/4)-1))
                    printf("Unexpected filler (word n. %d)\n", Rpnt);
            }
            Rpnt++;
            WordPnt++;
		}
    }
    
    SaveHistograms(Histo, HistoNbin, ChMask);        
    ErrorLevel=0;

	if (RawData == 1){
		fclose(fr);
		printf("Raw Data Files saved\n");		
	}
	
exit_prog:

    if (gnuplot != NULL)  
		_pclose(gnuplot);
    if (buff != NULL)  
		free(buff);
    for(i=0; i<NUM_CHANNELS; i++) 
        if (Histo[i] != NULL)  free(Histo[i]);
    if (handle != -1)  
		CAENVME_End(handle);
    if (ErrorLevel<0)
        getch();
        
    
    printf("tNErrs = %d\n", tNErrs);
    printf("tNGulps = %d\n", tNGulps);  
    tTreeFile->Write(); 
    tTreeFile->Close();      
        
    return 0;
}



