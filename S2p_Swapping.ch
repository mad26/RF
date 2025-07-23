// File Created by Guts Explorer: 7/16/2025 3:08:26 PM
#include "CHInclude.h"
#include "GlobalAPI\\CRSF\\CRSF.h"

ExecutionTable     	O_ExecutionTable;
CRSF 				O_CRSF;
SkySignal       	O_SIG;

char set_s2p[1000];

char temp1[10000];
char temp2[10000];

char Path[512];
char Path2[512];
char s2pNameOnly[512];

double val;
int ExecStatus;

void Init()
{
	O_CRSF.GutsOptionsCheck();
	O_CRSF.Initialize(&O_ExecutionTable);

}

//Measure() is site independent
void Measure()
{	
	
	ExecStatus = O_ExecutionTable.SetCurrentRow(0);
	
	while(ExecStatus > -1)
	{
		
		
		O_ExecutionTable.GetValFromArray("SET_PORT_DEEMB", &val, temp1);
		sprintf(set_s2p, temp1);	
		
		O_CRSF.SplitFileToPathAndName(temp1, Path, s2pNameOnly);
		O_CRSF.StripExtension(s2pNameOnly, s2pNameOnly);
		
		O_SIG.LoadS2p(1, temp1);
		O_SIG.S2pSwapPorts(1, 2);
		
		sprintf(Path2, "%s\\Swapped", Path);
		O_CRSF.DirectoryExists(Path2, true);
		
		sprintf(temp2, "%s\\%s_SWAP.s2p", Path2, s2pNameOnly);
		
		
		O_SIG.SaveS2p(2, temp2);
		ExecStatus = O_CRSF.IncrementSetRow();
	}	
	
	
}

void Closing()
{

}

