#enter the freq and units
Freq = input("Enter the frequency with units like 1-GHZ or 5.5-MHZ: ") # "-" is used as a deliminiter here
if "-" not in Freq:
    print("Enter the frequency in proper format as requested")
else:
    Freq_units = (Freq[Freq.find("-")+1:]) #+1 as we need plus one character to slice accurately
    FREQ_UNITS = Freq_units.upper()
    FREQ = float(Freq[:Freq.find("-")])

    if(FREQ_UNITS == "GHZ"):
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**3} MHZ")
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**6} KHZ")
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**9} HZ")

    elif(FREQ_UNITS == "MHZ"):
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**-3} GHZ")
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**3} KHZ")
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**6} HZ")

    elif(FREQ_UNITS == "HZ"):
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**-9} GHZ")
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**-6} MHZ")
        print(f"{FREQ} {FREQ_UNITS} is equal to {FREQ*10**-3} KHZ")