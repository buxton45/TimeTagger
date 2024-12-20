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
#include "TGClient.h"

#include <assert.h>
#include <vector>
#include <ctime>
#include <iostream>
#include <string>
#include <bitset>
using std::vector;
using std::cout;
using std::endl;




//***************************************************************************
#define SWRELEASE     "1.1"
//***************************************************************************

//#define NUM_CHANNELS   32
#define NUM_CHANNELS   16 // V1290N has 16 channels, not 32
#define OUTFILE_NAME   "V1190EventList.txt"

// ###########################################################################
// Global Variables
// ###########################################################################
int handle=-1;
unsigned int BaseAddress = 0xEE000000;
int VMEerror = 0;
FILE *gnuplot=NULL;
int PlotError=0;
int RawDataTxt=0;
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
void FillAutoCF(TH1D* aAutoCF, const vector<float> &aData, TH1D* aTimes)
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
void FillCF(TH1D* aCF, const vector<float> &aData1, const vector<float> &aData2)
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
unsigned int GetPositionInArray(unsigned int aGridSize, unsigned int aI, unsigned int aJ)
{
//For example, in the diagram below

//     j=0   
//       |
// i=0-- 0   1   2   3              0   1   2   3
//       4   5   6   7    -------->     4   5   6
//       8   9   10  11   -------->         7   8
//       12  13  14  15                         9

  return aI*aGridSize + aJ - aI*(aI+1)/2;  //Note: aI*(aI+1)/2 = 1 + 2 + ... + I
}

// ---------------------------------------------------------------------------
void FillAllCFsAndTimes(const vector<unsigned int> &aActiveChannels, TObjArray* aRTCFs, TObjArray* aRTAbsTimes, const vector<vector<float> > &aData)
{
  unsigned int tCFPosInArr=-1;
  for(unsigned int i=0; i<aActiveChannels.size(); i++)
  {
    for(unsigned int j=i; j<aActiveChannels.size(); j++)
    {
      tCFPosInArr = GetPositionInArray(aActiveChannels.size(), i, j);
      if(i==j)
      {
/*
cout << "About to call FillAutoCF.........................." << endl;
cout << "((TH1D*)aRTCFs->At(tCFPosInArr))->GetTitle() = " << ((TH1D*)aRTCFs->At(tCFPosInArr))->GetTitle() << endl;
cout << "aActiveChannels[i] = " << aActiveChannels[i] << endl;
cout << "((TH1D*)aRTAbsTimes->At(i))->GetTitle() = " << ((TH1D*)aRTAbsTimes->At(i))->GetTitle() << endl << endl;
*/
        FillAutoCF((TH1D*)aRTCFs->At(tCFPosInArr), aData[aActiveChannels[i]], (TH1D*)aRTAbsTimes->At(i));
      }
      else
      {
/*
cout << "About to call FillCF........................." << endl;
cout << "((TH1D*)aRTCFs->At(tCFPosInArr))->GetTitle() = " << ((TH1D*)aRTCFs->At(tCFPosInArr))->GetTitle() << endl;
cout << "aActiveChannels[i] = " << aActiveChannels[i] << endl;
cout << "aActiveChannels[j] = " << aActiveChannels[j] << endl << endl;
*/
        FillCF((TH1D*)aRTCFs->At(tCFPosInArr), aData[aActiveChannels[i]], aData[aActiveChannels[j]]);       
      }
    }
  }
}

