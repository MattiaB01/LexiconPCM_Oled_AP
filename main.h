/* Pin utilizzati su scheda Heltec ESP32*/

#ifndef main.h
#define main.h

/* pin busy e datawrite*/
#define DISPBSY 5

#define DISPDWR 17


/*pin utilizzati su Esp32 per il busdati da Lexicon*/
/* D0..................D7*/    			
const int databus[8] = {36,39,34,35,26,27,14,13};

// Globals
int mode=0;
int data=0;
int position=0;
char character[40];

void NewDataInterrupt();
void ParseData();


#endif /* main.h */
