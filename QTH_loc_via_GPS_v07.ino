/* GPS display software version 1.1A special of 10 december 2015. LCD 20 x 4 and lon and lat display on rows 3&4
The GPS display is an Arduino version of the original Roverbox of ON4IY.
Ideas and part of the code are taken from the internet e.g.from ON4IY, ON7EQ, K3NG etc.
The astronomical algorithms used are from Jean Meeus.
Most of the c code for calculating the solar position is taken from David Brooks ( http://www.instesre.org/ArduinoUnoSolarCalculations.pdf ).
Comments are welcome. ON4CDU (at) uba.be
--------------------------------------
Original design found in VERON Electron, November 2015 by Hans Wagemans, ON4CDU (all credits !)
Modified for showing coordinates on LCD by ON4CDU for PE1PIC
Modified by PA0GTB for showing unambiguous coordinates all time, structured the code and add comments  ( May 2017)
If you want: Connect external LED via PIN 13 via 470 Ohm to ground
--------------------------------------
*/

//  
#include <LiquidCrystal.h> // include the liquid crystal library code
LiquidCrystal lcd(12,11,10,9,8,7);// initialize the library with the numbers of the interface pins

// bitrate GPS receiver (type depended)
const int GPSbitrate = 9600; 

#define HEADER_RMC "$GPRMC"
#define HEADER_GGA "$GPGGA"
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define PI 3.1415926535897932384626433832795
#define TWOPI 6.28318531

// NMEA fields
#define RMC_TIME 1
#define RMC_STATUS 2
#define RMC_LAT 3
#define RMC_NS 4
#define RMC_LONG 5
#define RMC_EW 6
#define RMC_DATE 9
#define GGA_FIX 6
#define GGA_SATS 7
#define GGA_HDOP 8
#define GGA_ALTITUDE 9

const int pinled = 13;
const int EOled = 8; 
const int sentenceSize = 82;
const int max_nr_words = 20;

