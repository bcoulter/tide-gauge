
/*
 Receives data from NMEA output from internal GPS on UART 1 and stand alone Sounder on @UART 2
 Version 1604 Gets date and time from NMEA $GPRMC sentence e.g.
 Version 1606 Configured for RGB tri-LED
 Version 1610 as 1606 but dual input
 $GPRMC,135448,A,5220.7762,N,00628.2872,W,0.00,0.00,020516,3.87,W,A*2A

 The circuit:
 * Power on is digital pin 7
 * RTC/SD begin is digital pin 5
 * Sentence 1 save is digital pin 6
 * Sentence 2 save is digital pin 4
 */
#include <MemoryFree.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

const uint8_t SD_CS = 53; // SD chip select - 53 for Mega
RTC_DS1307 RTC;  // define the Real Time Clock object

// Open serial communications and wait for port to open:
//------------------------------------------------------------------------------
// call back for file timestamps
void dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = RTC.now();

  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(now.year(), now.month(), now.day());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(now.hour(), now.minute(), now.second());

}
//------------------------------------------------------------------------------

int errCode, charCount, charCount2 = 0;
String dataString, dataString2, dataStringDepth = "";
int NMEA_start, NMEA_count, NMEA_last = 0;
int NMEA_start2, NMEA_count2, NMEA_last2 = 0;
char NMEA_fini[7] = ""; //"$GPRTE"
char NMEA_finis[7] = ""; //"$GPRTE"
char NMEA_type[7] = "";
char NMEA_type2[7] = "";
char NMEA_rmc[7] = "$DBRMC";  // Sentence from COM1 meaning recemmended minimum navigation
char NMEA_dbt[7] = "$SDDBT";  // Sentence from COM2 meaning depth below transducer
char DBS[5] = "";      // Depth of transducer below surface
char SD_file[11] = "config.txt";
char baudrate[7] = "";
char inputRead, inputRead2 = 0;
char fileName[13] = "";
char fileNam2[13] = "";
char sentence[60] = "";
char csvField[7] = "";
char dateTimeUT[13] = "";
char colour = 0;

int csvValid, gotDate = 0;
int yrI, moI, daI, hrI, miI, seI = 0;
char NMEA_dateTime[7] = "";
long dataPrev = -1;  // Pointer to file in SD write
int firstRec, firstRec2 = true; // Flag so depth below surface is written to fileNam2 instead of first NMEA paragraph
long int timer = 0;
long int dely = 0;
// Start Setup