// ---------------------------------------------------------------------------
void DrawAllCFs(unsigned int aNActiveChannels, TObjArray* aRTCFs, TObjArray* aRTCanvases, TH1D* aPhaseSpace)
{
  unsigned int tCFPosInArr=-1;
  unsigned int tPadNumber=-1;
  TH1D* tNormCFToDraw;
  TCanvas* tRTCanCFs     = (TCanvas*)aRTCanvases->At(0);
  TCanvas* tRTCanCFsZoom = (TCanvas*)aRTCanvases->At(1);
  for(unsigned int i=0; i<aNActiveChannels; i++)
  {
    for(unsigned int j=i; j<aNActiveChannels; j++)
    {
      tCFPosInArr = GetPositionInArray(aNActiveChannels, i, j);
      tNormCFToDraw = (TH1D*)((TH1D*)aRTCFs->At(tCFPosInArr))->Clone(TString::Format("Norm_%s", ((TH1D*)aRTCFs->At(tCFPosInArr))->GetTitle()));
      tNormCFToDraw->Divide(aPhaseSpace);
      
      tPadNumber = i*aNActiveChannels + j + 1; //+1 because pad numbering starts at 1, not 0!
      tRTCanCFs->cd(tPadNumber);
      tNormCFToDraw->DrawCopy();
      
      tNormCFToDraw->GetXaxis()->SetRangeUser(-0.000005, 0.000005);
      tRTCanCFsZoom->cd(tPadNumber);
      tNormCFToDraw->DrawCopy();      
    }
  }
  tRTCanCFs->Update();
  tRTCanCFsZoom->Update();
  tNormCFToDraw->Delete();
}

// ---------------------------------------------------------------------------
void DrawAllAbsTimes(unsigned int aNActiveChannels, TObjArray* aRTAbsTimes, TCanvas* aCan)
{
  for(unsigned int i=0; i<aNActiveChannels; i++)
  {
    aCan->cd(i+1); //+1 because pad numbering starts at 1, not 0!
    ((TH1D*)aRTAbsTimes->At(i))->DrawCopy();
  }
  aCan->Update();
}

// ---------------------------------------------------------------------------
unsigned int ExtractEventsAndLoadTree(vector<unsigned int> &aActiveChannels, const double aRes, int aGulp, int aNb, unsigned int *aBuff, TimeTaggerEvent* aTTE, TTree* aTTETree, TObjArray* aRTCFs, TObjArray* aRTAbsTimes, TObjArray* aRTCanvases, TH1D* aPhaseSpace, bool aEnRTMonitor)
{
  int tNChannels=16;
  vector<vector<float> > tData(tNChannels, vector<float>(0));
  for(unsigned int iCh=0; iCh<tData.size(); iCh++) tData[iCh].reserve(aNb);
  int tEventNumber = -1;
  unsigned int tNErrorsInGulp = 0;
  
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
      if(((aBuff[i]>>24) & 0x7) != 0) 
      {
        aTTE->SetContainsError(true);
        tNErrorsInGulp++;
      }
      for(unsigned int iCh=0; iCh<tData.size(); iCh++) aTTE->SetTimeList(iCh, tData[iCh].size(), tData[iCh].data());
      if(!aTTE->ContainsError()) aTTETree->Fill();
      //-------------------------------------------
      if(aEnRTMonitor)
      {
        //Fill real-time histograms
        FillAllCFsAndTimes(aActiveChannels, aRTCFs, aRTAbsTimes, tData);
      
        //Draw real-time histograms if tEventNumber%1000==0 
        if(tEventNumber%1000==0 && tEventNumber>0)
        {
          DrawAllCFs(aActiveChannels.size(), aRTCFs, aRTCanvases, aPhaseSpace);
          DrawAllAbsTimes(aActiveChannels.size(), aRTAbsTimes, (TCanvas*)aRTCanvases->At(2));
        }
      }
      //-------------------------------------------
      for(unsigned int iCh=0; iCh<tData.size(); iCh++) tData[iCh].clear();
      aTTE->Reset();
    }    
    else if(IS_TDC_DATA(aBuff[i]))
    {
      int tChannel = DATA_CH_25(aBuff[i]);
      assert(tChannel>=0 && tChannel<16);  //TODO not vital, just safeguard
      tData[tChannel].push_back(aRes*DATA_MEAS_25(aBuff[i]));  
    }
    else if(IS_TDC_ERROR(aBuff[i])) 
    {
      aTTE->SetContainsError(true);
      tNErrorsInGulp++;
    }
    else continue;
  }
  return tNErrorsInGulp;
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

