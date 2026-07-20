ExecutionTable O_ExecutionTable;
CRSF O_CRSF;

#define LARGE_STRING_VAR_SIZE (512 * 1024) // 512Kb
char *largeStringVar = NULL;
char g_LogFile[1024];

string_t setfile;
string_t prmfile;


void CheckForWaveforms();
void CheckForPulsedWaveforms();
void CheckForUniqueMainNumbers();
void CheckForRealTime();
void CheckForSetPortAndDeembed();
void CheckForGainComp();
void CheckForNAPowerSupplies();
void CheckForTempOrder();
void CheckForBandFreqRange();
void CheckForVSWRandPhase();
void CheckForMIPIWords();
void CheckSetFileAgainstPrm();
void CheckForMeasParams();


// Utils
void ClearWaveformFolder();
void CheckMIPIBus(char *mipi_command_list, char *column_name, char *main);
double GetNumberFromString(char *stringdata);
int GetMIPIBus(char *WordIn, char *WordOut);
void createlog();
void WriteToLog(const char* msg);



void createlog()
{
	//getting PRM and SET file info. to write in start of log file
	GetTestProperty("SETFileName", &setfile);
	GetTestProperty("ParameterFileName", &prmfile);
	
    char DateTime[100];

    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(DateTime, sizeof(DateTime), "%m%d%y_%H%M%S", timeinfo);
	O_CRSF.DirectoryExists("C:\\Char_Testplans\\SET_file_Checker\\Log", true); //create log folder, if it does not exist.
	sprintf(g_LogFile, "C:\\Char_Testplans\\SET_file_Checker\\Log\\error_log_%s.txt", DateTime); 

    FILE* fp = fopen(g_LogFile, "w");

    if (fp != NULL)
    {
        fprintf(fp, "Date/Time: %s\n", DateTime);
        fprintf(fp, "\nParameter File: %s\n",prmfile);
		fprintf(fp, "Set File: %s\n",setfile);
		fprintf(fp, "\n");
        fclose(fp);
    }
}
void WriteToLog(const char* msg)
{
    FILE* fp = fopen(g_LogFile, "a");

    if (fp != NULL)
    {
        fprintf(fp, "%s\n", msg);
        fclose(fp);
    }
}



void CheckForWaveforms()
{
	
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	if (O_ExecutionTable.DoesColumnExist("SET_MODULATION"))
	{
		AllowAbort();
		StatusMessage("Generating waveform list...");

		int waveformsCount = O_ExecutionTable.GetUniqueVals("SET_MODULATION", largeStringVar, LARGE_STRING_VAR_SIZE);
		StatusMessage("Checking waveform list: %d waveforms...", waveformsCount);
	

		int count = 0;
		bool isCW = false;
		bool fileExists = false;

		char *pWaveformsList = largeStringVar;
		char found[256];
		char waveformPath[1024];

		char *pStr = SplitString(pWaveformsList, ",", found);
		while (found[0])
		{
			count++;
			isCW = !strcmp(found, "CW") || !strcmp(found, "EDGE") || !strcmp(found, "GMSK");

			fileExists = isCW || O_CRSF.GetWfmDirectory(found, waveformPath);
			
			double mpr = get_mpr(found);

			char mprStr[64];

			if (mpr >= 0)
			{
				sprintf(mprStr, " | MPR: %.2f", mpr);
			}
			else
			{
				strcpy(mprStr, " | MPR: MPR not found");
			}
			
			char msg[1024];

			snprintf(msg,sizeof(msg),"Waveform %d/%d: %s%s => %s%s",count,waveformsCount,found,isCW ? ": CW based" : "",fileExists ? waveformPath : "NOT found on Server",mprStr);

			StatusMessage(msg);   // Display all waveforms

			if (!fileExists)      // Log only missing waveforms
			{
			 WriteToLog(msg);
			}

			if (!isCW && fileExists)
			{
				O_CRSF.CopyWHMToLocalDrive(found);	// Q: Is the file copy needed?  - its a feature, helps to get waveform locally if user want to plot it in real-time, if not last functions asks user to delete the local waveform folder.
				;
			}

			pStr = SplitString(pWaveformsList, ",", found);
		};
	}
	else
	{
		StatusMessage("SET_MODULATION column not in SET file");
	}

	StatusMessage("Check complete");
}

