// File Created by Guts Explorer: 6/28/2026 5:53:17 PM, 
//Author: Madhav Khindri, Jawandt; Memory Optimization: Netza Soto

#define ShowStatusMessage // Enables GUTS' internal StatusMessage() macro. Must be defined before including CHInclude.h
#include "CHInclude.h"
#include "GlobalAPI\\CRSF\\CRSF.h"
#include "C:\\Char_Testplans\\SET_file_Checker\\waveform_mpr_lookup.h"
#include "C:\\Char_Testplans\\SET_file_Checker\\Set_Functions.h"

void Init()
{
	//initCode

}

//Measure() is site independent
void Measure()
{
	if (GetCurrentSite() > 1 || GetDeviceCount() > 1)
		return;

	if (!largeStringVar)
	{
		largeStringVar = new char[LARGE_STRING_VAR_SIZE + 1];
		if (!largeStringVar)
		{
			MsgBox("Cannot reserve memory for verification", ERROROKONLY, false);
			Abort();
			return;
		}
	}

	createlog(); //start logging error file, always stay at top other functions

	CheckForWaveforms();
	CheckForPulsedWaveforms();
	CheckForUniqueMainNumbers();
	CheckForRealTime();
	CheckForSetPortAndDeembed();
	CheckForGainComp();
	CheckForNAPowerSupplies();
	CheckForTempOrder();
	CheckForBandFreqRange();
	CheckForVSWRandPhase();
	CheckForMIPIWords();
	CheckSetFileAgainstPrm();
	CheckForMeasParams();
	ClearWaveformFolder();
	

	if (largeStringVar)
	{
		largeStringVar = NULL;
	}
}



void Closing()
{
	
}