// ---------------------------------------------------------------------------
vector<unsigned int> GetActiveChannels(const int aChMask)
{
  std::bitset<16> tChMask_binary (aChMask);
  cout << "tChMask_binary = " << tChMask_binary << endl;
  vector<unsigned int> tActiveChannels(0);
  for(unsigned int i=0; i<tChMask_binary.size(); i++)
  {
    if(tChMask_binary[i]) tActiveChannels.push_back(i);
  }
  for(unsigned int i=0; i<tActiveChannels.size(); i++) cout << "Channel " << tActiveChannels[i] << " is active" << endl;
  cout << endl;
  return tActiveChannels;
}

// ---------------------------------------------------------------------------
unsigned int fact(int aN)
{
  //factorial (should only be used for small numbers, i.e. small enough to fit in int
  unsigned int tResult = 1;
  for(int i=1; i<=aN; i++) tResult *= i;
  return tResult;
}

// ---------------------------------------------------------------------------
unsigned int GetNumberOfCFsToDraw(int aNActiveChannels)
{
  //Number of correlation functions is nCr, 
  //  where r=2, and n=aNActiveChannels
  //Number of auto-correlations is aNActiveChannels
  return aNActiveChannels + fact(aNActiveChannels)/(fact(2)*fact(aNActiveChannels-2));
}




// ---------------------------------------------------------------------------
void CreateRTCanvases(TObjArray* aRTCanvases, unsigned int aNActiveChannels, 
                      double aMenuSizeLeft=0.05, double aMenuSizeRight=0.00,
                      double aMenuSizeTop=0.03, double aMenuSizeBottom=0.00)
{
  //NOTE: MenuSizes in terms of % of screen
  //      Defaults Left=0.05, Right=0.00, Top=0.03, Bottom=0.00 for typical Ubuntu installation

  //NOTE: ExtractEventsAndLoadTree and methods within (e.g. DrawAllCFs) expect 
  //      tRTCanvases->At(0) to contain a TCanvas for CFs (with aNActiveChannels x aNActiveChannels grid)
  //      tRTCanvases->At(1) to contain a TCanvas for CFsZoom (with aNActiveChannels x aNActiveChannels grid)
  //      tRTCanvases->At(2) to contain a TCanvas for AbsTimes (with aNActiveChannels x 1 grid)
    
  unsigned int tDispWidth  = gClient->GetDisplayWidth();
  unsigned int tDispHeight = gClient->GetDisplayHeight();

  double tUsableWidth  = (1.0-aMenuSizeLeft-aMenuSizeRight)*tDispWidth;
  double tUsableHeight = (1.0-aMenuSizeTop-aMenuSizeBottom)*tDispHeight;
  //----------
  aRTCanvases->Add(new TCanvas("tRTCanCFs", "tRTCanCFs", 
                               aMenuSizeLeft*tDispWidth, 
                               aMenuSizeTop*tDispHeight, 
                               0.5*tUsableWidth, 
                               0.5*tUsableHeight));
  ((TCanvas*)aRTCanvases->At(0))->Divide(aNActiveChannels, aNActiveChannels);
  //----------
  aRTCanvases->Add(new TCanvas("tRTCanCFsZoom", "tRTCanCFsZoom", 
                               aMenuSizeLeft*tDispWidth + 0.5*tUsableWidth, 
                               aMenuSizeTop*tDispHeight, 
                               0.5*tUsableWidth, 
                               0.5*tUsableHeight));
  ((TCanvas*)aRTCanvases->At(1))->Divide(aNActiveChannels, aNActiveChannels);
  //----------
  aRTCanvases->Add(new TCanvas("tRTCanAbsTimes", "tRTCanAbsTimes", 
                               aMenuSizeLeft*tDispWidth, 
                               aMenuSizeTop*tDispHeight + 0.5*tUsableHeight, 
                               0.5*tUsableWidth, 
                               0.5*tUsableHeight));
  ((TCanvas*)aRTCanvases->At(2))->Divide(aNActiveChannels, 1);    
}