void CheckForPulsedWaveforms()
{
	// checks when pulsewaveforms are used, the SET PWIDTH column is set correctly according to the value in modulation. The function calls
	// another function here GetNumberFromString().

	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	if (O_ExecutionTable.DoesColumnExist("SET_MODULATION"))
	{
		AllowAbort();
		StatusMessage("Checking for Pulsed waveform and reated Pwidth in the SET file...");

		bool singleMod, Temp_Main;
		char G_MOD[300], G_Main[300];
		int Mod, Main;

		int ExecStatus = O_ExecutionTable.SetCurrentRow(0);
		while (ExecStatus > -1)
		{
			singleMod = O_ExecutionTable.GetValFromArray("SET_MODULATION", &Mod, G_MOD);
			Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);
			if (strstr(G_MOD, "PULSE"))
			{
				// G_MOD[0], G_MOD[1];  // Q: What is this code trying to do?? - not needed, was initially added while writing the function.
				// normalize_decimal_marker(G_MOD);
				O_CRSF.ReplaceInString(G_MOD, 'p', '.');
				double pwidth = GetNumberFromString(G_MOD);
				double PW = O_ExecutionTable.GetNumericCell("SET_PWIDTH");
				if (PW != pwidth * 1000)
				{
					//StatusMessage("SET_PWIDTH is not set correctly for pulsed modulation %s in %s", G_MOD, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "SET_PWIDTH is not set correctly for pulsed modulation %s in %s", G_MOD, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
					
				}
			}

			ExecStatus = O_ExecutionTable.IncrementRow();
		};
	}
	else
	{
		StatusMessage("SET_MODULATION column not in SET file");
	}

	StatusMessage("Check complete");
}


void CheckForUniqueMainNumbers()
{
	// sometimes setfile is edited in csv format and lines are copied which makes main numbers same and hence tester throws an error.

	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	if (O_ExecutionTable.DoesColumnExist("OFFSET_FROM_START_OF_TEST"))
	{
		StatusMessage("Checking the unique main numbers...");

		int numRows = O_ExecutionTable.GetNumDataRows();
		if (numRows < 50000) // Q: Why capped to 50000? - was taken from common engine code, they had max declare for this function as 50000.
		{
			int uniqueSetRowNames = O_ExecutionTable.GetUniqueVals("OFFSET_FROM_START_OF_TEST", largeStringVar, LARGE_STRING_VAR_SIZE);

			char msg[1024];

			snprintf(msg,sizeof(msg),"%s",numRows == uniqueSetRowNames ?"Unique Main number present" :"Found multiple instances of OFFSET_FROM_START_OF_TEST with the same value");

			StatusMessage(msg);

			if (numRows != uniqueSetRowNames)
			{
				WriteToLog(msg);
			}
		}
	}
	else
	{
		char msg[1024];

		snprintf(msg,
			sizeof(msg),
			"OFFSET_FROM_START_OF_TEST column not in SET file");

		StatusMessage(msg);
		WriteToLog(msg);
	}

	StatusMessage("Check complete");
}