char sentence[sentenceSize], sz[20],regel[20];
char* words [max_nr_words];
char EO, sts, latdir, longdir;
char char_string [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char num_string[]= "0123456789";

int alternate;
int hours, minutes, seconds, year, yeard, month, day;
int gps_fixval, gps_sats, alti, hdopi;
int latdeg, latmin, latsec;
int londeg, lonmin, lonsec;

float time, date, latf, lonf, hdopf;
boolean status, gps_fix, hdop;

//some variables for sun position calculation
float Lon, Lat;
float T,JD_frac,L0,M,e,C,L_true,f,R,GrHrAngle,Obl,RA,Decl,HrAngle,elev,azimuth;
long JD_whole,JDx;
int azd, eld; // rounded values for displaying

void setup()
{
  Serial.begin(GPSbitrate);
  pinMode (pinled, OUTPUT);
  pinMode (EOled, OUTPUT);
  
  // set up the LCD's number of columns and rows and initial text
  lcd.begin(20, 4); 
  lcd.setCursor(0,2); lcd.print (" Wacht op GPS Data "); // Aangepast Pe1PIC ==
  lcd.setCursor(0,3); lcd.print ("     - PA0GTB -    "); // Aangepast Pe1PIC ==
  
  lcd.setCursor (0, 0);
  digitalWrite (EOled, LOW); // EO led off
  lcd.print ("GPS NIET aangesloten");
}

void loop()  // Begin of Loop
{
  digitalWrite (pinled, LOW);
  static int i = 0;
  if (Serial.available())
  {
    digitalWrite (pinled, HIGH); 
    char ch = Serial.read();
    if (ch != '\n' && i < sentenceSize)
    {
      sentence[i] = ch;
      i++;
    }
    else
    {
      sentence[i] = '\0';
      i = 0;
      displayGPS();
    }
  }
}

void displayGPS()
{
  getField ();
  if (strcmp(words [0], HEADER_RMC) == 0)
  {
    processRMCstring();
    printfirstrow ();
    locator();
  }
  else if (strcmp(words[0], HEADER_GGA) == 0)
  {
    processGGAstring ();
  }
  printGGAinfo ();
}

void getField () // split string was probably a better name
{
  char* p_input = &sentence[0];
  //words [0] = strsep (&p_input, ",");
  for (int i=0; i<max_nr_words; ++i)
  {
    words [i] = strsep (&p_input, ",");
  }
}

void processRMCstring ()
{
    time = atof(words [RMC_TIME]);
    hours = (int)(time * 0.0001);
    minutes = int((time * 0.01) - (hours * 100));
    seconds = int(time - (hours * 10000) - (minutes * 100));

    if (minutes % 2 > 0)
    {
      EO = 'O';
      digitalWrite (EOled, LOW); // EO led off
    }
    else
    {
      EO = 'E';
      digitalWrite (EOled, HIGH); // EO led on
    }

    sts = words[RMC_STATUS][0]; // status
    status = (sts == 'A');
 
    latf = atof(words [RMC_LAT]); // latitude
 
    latdeg = latf/100;
    latmin = (latf - (int(latf/100))*100);
    latsec = (((latf -int(latf))*100)*0.6);
 
    latf = int (latf * 0.01) + (((latf * 0.01) - int(latf * 0.01)) / 0.6);
    

    latdir = words[RMC_NS][0]; // N/S
    
    lonf= atof(words [RMC_LONG]);// longitude
  
    londeg = lonf/100;
    lonmin = (lonf - (int(lonf/100))*100);
    lonsec = (((lonf -int(lonf))*100)*0.6);

    lonf = int (lonf * 0.01) + ((lonf * 0.01 - int(lonf * 0.01)) / 0.6);

    longdir = words[RMC_EW][0]; // E/W
       
    date = atof(words [RMC_DATE]); // date
    day = int(date * 0.0001);
    month = int ((date * 0.01) - (day * 100));
    year = int (date - (day * 10000) - (month * 100));

    // Showing year in 4 digits
    yeard = year;
    year = year + 2000;  
}

void processGGAstring ()
{
    gps_fixval = words[GGA_FIX][0] - 48;
    gps_fix = (gps_fixval > 0);

    gps_sats = atoi(words[GGA_SATS]);

    hdopf = atof (words[GGA_HDOP]);
    if (hdopf > 1) 
    {
      hdopi = 2;
      hdop = true;
    }
    else if (hdopf > 2) {
      hdopi = 3;
      hdop = false;
    }
    else {
      hdopi = 1;
      hdop = true;
    }
    
    alti = atoi(words[GGA_ALTITUDE]); 
}

void printfirstrow ()
{
    sprintf(sz, "%02d/%02d/%02d  %02d:%02d:%02d",
            day, month , year , hours , minutes , seconds ); // Aangepast Pe1PIC ==
    lcd.setCursor(0, 0);
    lcd.print(sz); // print time, EO and date string on LCD  
}

void printGGAinfo ()
{
  if (seconds == 30)
  {
  // sunpos();  // If you don't want the Sunset, just block this line
  }
  if (status && gps_fix && hdop)
  {
    alternate = seconds / 20;
    
    switch (alternate)
    {
      case 0:
      {
        sprintf (sz, " ! %2d Sat" , gps_sats);
        sprintf(regel, "Lat:  %2d %2d %2d %2c ", latdeg, latmin , latsec ,latdir);
        lcd.setCursor(0, 2); lcd.print(regel); // latitude string on LCD
        lcd.setCursor(8, 2); lcd.print((char)223); lcd.setCursor(11, 2); lcd.print("'");  // Print Graden - Minuten  symbool // Aangepast Pe1PIC ==
        sprintf(regel, "Lon:  %2d %2d %2d %2c ", londeg, lonmin , lonsec ,longdir);
        lcd.setCursor(0, 3); lcd.print(regel); // longitude string on LCD 
        lcd.setCursor(8,3); lcd.print((char)223); lcd.setCursor(11, 3); lcd.print("'");  // Print Graden - Minuten  symbool // Aangepast Pe1PIC == 
        break;
      }
      case 1:
      {
        sprintf (sz, "!  HDOP %1d" , hdopi); // positie aangepast Pe1PIC ==
        break;
      }
      case 2:
      {
        sprintf (sz, "!   %3dm " , alti); // positie aangepast Pe1PIC ==
        break;
      }
      case 3:
      {
        sprintf (sz, "  %3d/%3d" , azd,eld); // positie aangepast Pe1PIC ==
        break;
      }
    }
  }
  else
  {
    alternate = (seconds % 2);
    if (0 == alternate)
    {
      sprintf (sz, "? %2d Sat" , gps_sats); // Aangepast Pe1PIC ==
    }
    else
    {
      sprintf (sz, "  %2d Sat" , gps_sats); // Aangepast Pe1PIC ==
    }
  }
  
  lcd.setCursor(11, 1); // Aangepast Pe1PIC ==
  lcd.print (sz);
    
}

  
void locator() // locator calculation and display
{
  float loclong = lonf * 1000000;
  float loclat = latf * 1000000;
  float scrap;


  if (longdir == 'E') 
  {
    loclong = (loclong) + 180000000;
  }
  if (longdir == 'W') 
  {
    loclong = 180000000 - (loclong);
  }

  if (latdir == 'N') 
  {
    loclat = loclat + 90000000;
  }
  if (latdir == 'S') 
  {
    loclat = 90000000 - loclat;
  }

  lcd.setCursor(0, 1);lcd.print ("Loc:"); // printing QTH locator // Aangepast Pe1PIC ==

  // First Character - longitude based (every 20° = 1 gridsq)
  lcd.print(char_string[int(loclong / 20000000)]);

  // Second Character - latitude based (every 10° = 1 gridsq)
  lcd.print(char_string[int(loclat / 10000000)]);

  // Third Character - longitude based (every 2° = 1 gridsq)
  scrap = loclong - (20000000 * int(loclong / 20000000));
  lcd.print(num_string[int(scrap * 10 / 20 / 1000000)]);

  // Fourth Character - latitude based (every 1° = 1 gridsq)
  scrap = loclat - (10000000 * int(loclat / 10000000));
  lcd.print(num_string[int(scrap / 1000000)]);

  // Fifth Character - longitude based (every 5' = 1 gridsq)
  scrap = (loclong / 2000000) - (int(loclong / 2000000));
  lcd.print(char_string[int(scrap * 24)]);

  // Sixth Character - longitude based (every 2.5' = 1 gridsq)
  scrap = (loclat / 1000000) - (int(loclat / 1000000));
  lcd.print(char_string[int(scrap * 24)]);
}

void sunpos()
{
int A, B;
    
    Lat = latf*DEG_TO_RAD;
    if (latdir == 'S')
    {
      Lat = -Lat;
    } 
    Lon = lonf*DEG_TO_RAD;
    if (longdir == 'W')
    {
      Lon = -Lon;
    }

    if(month <= 2) 
    { 
      year--;
      month+=12;
    }  
    A = year/100; 
    B = 2 - A + A/4;
    JD_whole = (long)(365.25*(year + 4716)) + (int)(30.6001 * (month+1)) + day + B - 1524;
    JD_frac=(hours + minutes/60. + seconds/3600.)/24. - .5;
    T = JD_whole - 2451545; 
    T = (T + JD_frac)/36525.;
    L0 = DEG_TO_RAD * fmod(280.46645 + 36000.76983 * T,360);
    M = DEG_TO_RAD * fmod(357.5291 + 35999.0503 * T,360);
    e = 0.016708617 - 0.000042037 * T;
    C=DEG_TO_RAD*((1.9146 - 0.004847 * T) * sin(M) + (0.019993 - 0.000101 * T) * sin(2 * M) + 0.00029 * sin(3 * M));
    f = M + C;
    Obl = DEG_TO_RAD * (23 + 26/60. + 21.448/3600. - 46.815/3600 * T);     
    JDx = JD_whole - 2451545;  
    GrHrAngle = 280.46061837 + (360 * JDx)%360 + .98564736629 * JDx + 360.98564736629 * JD_frac;
    GrHrAngle = fmod(GrHrAngle,360.);    
    L_true = fmod(C + L0,TWOPI);
    R=1.000001018 * (1 - e * e)/(1 + e * cos(f));
    RA = atan2(sin(L_true) * cos(Obl),cos(L_true));
    Decl = asin(sin(Obl) * sin(L_true));
    HrAngle = DEG_TO_RAD * GrHrAngle + Lon - RA;
    elev = asin(sin(Lat) * sin(Decl) + cos(Lat) * (cos(Decl) * cos(HrAngle)));
    azimuth = PI + atan2(sin(HrAngle),cos(HrAngle) * sin(Lat) - tan(Decl) * cos(Lat));
    eld = round (elev/DEG_TO_RAD);
    azd = round (azimuth/DEG_TO_RAD); 
}