// ##################################################################################################################
// ------------------------------------------------------------------------------------------------------------------
// ##################################################################################################################
int main(int argc, char *argv[])
{
  TApplication theApp("App", &argc, argv);  //Needed for real time figures to be plotted with ROOT
                                            //Without this, apparently ROOT runs in batch mode
  TObjString* tCommentString = GetComments();

  unsigned short fwrev, ctrl, ErrorFlags=0, opcd[10];
  int sn, BoardType;
    int TDCerrors=0, Ovf=0, TrgLost=0;
    int Write2File=1, TMWwidth=40, TMWoffs=-20;
  int Quit=0, BufferSize, i, WordPnt, totnb=0, ChFound[NUM_CHANNELS], TimeAbs[NUM_CHANNELS];
  unsigned int ChMask;
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
  //Because of all the goto exit_prog, the compiler complains a lot when you try to declare variables within the body
  //of the program.  So, we declare basically everything here.
  int tNGulps=0;
  int tNErrs=0;
  vector<unsigned int> tActiveChannels(0);
  unsigned int tNCFsToDraw;
	
  char tSaveDirBase[1000];
  TString tTreeFileName;
  TFile* tTreeFile;
  TTree* tTTETree;
  TTree* tInfoTree;
  TimeTaggerEvent* tTTE;
  
	
  unsigned int EnSubTrg, SetRes, EnRTMonitor;
  double tRes;
	
  //RT = real time
  TObjArray* tRTCFs = new TObjArray();
  TObjArray* tRTCanvases = new TObjArray();
  TObjArray* tRTAbsTimes = new TObjArray();
  TH1D* tPhaseSpace;
  
  Float_t tTMWwidthInSec;  //widht given in number of clock cycles
  Float_t tPrecision;  

  time_t now = std::time(0);
  tm *ltm = std::localtime(&now);
  TString tTimeTagString = TString::Format("%02d%02d%02d%02d%02d",
                                            ltm->tm_year-100,
                                            ltm->tm_mon+1, 
                                            ltm->tm_mday, 
                                            ltm->tm_hour, 
                                            ltm->tm_min);
  UInt_t  tTimeTagInt = tTimeTagString.Atoi();
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

  // ************************************************************************
  // Read configuration file
  // ************************************************************************

  if (argc > 1) strcpy(tmpConfigFileName, argv[1]);

#ifdef  WIN32
	sprintf(path, "%s\\VME\\", getenv("USERPROFILE"));
	_mkdir(path);
	sprintf(ConfigFileName, "%s\\%s", path, tmpConfigFileName);
#else
	sprintf(path, ".");
	sprintf(ConfigFileName, "%s/%s", path, tmpConfigFileName);
#endif

  if ( (f_ini = fopen(ConfigFileName, "r")) == NULL ) 
  {
    printf("Can't open Configuration File %s\n", ConfigFileName);
    goto exit_prog;
  }

	    
  printf("Reading Configuration File %s\n", ConfigFileName);
  while(!feof(f_ini)) 
  {
    int data, addr;
    int nwopc, val;
    unsigned short opcd[10];
    int app;
		
    str[0] = '#';
    fscanf(f_ini, "%s", str);
    if (str[0] == '#') fgets(str, 1000, f_ini);
    else 
    {
      // LINK: Open VME master
      if (strstr(str, "LINK")!=NULL) 
      {
        CVBoardTypes BType;
        int link=0, bdnum=0;
				
        fscanf(f_ini, "%s", tmp);
        if      (strstr(tmp, "V1718")!=NULL) BType = cvV1718;
        else if (strstr(tmp, "V2718")!=NULL) BType = cvV2718;
        else 
        {
          printf("Invalid VME Bridge Type\n");
          goto exit_prog;
        }
        fscanf(f_ini, "%d", &link);
        if (BType == cvV2718) fscanf(f_ini, "%d", &bdnum);
        if (CAENVME_Init(BType, link, bdnum, &handle) != cvSuccess) 
        {
          printf("VME Bridge init failure\n");
          goto exit_prog;
        }
      }

      // Base Address
      if (strstr(str, "BASE_ADDRESS")!=NULL) fscanf(f_ini, "%x", (int *)&BaseAddress);

      // Raw Data
      if (strstr(str, "RAW_DATA_TXT")!=NULL) fscanf(f_ini, "%d", (int *)&RawDataTxt);
				
      // ChannelMask
      if (strstr(str, "CHANNEL_MASK")!=NULL) fscanf(f_ini, "%x", (int *)&ChMask);

      // Time Reference
      if (strstr(str, "TIME_REF")!=NULL) fscanf(f_ini, "%d", &ChTref);

      // Trigger Matching Window
      if (strstr(str, "TRIGGER_WINDOW")!=NULL) 
      {
        fscanf(f_ini, "%d", &val);
        TMWwidth = (unsigned short)val;
        fscanf(f_ini, "%d", &val);
        TMWoffs = (unsigned short)val;
      }
                
      // Write File
      if (strstr(str, "WRITE_EVENT_FILE")!=NULL) fscanf(f_ini, "%d", &Write2File);

      // Generic VME Write
      if (strstr(str, "WRITE_REGISTER")!=NULL) 
      {
        fscanf(f_ini, "%x", &addr);
        fscanf(f_ini, "%x", &data);
				V1190WriteRegister((unsigned short)addr, (unsigned short)data);
        if (VMEerror) 
        {
          printf("VME Write failure at address %08X\n", BaseAddress + addr);
          goto exit_prog;
        }
      }

      // Write Opcode
      if (strstr(str, "WRITE_OPCODE")!=NULL) 
      {
        fscanf(f_ini, "%d", &nwopc);
        for(i=0; i<nwopc; i++) 
        {
          //fscanf(f_ini, "%x", (int *)&opcd[i]);
				  fscanf(f_ini, "%x", &app);
				  opcd[i] = (unsigned short) app;
        }
        if(V1190WriteOpcode(nwopc, opcd)) 
        {
          printf("Opcode Write failure (nwopc=%d, opcd=%x)\n", nwopc, opcd[0]);
          goto exit_prog;
        }              
      }
            
      // Trigger subtraction
      if (strstr(str, "EN_SUB_TRG")!=NULL) fscanf(f_ini, "%d", &EnSubTrg);
                
      // Resolution setting
      if (strstr(str, "SET_RES")!=NULL) fscanf(f_ini, "%d", &SetRes);                
             
      // Save directory base
      if (strstr(str, "DATA_SAVE_BASE")!=NULL) fscanf(f_ini, "%s", tSaveDirBase);    
                
      // Real time monitoring
      if (strstr(str, "REAL_TIME_MONITOR")!=NULL) fscanf(f_ini, "%d", &EnRTMonitor);                
                                           
    }
  }
  fclose (f_ini);


	if (RawDataTxt == 1) fr = fopen(TString::Format("%sRaw_Data%s.txt", tSaveDirBase, tTimeTagString.Data()).Data(), "w");
		
  tTreeFileName = TString::Format("%sRun%s.root", tSaveDirBase, tTimeTagString.Data());
  tTreeFile = new TFile(tTreeFileName, "RECREATE");	
  tTTE = new TimeTaggerEvent();
  tTTETree = new TTree("tTTETree", "tTTETree");
  tTTETree->Branch("TimeBranch", &tTTE);


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
  if (VMEerror) 
  {
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

//  opcd[0]=0x3100; 
//  V1190WriteOpcode(1, opcd);  // Disable TDC Header/Trailer
    
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

  if (BoardType == 1190) V1190WriteOpcode(9, opcd);  // Enable Channels
	else                   V1190WriteOpcode(3, opcd);  // Enable Channels


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
  tInfoTree = new TTree("tInfoTree", "tInfoTree");  
  
  tTMWwidthInSec = TMWwidth*25e-9;  //widht given in number of clock cycles
  tPrecision = tRes;  
  //-----  
  tInfoTree->Branch("Comment", &tCommentString);
  tInfoTree->Branch("TimeWindowSize", &tTMWwidthInSec, "TimeWindowSize/F");
  tInfoTree->Branch("Precision", &tPrecision, "Precision/F");
  tInfoTree->Branch("ChannelMask", &ChMask, "ChannelMask/i");
  tInfoTree->Branch("TimeStamp", &tTimeTagInt, "TimeStamp/i");  
  tInfoTree->Fill();

  //----------------------------------------------------------------------  
  //-------Initialize histograms for real-time monitoring
  tActiveChannels = GetActiveChannels(ChMask);
  if(EnRTMonitor)
  {
    tNCFsToDraw = GetNumberOfCFsToDraw(tActiveChannels.size());
    for(unsigned int i=0; i<tActiveChannels.size(); i++)
    {
      for(unsigned int j=i; j<tActiveChannels.size(); j++)
      {
        if(i==j)
        {
          tRTCFs->Add(new TH1D(TString::Format("tAutoCF_%u", tActiveChannels[i]), 
                               TString::Format("tAutoCF_%u", tActiveChannels[i]), 
                               2000, -2500000*tRes, 2500000*tRes));
        }
        else
        {
          tRTCFs->Add(new TH1D(TString::Format("tCF_%u%u", tActiveChannels[i], tActiveChannels[j]), 
                               TString::Format("tCF_%u%u", tActiveChannels[i], tActiveChannels[j]), 
                               2000, -2500000*tRes, 2500000*tRes));        
        }
      }
    }
    assert(tNCFsToDraw == (unsigned int)tRTCFs->GetEntries());  //TODO probably don't really need tNCFsToDraw
    //----------
    for(unsigned int i=0; i<tActiveChannels.size(); i++)
    {
      tRTAbsTimes->Add(new TH1D(TString::Format("tTimes_%u", tActiveChannels[i]), 
                                TString::Format("tTimes_%u", tActiveChannels[i]), 
                                2000, 0., 0.000055));
    }
    //----------
    tPhaseSpace = (TH1D*)((TH1D*)tRTCFs->At(0))->Clone("tPhaseSpace");
    for(int i=1; i<tPhaseSpace->GetNbinsX()+1; i++)
    {
      if(fabs(tPhaseSpace->GetXaxis()->GetBinCenter(i)) > tRes*pow(2., 21)) tPhaseSpace->SetBinContent(i, 0.);
      else
      {
        tPhaseSpace->SetBinContent(i, 1.0-fabs(tPhaseSpace->GetXaxis()->GetBinCenter(i)/(tRes*pow(2., 21))));
      }
    }
  
  }
  //--------------------------------------------------------------------------------
    
  printf("Board Ready. Press a key to start the acquisition ('q' to quit)\n");
  if (getch()=='q') goto exit_prog;
  
  //Put the creation of TCanvases after "Board Ready..." so user doesn't need to navigate back to terminal
  if(EnRTMonitor)
  {
    //NOTE: ExtractEventsAndLoadTree and methods within (e.g. DrawAllCFs) expect 
    //      tRTCanvases->At(0) to contain a TCanvas for CFs (with tActiveChannels.size() x tActiveChannels.size() grid)
    //      tRTCanvases->At(1) to contain a TCanvas for CFsZoom (with tActiveChannels.size() x tActiveChannels.size() grid)
    //      tRTCanvases->At(2) to contain a TCanvas for AbsTimes (with tActiveChannels.size() x 1 grid)
    CreateRTCanvases(tRTCanvases, tActiveChannels.size());
  }
        
  V1190WriteRegister(SW_CLEAR, 0);        

  // Set maximum buffer size for event readout
  BufferSize =  1024 * 1024;
  if ( (buff = (unsigned int *)malloc(BufferSize)) == NULL) 
  {
    printf("Can't allocate memory buffer of %d KB\n", BufferSize/1024);
    goto exit_prog;
  }
  for(i=0; i<NUM_CHANNELS; i++) 
  {
    DiscardCnt[i] = 0;
    HitCnt[i] = 0;
    NegCnt[i] = 0;
    mean[i]=0;
    stddev[i]=0;
    nstat[i]=0;
  }
  TrgCnt = 0;

  if (Write2File) fout = fopen(TString::Format("%s%s", tSaveDirBase, OUTFILE_NAME).Data(), "w");
  // ************************************************************************
  // Readout
  // ************************************************************************
  printf("\nReadout started.\n");
  PreviousTime = get_time();
  // Readout Loop
  while(!Quit) 
  {
    if (kbhit()) 
    {
      char c;
      c = getch();
      if (c == 'q') Quit = 1;
      if (c == 'r') 
      {
        printf("Statistics have been cleared\n");
        TrgCnt=0; PrevTrgCnt=0;
        for(i=0; i<NUM_CHANNELS; i++) 
        {
          DiscardCnt[i] = 0;
          HitCnt[i] = 0;
          NegCnt[i] = 0;
        }
      }
      if (c == ' ') 
      {
        printf("\n\n[q] Quit\n");
        printf("[r] Reset Statistics\n");
      }
    }

    //THE FOLLOWING LINE IS THE ACTUAL DATA GRAB FROM THE VME/TIME TAGGER
    ret = CAENVME_FIFOBLTReadCycle(handle, BaseAddress, (unsigned char *)buff, BufferSize, cvA32_U_MBLT, cvD64, &nb);
    if ((ret != cvSuccess) && (ret != cvBusError)) 
    {
      printf("Readout Error\n");
      goto exit_prog;
    }
		
    // ---------------------------------------------------------------------------
    // Save raw data to output files
    // ---------------------------------------------------------------------------
    tNGulps++;
    tNErrs += ExtractEventsAndLoadTree(tActiveChannels, tRes, tNGulps, nb, buff, tTTE, tTTETree, tRTCFs, tRTAbsTimes, tRTCanvases, tPhaseSpace, EnRTMonitor);            
    if (RawDataTxt == 1)
    {
      for(r=0; r<nb/4; r++)
      {
        fprintf(fr, "%8x\n", buff[r]);
      }
    }
		
    // --------------------------------------------------------------------
    // Calculate throughput rate (every second)
    // --------------------------------------------------------------------
    CurrentTime = get_time(); // Time in milliseconds
    ElapsedTime = CurrentTime - PreviousTime;
    if (ElapsedTime > 1000) 
    {
      TPrate = ((float)(totnb) / ElapsedTime)*1000.0;     // Bytes/second
      TRGrate = ((float)(TrgCnt - PrevTrgCnt) / ElapsedTime)*1000.0;  // Triggers/second
      if (totnb==0) printf("No data\n");
      else			
      {
        printf("Readout Rate=%.2fMB/s. TrgRate=%.4f Hz\n", TPrate/1048576, TRGrate);
        printf("TDC_ErrorFlags = %04x; Ovf=%d; TrgLost=%d\n", TDCerrors, Ovf, TrgLost);
      }      
      if (TrgCnt==0) printf("No Trigger\n");
      else 
      {
        printf("%lld triggers processed\n", TrgCnt);
        printf("CH\tFound\tDiscarded\tNegative\n");				
        for(i=0; i<(NUM_CHANNELS); i++) 
        {
          if (nstat[i] > 0) 
          {
            mean[i] = mean[i] / nstat[i];
            stddev[i] = sqrt(stddev[i]/nstat[i] - mean[i]*mean[i]);
            if (BoardType == 1190)
            {
              printf("%d\t%6.2f%%\t%6.2f%%\t\t%6.2f%%\t  nstat=%d m=%.3f (ps)  s=%.3f (ps)\n", i, (float)HitCnt[i]*100.0/TrgCnt, (float)DiscardCnt[i]*100.0/TrgCnt,(float)NegCnt[i]*100.0/TrgCnt, nstat[i], 100*mean[i], 100*stddev[i]);
            }
						else
						{
              printf("%d\t%6.2f%%\t%6.2f%%\t\t%6.2f%%\t  nstat=%d m=%.3f (ps)  s=%.3f (ps)\n", i, (float)HitCnt[i]*100.0/TrgCnt, (float)DiscardCnt[i]*100.0/TrgCnt,(float)NegCnt[i]*100.0/TrgCnt, nstat[i], 25*mean[i], 25*stddev[i]);
            }
          } 
          else 
          {
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
		
    if (nb == 0) continue;
    totnb += nb;
		

    // --------------------------------------------------------------------
    // Data Analysis
    // --------------------------------------------------------------------
    Rpnt = 0;
    while (Rpnt < (nb/4)) 
    {
      if(IS_GLOBAL_HEADER(buff[Rpnt])) 
      {
        if (!Header)     printf("Unexpected Header (word n. %d)\n", Rpnt);
        if (Write2File) fprintf(fout, "Event Counter = %d (Ev.n.%lld)\n", DATA_EVENT_COUNTER(buff[Rpnt]), TrgCnt);
        Header=0;
        WordPnt = 1;
        TrgCnt++;
        ErrorFlags=0;
        memset(ChFound, 0, NUM_CHANNELS*sizeof(ChFound[0]));
      } 
      else if (IS_GLOBAL_TRAILER(buff[Rpnt])) 
      {
        //nw = DATA_TDC_WORD_CNT(buff[Rpnt]);
        Ovf = buff[Rpnt]>>25&1;
        TrgLost = buff[Rpnt]>>26&1;
        /*if (nw != (WordPnt+1)) 
        {
          printf("Wrong Event size: Word counter in trailer is %d, while actual is %d\n", nw, WordPnt);
        }*/                
        if (Write2File) 
        {
          fprintf(fout, "Status: ErrorFlags = %04x; Ovf=%d; TrgLost=%d\n\n", ErrorFlags, buff[Rpnt]>>25&1, buff[Rpnt]>>26&1);
				}
				if (ChFound[ChTref]) 
        {
          fprintf(fout, "Tref(Ch %d) = %d\n", ChTref, TimeAbs[ChTref]);
          for (i=0; i<NUM_CHANNELS; i++) 
          {
            if ((ChFound[i]) && (i != ChTref)) 
            {
              timerel = TimeAbs[i] - TimeAbs[ChTref]; 
              if (timerel > 0) 
              {
                nstat[i]++;
                mean[i] += (double)timerel;
                stddev[i] += (double)(timerel*timerel);
                if (Write2File) fprintf(fout, "Ch %d = %d\n", i, timerel);
              }
							else NegCnt[i]++;
            }
          }
        }
        Header=1;    
      } 
      else if (IS_TDC_ERROR(buff[Rpnt])) 
      {
        ErrorFlags = buff[Rpnt] & 0x7FFF;
        TDCerrors |= ErrorFlags;
      } 
      else if (IS_TDC_DATA(buff[Rpnt])) 
      {                
        if (Header) printf("Missing Header (word n. %d)\n", Rpnt);
        if (BoardType == 1190) 
        {
          ch = DATA_CH(buff[Rpnt]);
          time = DATA_MEAS(buff[Rpnt]);
        } 
        else 
        {
          ch = DATA_CH_25(buff[Rpnt]);
          time = DATA_MEAS_25(buff[Rpnt]);
        }

        if (ch < NUM_CHANNELS) 
        {
          if (!ChFound[ch]) 
          {
						ChFound[ch] = 1;
						TimeAbs[ch] = time;
//          if (ch == ChTref) 
//          Tref = time;
          }
          else DiscardCnt[ch]++;

          HitCnt[ch]++;                    
        }
      }
      else if (IS_FILLER(buff[Rpnt])) 
      {
        if (Rpnt < ((nb/4)-1)) printf("Unexpected filler (word n. %d)\n", Rpnt);
      }
      Rpnt++;
      WordPnt++;
    }
  }
    
  ErrorLevel=0;

  if (RawDataTxt == 1)
  {
    fclose(fr);
    printf("Raw Data Files saved\n");		
  }
	
exit_prog:

  if (gnuplot != NULL) _pclose(gnuplot);
  if (buff != NULL) free(buff);
  if (handle != -1) CAENVME_End(handle);
  if (ErrorLevel<0) getch();
        
  printf("tNErrs = %d\n", tNErrs);
  printf("tNGulps = %d\n", tNGulps);  
  tTreeFile->Write(); 
  tTreeFile->Close();      
        
  return 0;
}