void setup() {
  // initialize digital pins 4 - 7 as outputs
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);   // turn the LED 7 to show power OK
  Serial.begin(57600);
  Wire.begin();
  RTC.begin();
  digitalWrite(5, LOW);   // turn off LED 5
  if (!RTC.begin()) {
    //   Serial.println("RTC failed");
    digitalWrite(5, HIGH);   // turn on LED 5 to show RTC failure
    digitalWrite(7, LOW);   // turn off the LED 7 to leave red light burning alone
    while (1);               // dont do anything else
  }
  else {
    //  Serial.println("RTC started");
  }
  // set date time callback function
  SdFile::dateTimeCallback(dateTime);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD.begin failed");
    digitalWrite(5, HIGH);   // turn on LED 5 to show SD failure
    digitalWrite(7, LOW);   // turn off the LED 7 to leave red light burning alone
    while (1);               // dont do anything else
  }
  else {
    Serial.println(""); Serial.println(""); Serial.println("SD started");
  }
  strcpy(fileName, "XX      .txt");
  strcpy(fileNam2, "XX      .dbs");
  readline(fileName, 1);
  readline(fileNam2, 1);
  // set the data rate for the SoftwareSerial port
  readline (baudrate, 2);
  Serial.println(atol(baudrate));
  // Start the Serial ports 1 and 2
  Serial1.begin(9600); // Internal GPS Xmits at 9600
  Serial2.begin(atol(baudrate)); // Baudrate of Garmin Intelliducer
  readline (DBS, 3);
  Serial.println(DBS);
  Serial.println(fileName);
  strcpy(NMEA_dateTime, "$GPRMC"); // NMEA sentence that gives date/time/validity of position
  while (!Serial1.available()) {}  //Wait for first char

  read2end(); // Read to end of current line
  getDateTimeUT(); // Read NMEA sentences until a valid (status="A") $GPRMC and extract the date/time as a string
  Serial.println(yrI); Serial.println(moI); Serial.println(daI); Serial.println(hrI); Serial.println(miI); Serial.println(seI);
  // Set the clock to exact time (UT/GMT)using the string obtained from the $GPRMC sentence
  Serial.println("Adjust DATETIME");
  RTC.adjust(DateTime(yrI, moI, daI, hrI, miI, seI));

  DateTime now = RTC.now();
  yrI = now.year() - 2000;
  moI = now.month();
  daI = now.day();
  // Serial.println(yrI); Serial.println(moI); Serial.println(daI);
  byte dy = yrI / 10;
  byte yr = yrI % 10;
  byte dm = moI / 10;
  byte mo = moI % 10;
  byte dd = daI / 10;
  byte da = daI % 10;
  fileName[2] = fileNam2[2] = dy + 48;
  fileName[3] = fileNam2[3] = yr + 48;
  fileName[4] = fileNam2[4] = dm + 48;
  fileName[5] = fileNam2[5] = mo + 48;
  fileName[6] = fileNam2[6] = dd + 48;
  fileName[7] = fileNam2[7] = da + 48;
  fileName[12] = fileNam2[12] = 0;

  // ============================================================================================
  // read 100 sentences and get greatest time delay between sentences and corresponding NMEA type
  // ============================================================================================

  long int timer = 0;
  long int dely = 0;
  long int max_dely = 20;

  //  Serial.println("Start timing");
  timer = millis();
  for (int l = 0; l < 400; l++) {
    while (!Serial1.available()) {
    }  //Wait for next char
    inputRead = Serial1.read();
    //    Serial.print(inputRead);
    if (inputRead == '$') {
      //      Serial.print("!");
      dely = millis() - timer;
      timer = millis();
      if (dely > max_dely) {
        for (int j = 0; j < 6; j++) {
          NMEA_fini[j] = NMEA_type[j];
          max_dely = dely;
        }
        NMEA_type[6] = 0;
        NMEA_fini[6] = 0;
      }
      /*     Serial.print(NMEA_type);
      Serial.print(" - ");
      Serial.print(NMEA_fini);
      Serial.print(" - ");
      Serial.print(dely);
      Serial.print(" - ");
      Serial.println(max_dely); */
    }
    if (inputRead == '$' || NMEA_start) {
      NMEA_start = true;
      NMEA_type[NMEA_count] = inputRead; //Characters input
      //Serial.print(NMEA_type[NMEA_count]);
      NMEA_count++;
      if (NMEA_count == 6) {
        NMEA_start = 0;
        NMEA_count = 0;
        //        Serial.println(NMEA_type);
      }
    }
    else {
      read2end();
      colour++;
      if (colour == 1) {
        digitalWrite(6, HIGH); // Turn on blue
        digitalWrite(7, LOW); // Turn off green
      }
      if (colour >= 2) {
        digitalWrite(6, LOW); // Turn off blue
        digitalWrite(7, HIGH); // Turn on green
        colour = 0;
      }
    }
  }
  // Serial.print("Fini_type: ");
  // Serial.println(NMEA_fini);
  digitalWrite(5, LOW); // Turn off red
  digitalWrite(6, LOW); // Turn off blue
  digitalWrite(7, LOW); // Turn off green
}
// Finished Setup

// =====================
// Start processing loop
// =====================