void CheckForRealTime()
{
	// checks if realtime is enabled in set file. To enable realtime during a test, it should be enabled in set file and tester config. settings
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	if (O_ExecutionTable.DoesColumnExist("MEAS_REALTIME"))
	{
		int cnt = O_ExecutionTable.GetUniqueVals("MEAS_REALTIME", largeStringVar, LARGE_STRING_VAR_SIZE);

		if (cnt && !strcmp(largeStringVar, "NA")) // Meas Realtime column has NA
		{
			//StatusMessage("Realtime is not being measured");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Realtime is not being measured");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else if (cnt > 1 || strstr(largeStringVar, "MEAS")) // Meas Realtime column has a Meas{NA:NA}  or different values with Meas
		{
			StatusMessage("Realtime is being measured");
		};
	}
	else
	{
		StatusMessage("MEAS_REALTIME column not in SET file");
	}

	StatusMessage("Check complete");
}

void CheckForSetPortAndDeembed()
{
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	char *pDeembeds = NULL;
	char single_Deembed[300];
	char msg[1024];

	// check for input ports and its deembeds
	if (O_ExecutionTable.DoesColumnExist("SET_INPUT_PORT"))
	{
		int P1 = O_ExecutionTable.GetUniqueVals("SET_INPUT_PORT", largeStringVar, LARGE_STRING_VAR_SIZE);

		if (P1 && !strcmp(largeStringVar, "NA"))
		{
			StatusMessage("INPUT PORT is not being SET");
		}
		else
		{
			StatusMessage("INPUT PORT is being set to %s", largeStringVar);

			if (O_ExecutionTable.DoesColumnExist("SET_INPUTPORT_DEEMB"))
			{
				int D1 = O_ExecutionTable.GetUniqueVals("SET_INPUTPORT_DEEMB", largeStringVar, LARGE_STRING_VAR_SIZE);

				if (D1 && !strcmp(largeStringVar, "NA"))
				{
					StatusMessage("INPUT PORT DEEMBED is not being SET");
				}
				else if (P1 != D1)
				{
					snprintf(msg, sizeof(msg),
						"Number of INPUT ports is not equal to Number of INPUT Deembeds present");

					StatusMessage(msg);
					WriteToLog(msg);
				}
				else
				{
					StatusMessage("INPUT PORT DEEMB is being set to %s", largeStringVar);
					StatusMessage("Checking Deembed list against available locations...");

					pDeembeds = largeStringVar;
					single_Deembed[0] = '\0';

					SplitString(pDeembeds, ",", single_Deembed);

					while (single_Deembed[0])
					{
						bool exists = O_CRSF.FileExists(single_Deembed);

						snprintf(msg,
							sizeof(msg),
							"INPUT PORT DEEMB path %s is %svalid",
							single_Deembed,
							exists ? "" : "not ");

						StatusMessage(msg);

						if (!exists)
						{
							WriteToLog(msg);
						}

						SplitString(pDeembeds, ",", single_Deembed);
					}
				}
			}
			else
			{
				snprintf(msg, sizeof(msg),
					"SET_INPUTPORT_DEEMB column not in SET file");

				StatusMessage(msg);
				WriteToLog(msg);
			}
		}
	}
	else
	{
		snprintf(msg, sizeof(msg),
			"SET_INPUT_PORT column not in SET file");

		StatusMessage(msg);
		WriteToLog(msg);
	}

	// check for output ports and its deembeds
	if (O_ExecutionTable.DoesColumnExist("SET_OUTPUT_PORT"))
	{
		int P2 = O_ExecutionTable.GetUniqueVals("SET_OUTPUT_PORT", largeStringVar, LARGE_STRING_VAR_SIZE);

		if (P2 && !strcmp(largeStringVar, "NA"))
		{
			StatusMessage("OUTPUT PORT is not being SET");
		}
		else
		{
			StatusMessage("OUTPUT PORT is being set to %s", largeStringVar);

			if (O_ExecutionTable.DoesColumnExist("SET_OUTPUTPORT_DEEMB"))
			{
				int D2 = O_ExecutionTable.GetUniqueVals("SET_OUTPUTPORT_DEEMB", largeStringVar, LARGE_STRING_VAR_SIZE);

				if (D2 && !strcmp(largeStringVar, "NA"))
				{
					StatusMessage("OUTPUT PORT DEEMBED is not being SET");
				}
				else if (P2 != D2)
				{
					snprintf(msg, sizeof(msg),
						"Number of OUTPUT ports is not equal to Number of OUTPUT Deembeds present");

					StatusMessage(msg);
					WriteToLog(msg);
				}
				else
				{
					StatusMessage("OUTPUT PORT DEEMB is being set to %s", largeStringVar);
					StatusMessage("Checking Deembed list against available locations...");

					pDeembeds = largeStringVar;
					single_Deembed[0] = '\0';

					SplitString(pDeembeds, ",", single_Deembed);

					while (single_Deembed[0])
					{
						bool exists = O_CRSF.FileExists(single_Deembed);

						snprintf(msg,
							sizeof(msg),
							"OUTPUT PORT DEEMB path %s is %svalid",
							single_Deembed,
							exists ? "" : "not ");

						StatusMessage(msg);

						if (!exists)
						{
							WriteToLog(msg);
						}

						SplitString(pDeembeds, ",", single_Deembed);
					}
				}
			}
			else
			{
				snprintf(msg, sizeof(msg),
					"SET_OUTPUTPORT_DEEMB column not in SET file");

				StatusMessage(msg);
				WriteToLog(msg);
			}
		}
	}
	else
	{
		snprintf(msg, sizeof(msg),
			"SET_OUTPUT_PORT column not in SET file");

		StatusMessage(msg);
		WriteToLog(msg);
	}

	// check for ISO ports
	if (O_ExecutionTable.DoesColumnExist("SET_ISOMEAS_PORT"))
	{
		int P3 = O_ExecutionTable.GetUniqueVals("SET_ISOMEAS_PORT", largeStringVar, LARGE_STRING_VAR_SIZE);

		if (P3 && !strcmp(largeStringVar, "NA"))
		{
			StatusMessage("ISO PORT is not being SET");
		}
		else
		{
			StatusMessage("ISO PORT is being set to %s", largeStringVar);
		}
	}
	else
	{
		snprintf(msg, sizeof(msg),
			"SET_ISOMEAS_PORT column not in SET file");

		StatusMessage(msg);
		WriteToLog(msg);
	}

	// check for CPL PORT DEEMBED
	if (O_ExecutionTable.DoesColumnExist("SET_CPLPORT_DEEMB"))
	{
		int D3 = O_ExecutionTable.GetUniqueVals("SET_CPLPORT_DEEMB", largeStringVar, LARGE_STRING_VAR_SIZE);

		if (D3 && !strcmp(largeStringVar, "NA"))
		{
			StatusMessage("CPL PORT Deembed is not being SET");
		}
		else
		{
			StatusMessage("CPL PORT DEEMBED is being set to %s", largeStringVar);
			StatusMessage("Checking Deembed list against available locations...");

			pDeembeds = largeStringVar;
			single_Deembed[0] = '\0';

			SplitString(pDeembeds, ",", single_Deembed);

			while (single_Deembed[0])
			{
				bool exists = O_CRSF.FileExists(single_Deembed);

				snprintf(msg,
					sizeof(msg),
					"CPL PORT DEEMB path %s is %svalid",
					single_Deembed,
					exists ? "" : "not ");

				StatusMessage(msg);

				if (!exists)
				{
					WriteToLog(msg);
				}

				SplitString(pDeembeds, ",", single_Deembed);
			}
		}
	}
	else
	{
		snprintf(msg, sizeof(msg),
			"SET_CPLPORT_DEEMB column not in SET file");

		StatusMessage(msg);
		WriteToLog(msg);
	}

	StatusMessage("Check complete");
}

void CheckForGainComp()
{
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	if (O_ExecutionTable.DoesColumnExist("SET_GAIN_COMP"))
	{
		int cnt = O_ExecutionTable.GetUniqueVals("SET_GAIN_COMP", largeStringVar, LARGE_STRING_VAR_SIZE);
		if (cnt && !strcmp(largeStringVar, "NA")) // SET_Gain_Comp column has NA
		{
			//StatusMessage("Gain Compression is not being SET");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Gain Compression is not being SET");
					
			StatusMessage(msg);
			WriteToLog(msg);
			
			
		}
		else if ((cnt == 1) && (strcmp(largeStringVar, "0") < 0))
		{
			//StatusMessage("Gain Compression being SET negative, GC = %s", largeStringVar);
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Gain Compression being SET negative, GC = %s", largeStringVar);
					
			StatusMessage(msg);
			WriteToLog(msg);
			
		}
		else if ((cnt == 1) && (strcmp(largeStringVar, "0") > 0))
		{
			//StatusMessage("Gain Compression being SET positive, GC = %s", largeStringVar);
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Gain Compression being SET positive, GC = %s", largeStringVar);
					
			StatusMessage(msg);
			WriteToLog(msg);
			
		}
		else if (cnt >= 1)
		{
			//StatusMessage("Multiple Values of GC are present, GC= %s,", largeStringVar);
			
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Multiple Values of GC are present, GC = %s", largeStringVar);
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
	}
	else
	{
		StatusMessage("SET_GAIN_COMP column not in SET file");
	}

	StatusMessage("Check complete");
}

void CheckForNAPowerSupplies()
{
	// Level1 documentation:-  if a defined power supply is not used in the tested mode of operation, the
	// set voltage value should be set to 0 to null out any noise current measurement for that supply.

	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	int cnt;

	if (O_ExecutionTable.DoesColumnExist("SET_VBATT"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VBATT", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VBATT suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VBATT suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VBATT suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VBIAS"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VBIAS", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VBIAS suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VBIAS suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VBIAS suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VCC_4G"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VCC_4G", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VCC_4G suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VCC_4G suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VCC_4G suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VCC_B20"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VCC_B20", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VCC_B20 suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VCC_B20 suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VCC_B20 suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VCC_2G"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VCC_2G", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VCC_2G suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VCC_2G suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VCC_2G suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VLNA"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VLNA", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VLNA suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VLNA suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VLNA suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VDD_LNA"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VDD_LNA", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VDD_LNA suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VDD_LNA suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VDD_LNA suppy is clear of NA's as being Set value");
	}

	if (O_ExecutionTable.DoesColumnExist("SET_VRAMP"))
	{
		cnt = O_ExecutionTable.GetUniqueVals("SET_VRAMP", largeStringVar, LARGE_STRING_VAR_SIZE);
		if ((cnt >= 1) && (strstr(largeStringVar, "NA")))
		{
			//StatusMessage("VRAMP suppy has NA as one of set values, please consider changing it to 0");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "VRAMP suppy has NA as one of set values, please consider changing it to 0");
					
			StatusMessage(msg);
			WriteToLog(msg);
		}
		else
			StatusMessage("VRAMP suppy is clear of NA's as being Set value");
	}

	StatusMessage("Check complete");
}

void CheckForTempOrder()
{
	// checks for random temp change order, random change in Temp. order can increase test time.

	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	char temp[10];
	char set_temp[500];
	char msg[50];
	int cnt;
	if (O_ExecutionTable.DoesColumnExist("SET_TEMP"))
	{
		// GetStringData("SET_TEMP", set_temp);
		int cnt = O_ExecutionTable.GetUniqueVals("SET_TEMP", largeStringVar, LARGE_STRING_VAR_SIZE);
		{
			if (cnt == 1 && !strcmp(largeStringVar, "NA"))
			{
				//StatusMessage("Temperature is not being set");
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Temperature is not being set");
					
			StatusMessage(msg);
			WriteToLog(msg);
			}
			else if ((cnt == 1) && (!strcmp(largeStringVar, "25") || !strcmp(largeStringVar, "-30") || !strcmp(largeStringVar, "85")))
			{
				//StatusMessage("Temperature being only set to %s degrees", largeStringVar);
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "Temperature being only set to %s degrees", largeStringVar);
					
			StatusMessage(msg);
			WriteToLog(msg);
			}

			if (cnt > 1) // here we have to check if the temp are in order, if it starts with 25 then all lines should have 25 till temp. changes...no jumbled temp.
			{
				// char nums[550001];
				// int mainnums = O_ExecutionTable.GetUniqueVals("OFFSET_FROM_START_OF_TEST", nums, 550000);

				int ExecStatus = O_ExecutionTable.SetCurrentRow(0);
				int i = 0, Ref = 0, act = 0, Temp;
				while (ExecStatus > -1)
				{   
			        AllowAbort();
					Temp = O_ExecutionTable.GetNumericCell("SET_TEMP");
					if (i > 0 && Ref != Temp)
						act++;
					Ref = Temp;
					ExecStatus = O_ExecutionTable.IncrementRow();
					i++;
				}
				if (act == cnt - 1)
				{
					StatusMessage("Temperature Order is correct");
				}
				if (act > cnt - 1)
				{
					// ruggedness is other case where temp order will not be correct
					//StatusMessage("Temperature Order is not correct");
				char msg[1024];
					
				snprintf(msg, sizeof(msg), "Temperature Order is not correct");
					
				StatusMessage(msg);
				WriteToLog(msg);
				    
				}
			}
		}
	}
	else
	{
		StatusMessage("SET_TEMP column not in SET file");
	}

	StatusMessage("Check complete");
}

void CheckForBandFreqRange()
{
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	if (O_ExecutionTable.DoesColumnExist("SET_BAND"))
	{
		StatusMessage("Starting check for freq range for the Tx bands present in the SET file");

		char band[50];
		double freq;
		int Main;
		char G_Main[100];
		char G_STR[500];
		bool Temp_Band, Temp_Main;
		int Band;

		int ExecStatus = O_ExecutionTable.SetCurrentRow(0);
		while (ExecStatus > -1)
		{
			G_STR[0] = '\0';
			G_Main[0] = '\0';
            AllowAbort();
			Temp_Band = O_ExecutionTable.GetValFromArray("SET_BAND", &Band, G_STR);
			freq = O_ExecutionTable.GetNumericCell("SET_FREQ");
			Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);

			if (!strcmp(G_STR, "B12"))
			{
				if (!(freq >=  699 && freq <= 716))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B13"))
			{
				if (!(freq >= 777 && freq <= 787))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B14"))
			{
				if (!(freq >= 788 && freq <= 798))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B28A"))
			{
				if (!(freq >= 703 && freq <= 733))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B28B"))
			{
				if (!(freq >= 718 && freq <= 748))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (strstr(G_STR, "B20"))
			{
				if (!(freq >= 832 && freq <= 862))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B26"))
			{
				if (!(freq >= 814 && freq <= 849))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B71"))
			{
				if (!(freq >= 663 && freq <= 698))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B8"))
			{
				if (!(freq >= 880 && freq <= 915))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			else if (!strcmp(G_STR, "B28"))
			{
				if (!(freq >= 703 && freq <= 748))
				{
					//StatusMessage("Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "Tx Band Freq is not correct, Band = %s, Main = %s", G_STR, G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}
			}
			ExecStatus = O_ExecutionTable.IncrementRow();
		}

		StatusMessage("Ending Freq range check");
	}
	else
	{
		StatusMessage("SET_BAND column not in SET file");
	}

	StatusMessage("Check complete");
}

void CheckForVSWRandPhase()
{
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	char localStringVar[5000] = "";

	if ((O_ExecutionTable.DoesColumnExist("SET_VSWR") && O_ExecutionTable.DoesColumnExist("SET_PHASE")))
	{   AllowAbort();
		int cnt1 = O_ExecutionTable.GetUniqueVals("SET_VSWR", largeStringVar, LARGE_STRING_VAR_SIZE);
		int cnt2 = O_ExecutionTable.GetUniqueVals("SET_PHASE", localStringVar, sizeof(localStringVar) - 1);

		if (cnt1 && cnt2 && !strcmp(largeStringVar, "NA") && !strcmp(localStringVar, "NA"))
		{
			StatusMessage("VSWR and phase are not being set");
		}

		if (cnt1 > 1 && cnt2 > 1)
		{
			StatusMessage("Unique VSWR values are, VSWR = %s", largeStringVar);
			StatusMessage("Unique Phase values are, Phase = %s", localStringVar);

			largeStringVar[0] = '\0';
			localStringVar[0] = '\0';
			// check if pgen is being used instead of pout in mismatch/ruggedness test.
			int cnt3 = O_ExecutionTable.GetUniqueVals("SET_PGEN", largeStringVar, LARGE_STRING_VAR_SIZE);
			int cnt4 = O_ExecutionTable.GetUniqueVals("SET_POUT", localStringVar, sizeof(localStringVar) - 1);

			if ((cnt3 == 1 || cnt4 > 1) && (!strcmp(largeStringVar, "NA") || strcmp(localStringVar, "NA")))
			{
				StatusMessage("SET_PGEN is not being set instead SET_POUT is being set, please check");
			}

			// check if mismatch/ruggedness test is open loop or closed loop.
			int cnt5 = O_ExecutionTable.GetUniqueVals("OFFSET_FROM_START_OF_TEST", largeStringVar, LARGE_STRING_VAR_SIZE);
			if ((cnt5 >= 1) && (strstr(largeStringVar, "CAL")))
			{
				StatusMessage("Test is being set as for closed loop, LUT will be recalled");
			}
			else
				StatusMessage("Test is being set as for open loop, LUT will not be recalled");
		}
	}
	else
	{
		StatusMessage("SET_VSWR or SET_PHASE columns not in SET file");
	}

	StatusMessage("Check complete");
}

void CheckForMIPIWords()
{
	

	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	bool MIPISTAT;
	bool Temp_Main;

	int Main;
	char G_Main[100];

	int ExecStatus;
	int MS;
	char g_str[1000];

	// check for Mipi Static
	if (O_ExecutionTable.DoesColumnExist("SET_MIPI_STATIC"))
	{
		int cnt1 = O_ExecutionTable.GetUniqueVals("SET_MIPI_STATIC", largeStringVar, LARGE_STRING_VAR_SIZE);
		if (cnt1 > 1)
		{
			StatusMessage("Starting check for SET_MIPI_STATIC");

			ExecStatus = O_ExecutionTable.SetCurrentRow(0);
			while (ExecStatus > -1)
			{
				g_str[0] = '\0';
				G_Main[0] = '\0';
				MIPISTAT = O_ExecutionTable.GetValFromArray("SET_MIPI_STATIC", &MS, g_str);
				Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);
                AllowAbort();
				int count;
				int i = 0, j = 0;
				int len = strlen(g_str);

				for (count = 0; count < len; count++)
				{
					if (g_str[count] == '@')
						i++;

					if (g_str[count] == ':')
						j++;
				}
				if (j != i - 1 && !strstr(g_str, "NA"))
				{
					char msg[1024];
					
					snprintf(msg, sizeof(msg),"SET MIPI STATIC format in Main %s is not correct",G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);

				}

				ExecStatus = O_ExecutionTable.IncrementRow();
			}

			StatusMessage("Finishing check for SET_MIPI_STATIC");
		}
	}

	// check for Mipi Up
	if (O_ExecutionTable.DoesColumnExist("SET_MIPI_UP"))
	{
		int cnt2 = O_ExecutionTable.GetUniqueVals("SET_MIPI_UP", largeStringVar, LARGE_STRING_VAR_SIZE);
		if (cnt2 > 1)
		{
			StatusMessage("Starting check for SET_MIPI_UP");

			ExecStatus = O_ExecutionTable.SetCurrentRow(0);
			while (ExecStatus > -1)
			{
				G_Main[0] = '\0';
				g_str[0] = '\0';
				MIPISTAT = O_ExecutionTable.GetValFromArray("SET_MIPI_UP", &MS, g_str);
				Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);
				
                AllowAbort();
				int count;
				int i = 0, j = 0;
				int len = strlen(g_str);

				for (count = 0; count < len; count++)
				{
					if (g_str[count] == '@')
						i++;

					if (g_str[count] == ':')
						j++;
				}
				if (j != i - 1 && !strstr(g_str, "NA"))
				{
					//StatusMessage("SET MIPI UP format in Main: %s is not correct", G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "SET MIPI UP format in Main %s is not correct", G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
			
				}

				ExecStatus = O_ExecutionTable.IncrementRow();
				//check for Multiple MIPI buses in MIPI_UP, related to seperate code
				CheckMIPIBus(g_str, "SET_MIPI_UP", G_Main);
			}

			StatusMessage("Finishing check for SET_MIPI_UP");
		}
	}

	// Check for Mipi Down
	if (O_ExecutionTable.DoesColumnExist("SET_MIPI_DOWN"))
	{
		int cnt3 = O_ExecutionTable.GetUniqueVals("SET_MIPI_DOWN", largeStringVar, LARGE_STRING_VAR_SIZE);
		if (cnt3 > 1)
		{
			StatusMessage("Starting check for SET_MIPI_DOWN");

			ExecStatus = O_ExecutionTable.SetCurrentRow(0);
			while (ExecStatus > -1)
			{
				G_Main[0] = '\0';
				g_str[0] = '\0';
				MIPISTAT = O_ExecutionTable.GetValFromArray("SET_MIPI_DOWN", &MS, g_str);
				Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);
                AllowAbort();
				int count;
				int i = 0, j = 0;
				int len = strlen(g_str);

				for (count = 0; count < len; count++)
				{
					if (g_str[count] == '@')
						i++;

					if (g_str[count] == ':')
						j++;
				}
				if (j != i - 1 && !strstr(g_str, "NA"))
				{
					//StatusMessage("SET MIPI DOWN format in Main: %s is not correct", G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "SET MIPI DOWN format in Main %s is not correct", G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
			
				}

				ExecStatus = O_ExecutionTable.IncrementRow();
				//check for Multiple MIPI buses in MIPI_DOWN, related to seperate code
				CheckMIPIBus(g_str, "SET_MIPI_DOWN", G_Main);
			}

			StatusMessage("Finishing check for SET_MIPI_DOWN");
		}
	}

	// Check for MIPI LNA
	if (O_ExecutionTable.DoesColumnExist("SET_MIPI_LNA"))
	{
		int cnt4 = O_ExecutionTable.GetUniqueVals("SET_MIPI_LNA", largeStringVar, LARGE_STRING_VAR_SIZE);
		if (cnt4 > 1)
		{
			StatusMessage("Starting check for SET_MIPI_LNA");

			ExecStatus = O_ExecutionTable.SetCurrentRow(0);
			while (ExecStatus > -1)
			{
				G_Main[0] = '\0';
				g_str[0] = '\0';
				MIPISTAT = O_ExecutionTable.GetValFromArray("SET_MIPI_LNA", &MS, g_str);
				Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);
                AllowAbort();
				int count;
				int i = 0, j = 0;
				int len = strlen(g_str);

				for (count = 0; count < len; count++)
				{
					if (g_str[count] == '@')
						i++;

					if (g_str[count] == ':')
						j++;
				}
				if (j != i - 1 && !strstr(g_str, "NA"))
				{
					//StatusMessage("SET MIPI LNA format in Main: %s is not correct", G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "SET MIPI LNA format in Main %s is not correct", G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
			
				}

				ExecStatus = O_ExecutionTable.IncrementRow();
			}

			StatusMessage("Finishing check for SET_MIPI_LNA");
		}
	}

	// Check for MIPI FE
	if (O_ExecutionTable.DoesColumnExist("SET_MIPI_FE"))
	{
		int cnt5 = O_ExecutionTable.GetUniqueVals("SET_MIPI_FE", largeStringVar, LARGE_STRING_VAR_SIZE);
		if (cnt5 > 1)
		{
			StatusMessage("Starting check for SET_MIPI_FE");

			ExecStatus = O_ExecutionTable.SetCurrentRow(0);
			while (ExecStatus > -1)
			{
				G_Main[0] = '\0';
				g_str[0] = '\0';
				MIPISTAT = O_ExecutionTable.GetValFromArray("SET_MIPI_FE", &MS, g_str);
				Temp_Main = O_ExecutionTable.GetValFromArray("OFFSET_FROM_START_OF_TEST", &Main, G_Main);
                AllowAbort();
				int count;
				int i = 0, j = 0;
				int len = strlen(g_str);

				for (count = 0; count < len; count++)
				{
					if (g_str[count] == '@')
						i++;

					if (g_str[count] == ':')
						j++;
				}
				if (j != i - 1 && !strstr(g_str, "NA"))
				{
					//StatusMessage("SET MIPI FE format in Main: %s is not correct", G_Main);
					char msg[1024];
					
					snprintf(msg, sizeof(msg), "SET MIPI FE format in Main %s is not correct", G_Main);
					
					StatusMessage(msg);
					WriteToLog(msg);
				}

				ExecStatus = O_ExecutionTable.IncrementRow();
			}

			StatusMessage("Finishing check for SET_MIPI_FE");
		}
	}

	StatusMessage("Check complete");
}

void CheckSetFileAgainstPrm()
{
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	char *localLargeStringVar = new char[LARGE_STRING_VAR_SIZE + 1];
	if (!localLargeStringVar)
	{
		StatusMessage("Cannot reserve memory for verification, will bypass this check");
		MsgBox("CheckSetFileAgainstPrm: Cannot reserve memory for verification, will bypass this check", WARNINGOKONLY, false);
		StatusMessage("Check bypassed");
		return;
	}

	GetParameterNames(largeStringVar);
	char *allPrmNames = largeStringVar;

	O_ExecutionTable.GetAllSetPrms(localLargeStringVar, LARGE_STRING_VAR_SIZE);
	char *allSetPrms = localLargeStringVar;

	char aItem[1000] = "";
	SplitString(allSetPrms, ",", aItem);
	while (aItem[0])
	{
		if (!SearchCSVForMatch(allPrmNames, aItem))
		{   
			AllowAbort();
			//StatusMessage("SET file contains '%s', but missing from prm file\nCheck that SET file was generated using the prm file that is loaded", aItem);
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "SET file contains '%s', but missing from prm file\nCheck that SET file was generated using the prm file that is loaded", aItem);
					
			StatusMessage(msg);
			WriteToLog(msg);
			
			if (localLargeStringVar)
			{
				delete localLargeStringVar;
				localLargeStringVar = NULL;
			}
			StatusMessage("Check complete");
			return;
		}
		SplitString(allSetPrms, ",", aItem);
	}

	O_ExecutionTable.GetAllMeasPrms(localLargeStringVar, LARGE_STRING_VAR_SIZE);
	char *allMeasPrms = localLargeStringVar;

	aItem[0] = '\0';
	SplitString(allMeasPrms, ",", aItem);
	while (aItem[0])
	{
		if (!SearchCSVForMatch(allPrmNames, aItem))
		{
			//StatusMessage("SET file contains '%s', but missing from prm file\nCheck that SET file was generated using the prm file that is loaded", aItem);
			char msg[1024];
					
			snprintf(msg, sizeof(msg), "SET file contains '%s', but missing from prm file\nCheck that SET file was generated using the prm file that is loaded", aItem);
					
			StatusMessage(msg);
			WriteToLog(msg);
			if (localLargeStringVar)
			{
				delete localLargeStringVar;
				localLargeStringVar = NULL;
			}
			StatusMessage("Check complete");
			return;
		}
		SplitString(allMeasPrms, ",", aItem);
	}

	if (localLargeStringVar)
	{
		delete localLargeStringVar;
		localLargeStringVar = NULL;
	}

	StatusMessage("Check complete");
}

void CheckForMeasParams()
{
	if (!largeStringVar)
	{
		return;
	}

	largeStringVar[0] = '\0';

	StatusMessage(">>> %s: Executing...", __func__);

	char *localLargeStringVar = NULL;

	O_ExecutionTable.GetAllMeasPrms(largeStringVar, LARGE_STRING_VAR_SIZE);

	if (largeStringVar[0])
	{
		localLargeStringVar = new char[LARGE_STRING_VAR_SIZE + 1];
		if (!localLargeStringVar)
		{
			StatusMessage("Cannot reserve memory for verification, will bypass this check");
			MsgBox("CheckForMeasParams: Cannot reserve memory for verification, will bypass this check", WARNINGOKONLY, false);
			StatusMessage("Check bypassed");
			return;
		}
	}

	char *allMeasPrms = largeStringVar;

	char aItem[1000] = "";
	SplitString(allMeasPrms, ",", aItem);
	while (aItem[0])
	{
		if (O_ExecutionTable.DoesColumnExist(aItem))
		{
			AllowAbort();
			localLargeStringVar[0] = '\0';
			int uniqueParam = O_ExecutionTable.GetUniqueVals(aItem, localLargeStringVar, LARGE_STRING_VAR_SIZE);
			if (uniqueParam > 1)
			{
				StatusMessage("Parameter %s is being measured", aItem);
			}
		}
		// nextparam:
		SplitString(allMeasPrms, ",", aItem);
	}

	StatusMessage("Check complete");
}

void ClearWaveformFolder()
{
	StatusMessage(">>> %s: Executing...", __func__);

	char *full_wfm_path = "C:\\Resources\\Waveforms";
	bool directoryExists = O_CRSF.DirectoryExists(full_wfm_path, true);

	if (directoryExists && MsgBox2("Do you want to Delete all Waveforms in local folder?", YESNO, 10, false) != NO)
	{
		DIR *dir;
		struct dirent *ent;
		dir = opendir(full_wfm_path);
		char tmp_ent[1024];
		char file[1024];

		while ((ent = readdir(dir)) != NULL)
		{
			strcpy(tmp_ent, ent->d_name);
			if (tmp_ent[0] != '.')
			{
				// use local copy of the entry and modify it only
				sprintf(file, "%s\\%s", full_wfm_path, tmp_ent);
				remove(file);
			}
		}
	}

	StatusMessage("Check complete");
}

double GetNumberFromString(char *stringdata)
{
	// function will return -1 if number does not exist at the start/end of the string passed in
	int idx;

	int len = strlen(stringdata);
	int idx2;
	for (idx = 0; idx < len; idx++)
	{
		if (isdigit(stringdata[idx]))
			break;
	}
	idx2 = idx + 1;
	for (idx2; idx2 < 12; idx2++) // 12 - based on average length of Pulsed CW waveforms
	{
		if (isdigit(stringdata[idx2]))
			break;
	}

	int number1 = -1, number2 = -1;
	double dec;
	if (idx < 12) // first index is not exceding the splitting limit, if exceding code will not work
		number1 = atoi(stringdata + idx);
	number2 = atoi(stringdata + idx2);
	dec = 0.1 * number2 + number1;

	return dec;
}


void CheckMIPIBus(char *mipi_command_list, char *column_name, char *main)
{
	char *tmp_command_list = mipi_command_list;

	char busList[100] = "";
	char mipi_word[100] = "";
	char bus_string[100] = "";

	SplitString(tmp_command_list, ":", mipi_word);
	while (mipi_word[0])
	{
		int BusNum = GetMIPIBus(mipi_word, mipi_word);
		sprintf(bus_string, "%i", BusNum);

		if (!StrStr(busList, bus_string))
		{
			if (busList[0] != 0)
				strcat(busList, ",");
			strcat(busList, bus_string);
		}

		SplitString(tmp_command_list, ":", mipi_word);
	}

	// if the list contains a comma, then there was more than one bus defined for the parameter column
	if (StrStr(busList, ","))
	{
		//StatusMessage("%s contain more than one bus definition at %s", column_name, main);
		char msg[1024];
					
		snprintf(msg, sizeof(msg), "%s contain more than one bus definition at %s", column_name, main);
					
		StatusMessage(msg);
		WriteToLog(msg);
		
	}
}


int GetMIPIBus(char *WordIn, char *WordOut)
{
	int busNum = 1;
	if (WordIn[1] == '@' || WordIn[1] == '#')
	{
		busNum = atoi(WordIn);
		if (WordOut != WordIn)
			strcpy(WordOut, WordIn + 2);
		else
		{
			char temp[256] = "";
			strcpy(temp, WordIn + 2);
			strcpy(WordOut, temp);
		}
	}
	else
	{
		if (WordOut != WordIn)
			strcpy(WordOut, WordIn);
	}

	if (busNum > 4)
	{
		//StatusMessage("Bus Number %i in WordIn: %s is greater than 4!", busNum, WordIn);
		char msg[1024];
					
		snprintf(msg, sizeof(msg), "Bus Number %i in WordIn: %s is greater than 4!", busNum, WordIn);
					
		StatusMessage(msg);
		WriteToLog(msg);
	}

	return busNum;
}
