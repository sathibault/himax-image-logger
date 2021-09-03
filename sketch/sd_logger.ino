#include <SPI.h>
#include <SD.h>

// Minimum 1000 for raw format 320x320
#define FRAME_RATE_MS 1000

#define VERBOSE 0

#define SD_CARD_FREQ 12000000

int fileNo;
File imgfile;
unsigned long deadline;

////////// Prototypes

void makeFilename(char *filename, int n);
int findNextFileNo(int cur);
void open_new(int no);
void readBlock(unsigned char *data, int len);
void writeHeaderPGM(int w, int h);
void writePGM(int w, int h);
void halt();


void setup() {
  // Initialize FPGA module pins
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH); // FPGA PROGN (SPI_SEL)
  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH); // FPGA HOST SSN
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW); // FPGA CAPTURE REQ
  
  Serial.begin(115200);

#if 1
  delay(4000);
  // 12MHz CS pin 10
  if (!SD.begin(SD_CARD_FREQ, 10)) {
    Serial.println("Card init. failed!");
    halt();
  }
  fileNo = findNextFileNo(0);
#else
  pinMode(SPI_MISO_PIN, INPUT);
  pinMode(SPI_MOSI_PIN, OUTPUT);
  pinMode(SPI_SCK_PIN, OUTPUT);
  SPI.begin();
#endif

  delay(4000);
  deadline = millis();
}

#define CHUNK_SZ 256
#define CHUNK_CNT 400

void loop()
{
  int i, x;
  unsigned char data[CHUNK_SZ];

  unsigned long now = millis();
  if (now >= deadline) {
    digitalWrite(5, HIGH); // FPGA CAPTURE REQ
    digitalWrite(5, LOW);

    SPI.beginTransaction(SPISettings(3000000, MSBFIRST, SPI_MODE0));
    i = 0;
    while (1) {
      digitalWrite(9, LOW); // FPGA HOST SSN
      x = SPI.transfer(0);
      digitalWrite(9, HIGH); // FPGA HOST SSN
      // wait for begin image magic number 0x5a
      if (x == 0x5a)
        break;
      // if not ready should expect retry magic number 0x5a, otherwise we are out of sync somehow
      if (x != 0x96) {
        SPI.endTransaction();
        Serial.print(x);
        Serial.println(" capture error");
        halt();
      }
      i++;
    }
    SPI.endTransaction();

#if VERBOSE
    Serial.print("ready in ");
    Serial.println(i);
#endif

    open_new(fileNo);
    writeHeaderPGM(320, 320);
    
    for (i = 0; i < CHUNK_CNT; i++) {
      readBlock(data, CHUNK_SZ);
      imgfile.write(data, CHUNK_SZ);
    }
    imgfile.close();
    fileNo += 1;

#if VERBOSE
    Serial.print(millis()-now);
    Serial.println(" done");
#endif

    deadline += FRAME_RATE_MS;
  } else {
    delay(deadline - now);
  }
}

////////// SD functions

void makeFilename(char *filename, int n) {
  strcpy(filename, "/cam0000.pgm");
  for (int j = 7; j >= 4; j--) {
    filename[j] = '0' + n%10;
    n = n / 10;
  }
}

int findNextFileNo(int cur) {
  char filename[15];

  for (int i = cur; i < 10000; i++) {
    makeFilename(filename, i);
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      return(i);
    }
  }
}

void open_new(int no) {
  char filename[15];
  makeFilename(filename, no);

  imgfile = SD.open(filename, FILE_WRITE);
  if( ! imgfile ) {
    Serial.print("Couldnt create "); 
    Serial.println(filename);
    halt();
  }
#if VERBOSE
  Serial.print("Writing ");
  Serial.println(filename);
#endif
}


////////// FPGA functions

void readBlock(unsigned char *data, int len) {
  SPI.beginTransaction(SPISettings(3000000, MSBFIRST, SPI_MODE0));
  digitalWrite(9, LOW); // FPGA HOST SSN
  for (int i = 0; i < len; i++) {
    data[i] = SPI.transfer(0);
  }
  digitalWrite(9, HIGH); // FPGA HOST SSN
  SPI.endTransaction();
}


////////// Image functions

void writeHeaderPGM(int w, int h) {
  imgfile.print("P5\n");
  imgfile.print(w);
  imgfile.print(" ");
  imgfile.print(h);
  imgfile.print("\n255\n");
}

void writePGM(int w, int h) {
  writeHeaderPGM(w, h);
  unsigned char pat[4] = {0x40,0x80,0xc0,0xa0};
  for (int i=0; i<h; i++) {
    for (int j=0; j<w; j+=4)
      imgfile.write(pat,4);
  }
}


////////// Misc functions

void halt() {
  while (1) delay(1000);
}