void loop() {
  while (!Serial1.available() && !Serial2.available()) {
  } //Wait for next char from EITHER serial port
  if (Serial1.available()) {
    inputRead = Serial1.read();
    dataString  += inputRead; //Characters input
    charCount++;
    if (inputRead == '$' || NMEA_start) {
      NMEA_start = true;
      NMEA_type[NMEA_count] = inputRead; //Characters input
      //Serial.print(NMEA_type[NMEA_count]);
      NMEA_count++;
      if (NMEA_count == 6) {
        NMEA_start = 0;
        NMEA_count = 0;
        //        Serial.println(NMEA_type);
      }
    }
  }
  else {
    inputRead2 = Serial2.read();
    dataString2  += inputRead2; //Characters input
    charCount2++;
    if (inputRead2 == '$' || NMEA_start2) {
      NMEA_start2 = true;
      NMEA_type2[NMEA_count2] = inputRead2; //Characters input
      //Serial.print(NMEA_type2[NMEA_count2]);
      NMEA_count2++;
      if (NMEA_count2 == 6) {
        NMEA_start2 = 0;
        NMEA_count2 = 0;
        //        Serial.println(NMEA_type);
      }
    }
    if (inputRead2 == '\n') {
      charCount2 = 0;
      NMEA_last2 = 0;
      firstRec2 = false;
      //Serial.print("NMEA_type2: = ");Serial.println(NMEA_type2);
      if (!strCompare(NMEA_type2, NMEA_dbt)) { // Only store depth sentence if it starts with $SDDBT
        dataString2 = "";
      }
      else {
        dataStringDepth = dataString2;
        timer = millis(); // Set the time clock to measure delay between depth and GPS sentences
        //Serial.print("Com2: = ");Serial.print(dataString2);Serial.print(" >> ");Serial.print(dataStringDepth);
        //digitalWrite(4, HIGH);   // turn on LED 4 to show there will be a depth write
        dataString2 = "";
      }
    }
  }
  if (inputRead == '\n' && strCompare(NMEA_type, NMEA_fini)) {
    errCode = 0;
    dely = millis() - timer; // Delay from when depth sentence encountered
    timer = millis();
    //  Serial.print("freeMemory()=");
    //  Serial.println(freeMemory());
    if (firstRec) {
      File dataFile = SD.open(fileNam2, FILE_WRITE);
      DateTime now = RTC.now();
      if (dataFile) {
        dataFile.print(DBS);
        dataFile.print(now.year());
        dataFile.print("-");
        dataFile.print(now.month());
        dataFile.print("-");
        dataFile.print(now.day());
        dataFile.print(" ");
        dataFile.print(now.hour());
        dataFile.print(":");
        dataFile.println(now.minute());
        dataFile.close();
      }
    }
    else {
      File dataFile = SD.open(fileName, FILE_WRITE);
      long dataPos = dataFile.position();
      /*Serial.print("data.position=");
      Serial.println(dataPrev);
      Serial.println(dataPos); */
      if (dataPos <= dataPrev) {
        digitalWrite(5, HIGH);   // turn the LED 5 to show SD failure (not writing)
        while (1); // Give up processing
      }
      dataPrev = dataPos;
      if (dataFile) {
        digitalWrite(6, HIGH);   // turn on LED 6 to show SD write beginning
        if (dataStringDepth.length() > 0) digitalWrite(4, HIGH);   // turn on LED 4 to show depth write beginning
        dataFile.print(dataString);
        if (dataStringDepth.length() > 0 && dely < 5100) { // Must ensure that GPS signal and depth are synchronised
          dataFile.print(dataStringDepth);
        }
        dataFile.close();
        Serial.print(dataString);
        Serial.print("Delay = "); Serial.println(dely);
        Serial.print(dataStringDepth);
        digitalWrite(6, LOW);   // turn off LED 6 to show SD file closed
        digitalWrite(5, LOW);   // turn the LED 5 off to show SD success
        digitalWrite(4, LOW);   // turn off LED 4 to show depth write ended - if any
        errCode = 0;
        NMEA_last = false;
        // print to the serial port too:
      }
      else {
        // if the file didn't open, pop up an error:
        //      Serial.println("error opening datalog.txt");
        errCode = -1;
        digitalWrite(5, HIGH);   // turn the LED 5 to show SD failure
        while (1); // Give up processing
      }
    }
    charCount = 0;
    NMEA_last = 0;
    dataString = "";
    dataStringDepth = "";
    NMEA_type[0] = 0;
    firstRec = false;
  }
}


// =====================================
// Functions and subroutines
// =====================================

void read2end() {
  //  Serial.println("READ2END");
  inputRead = 0;
  while (inputRead != '\n') {  // Get up to end of input line
    while (!Serial1.available()) {
    }  //Wait for next char
    inputRead = Serial1.read();
    //    Serial.print(inputRead);
  }
  return;
}

int strCompare (char type[7], char fini[7]) {
  for (int k = 0; k < 6; k++) {
    //Serial.print(type[k]);Serial.print("??");Serial.println(fini[k]);
    if (type[k] != fini[k]) {
      return false;
    }
  }
  // Serial.println("true");
  return true;
}

// Read a line of text from SD card
char readline (char line[10], int ref) {
  char caract, i = 0;
  int linno = 1;
  File dataFile = SD.open(SD_file, FILE_READ);
  digitalWrite(6, HIGH);   // turn on LED 6 to show SD read beginning
  while (linno < ref) {
    while (!dataFile) {
    } // Wait for character ready
    caract = dataFile.read();
    if (caract == '\n') {
      linno++;
      caract = 0;
    }
  }
  while (caract != '\n') {
    while (!dataFile) {
    } // Wait for character ready
    caract = dataFile.read();
    line[i] = caract;
    i++;
  }
  line[i] = 0;
  dataFile.close();
  digitalWrite(6, LOW);   // turn off LED 6 to show SD file closed
  errCode = 0;
  return line[0];
  // print to the serial port too:
}

void getDateTimeUT() {
  digitalWrite(6, HIGH); // Turn on blue with the green
  digitalWrite(5, HIGH); // Turn on red
  // Serial.println("");   Serial.print("DATETIMEUT");
  // Get the time from the 2nd field and date from the tenth field when Valid RMC sentence is read from GPS e.g.
  // $GPRMC,120357,A,5220.4860,N,00627.5040,W,9.7,148.7,230416,5.9,W*73
  // Serial.print("gotDate & csvValid "); Serial.print(gotDate); Serial.print(csvValid);
  while (!gotDate || !csvValid) {
    // Serial.print("Testing for Valid $GPRMC");
    read2Comma();
    if (strCompare(csvField, NMEA_dateTime)) {
      // Serial.println("Compare $GPRMC");
      read2Comma();
      hrI = (csvField[0] - 48) * 10 + csvField[1] - 48;
      miI = (csvField[2] - 48) * 10 + csvField[3] - 48;
      seI = (csvField[4] - 48) * 10 + csvField[5] - 48;
      read2Comma();
      if (csvField[0] == 'A') {
        // Serial.print("Valid $GPRMC");
        // Valid $GPRMC position is A; a V means not valid date/time/position
        csvValid = true;
        for (int l = 0; l < 7; l++) {
          read2Comma();
        }
        daI = (csvField[0] - 48) * 10 + csvField[1] - 48;
        moI = (csvField[2] - 48) * 10 + csvField[3] - 48;
        yrI = (csvField[4] - 48) * 10 + csvField[5] - 48;
        read2end();
        gotDate = true;
      }
      else {
        colour++;
        if (colour == 1) {
          digitalWrite(5, HIGH); // Turn on red
          digitalWrite(6, LOW); // Turn off blue
          digitalWrite(7, LOW); // Turn off green
        }
        if (colour == 2) {
          digitalWrite(5, LOW); // Turn off red
          digitalWrite(6, HIGH); // Turn on blue
          digitalWrite(7, LOW); // Turn off green
        }
        if (colour >= 3) {
          digitalWrite(5, LOW); // Turn off red
          digitalWrite(6, LOW); // Turn off blue
          digitalWrite(7, HIGH); // Turn on green
          colour = 0;
        }
      }
    }
    else {
      read2end();
    }
  }
  digitalWrite(5, LOW); // Turn off red
  digitalWrite(6, LOW); // Turn off blue
  digitalWrite(7, LOW); // Turn off green
  //Serial.print("NOWgotDate & csvValid "); Serial.print(gotDate); Serial.print(csvValid);
}

void read2Comma () {
  // Serial.print("*");
  inputRead = 0;
  charCount = 0;
  while (inputRead != ',' && inputRead != '\n') {
    // Get up to comma or end of input line
    while (!Serial1.available()) {
    }
    //Wait for next char
    inputRead = Serial1.read();
    // Serial.print(inputRead);
    csvField[charCount] = inputRead;
    charCount++;

  }
  return;
}



