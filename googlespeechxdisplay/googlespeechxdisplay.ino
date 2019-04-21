#include <U8g2lib.h>
#include <TinyGPS++.h>
#include <math.h>
#include <WiFiClientSecure.h>
//WiFiClientSecure is a big library. It can take a bit of time to do that first compile
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif


const int DELAY = 1000;
const int SAMPLE_FREQ = 8000;                          // Hz, telephone sample rate
const int SAMPLE_DURATION = 5;                        // duration of fixed sampling (seconds)
const int NUM_SAMPLES = SAMPLE_FREQ * SAMPLE_DURATION;  // number of of samples
const int ENC_LEN = (NUM_SAMPLES + 2 - ((NUM_SAMPLES + 2) % 3)) / 3 * 4;  // Encoded length of clip

const uint16_t RESPONSE_TIMEOUT = 6000;
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
const uint16_t IN_BUFFER_SIZE = 100;
char speech_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request for Google Speech
char display_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response for serverside
char request[IN_BUFFER_SIZE];

const int DISPLAY_UPDATE = 30000; //30 second delay between each call for time
uint32_t display_timer ;

/* CONSTANTS */
//Prefix to POST request:
const char PREFIX[] = "{\"config\":{\"encoding\":\"MULAW\",\"sampleRateHertz\":8000,\"languageCode\": \"en-US\",\"speechContexts\": [{\"phrases\": [\"hungry\", \"shuttle\", \"food\" ]}]}, \"audio\": {\"content\":\"";
const char SUFFIX[] = "\"}}"; //suffix to POST request
const int AUDIO_IN = A0; //pin where microphone is connected
const char API_KEY[] = "AIzaSyD6ETx_Ammh1jgbfxG_wggm8eGa08yzQzQ"; //don't change this

const uint8_t PIN_1 = 16; //button 1

/* Global variables*/
uint8_t button_state; //used for containing button state and detecting edges
int old_button_state; //used for detecting button edges
uint32_t time_since_sample;      // used for microsecond timing

char speech_data[ENC_LEN + 200] = {0}; //global used for collecting speech data
const char* NETWORK = "MIT";               //"6s08";     // your network SSID (name of wifi network)
const char* PASSWORD = "";                  //"iesc6s08"; // your network password
const char*  SERVER = "speech.google.com";  // Server URL

uint8_t old_val;
uint32_t timer;

bool hungry;
bool shuttle;

WiFiClientSecure client; //global WiFiClient Secure object 

HardwareSerial gps_serial(2);
TinyGPSPlus gps; //gps object

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2 (U8G2_R0, A5, A4);

void setup() {
  Serial.begin(115200);               // Set up serial port
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x12_mn);
  hungry = false; // variables to remember if hungry has been said
  shuttle = false; // variables to remember if shuttle has been said
  display_timer = -30000;
  Serial.begin(115200); //begin serial comms
  delay(100); //wait a bit (100 ms)
  pinMode(PIN_1, INPUT_PULLUP);

  WiFi.begin(NETWORK, PASSWORD); //attempt to connect to wifi
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(NETWORK);
  while (WiFi.status() != WL_CONNECTED && count < 12) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.println(WiFi.localIP().toString() + " (" + WiFi.macAddress() + ") (" + WiFi.SSID() + ")");
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }
  timer = millis();
  old_val = digitalRead(PIN_1);
  analogSetAttenuation(ADC_6db); //set to 6dB attenuation for 3.3V full scale reading.
}

//main body of code
void loop(){
  if (gps_serial.available()){
    while (gps_serial.available()) 
      gps.encode(gps_serial.read());      // Check GPS
  }
  
  //in case of speech
  button_state = digitalRead(PIN_1);
  if (!button_state && button_state != old_button_state){
    doest_thou_speak();
  }
  old_button_state = button_state;

  //maintain display every 30s
  if (millis() - display_timer > DISPLAY_UPDATE){
    update_info_http();
    display_timer = millis();  
  }
}


void doest_thou_speak(){
  Serial.println("listening...");
  record_audio();
  Serial.println("sending...");
  Serial.print("\nStarting connection to server...");
  delay(300);
  bool conn = false;
  for (int i = 0; i < 10; i++) {
    int val = (int)client.connect(SERVER, 443);
    Serial.print(i); Serial.print(": "); Serial.println(val);
    if (val != 0) {
      conn = true;
      break;
    }
    Serial.print(".");
    delay(300);
  }
  if (!conn) {
    Serial.println("Connection failed!");
    return;
  } else {
    Serial.println("Connected to server!");
    Serial.println(client.connected());
    int len = strlen(speech_data);
    // Make a HTTP request:
    client.print("POST /v1/speech:recognize?key="); client.print(API_KEY); client.print(" HTTP/1.1\r\n");
    client.print("Host: speech.googleapis.com\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("cache-control: no-cache\r\n");
    client.print("Content-Length: "); client.print(len);
    client.print("\r\n\r\n");
    int ind = 0;
    int jump_size = 1000;
    char temp_holder[jump_size + 10] = {0};
    Serial.println("sending data");
    while (ind < len) {
      delay(80);//experiment with this number!
      //if (ind + jump_size < len) client.print(speech_data.substring(ind, ind + jump_size));
      strncat(temp_holder, speech_data + ind, jump_size);
      client.print(temp_holder);
      ind += jump_size;
      memset(temp_holder, 0, sizeof(temp_holder));
    }
    client.print("\r\n");
    //Serial.print("\r\n\r\n");
    Serial.println("Through send...");
    unsigned long count = millis();
    while (client.connected()) {
      Serial.println("IN!");
      String line = client.readStringUntil('\n');
      Serial.print(line);
      if (line == "\r") { //got header of response
        Serial.println("headers received");
        break;
      }
      if (millis() - count > RESPONSE_TIMEOUT) break;
    }
    Serial.println("");
    Serial.println("Response...");
    count = millis();
    while (!client.available()) {
      delay(100);
      Serial.print(".");
      if (millis() - count > RESPONSE_TIMEOUT) break;
    }
    Serial.println();
    Serial.println("-----------");
    memset(speech_response, 0, sizeof(speech_response));
    while (client.available()) {
      char_append(speech_response, client.read(), OUT_BUFFER_SIZE);
    }
    Serial.println(speech_response);
    char* trans_id = strstr(speech_response, "transcript");
    if (trans_id != NULL) {
      char* foll_coll = strstr(trans_id, ":");
      char* starto = foll_coll + 2; //starting index
      char* endo = strstr(starto + 1, "\""); //ending index
      int transcript_len = endo-starto+1;
      char transcript[100] = {0};
      strncat(transcript,starto,transcript_len);
      Serial.println(transcript);
      
      const char hungryupper[10] = "Hungry";
      const char hungrylower[10] = "hungry";
      char *hung = strstr(transcript, hungryupper); //looks to see if someone said "Hungry"
      char *hun = strstr(transcript, hungrylower); //looks to see if someone said "hungry"

      const char foodupper[10] = "Food";
      const char foodlower[10] = "food";
      char *foodd = strstr(transcript, foodupper); //looks to see if someone said "Food"
      char *food = strstr(transcript, foodlower); //looks to see if someone said "food"


      const char shuttleupper[10] = "Shuttle";
      const char shuttlelower[10] = "shuttle";
      char *shutt = strstr(transcript, shuttleupper); //looks to see if someone said "Shuttle"
      char *shut = strstr(transcript, shuttlelower); //looks to see if someone said "shuttle"
      
      if ((hung!=NULL)||(hun!=NULL)||(foodd!=NULL)||(food!=NULL)){
        hungry = true;
        Serial.println("Found Hungry or hungry or Food or food!!");
       }
      if ((shutt!=NULL)||(shut!=NULL)){
        shuttle = true;
        Serial.println("Found Shuttle or shuttle!!");
       }

      if (hungry&&shuttle){
        Serial.println("Please only give one command at a time.");
        hungry = false;
        shuttle = false;
       } else if (hungry){
        Serial.println("Here's some restaurants near you");
        hungry = false;
       } else if (shuttle){
        Serial.println("Here's the MIT shuttle schedule");
        shuttle = false;
       } else {
        Serial.println("Sorry, I didn't hear a command. Try asking again by holding the button.");
       }         
    }
    Serial.println("-----------");
    client.stop();
    Serial.println("done");
  }
}

//function used to record audio at sample rate for a fixed nmber of samples
void record_audio() {
  int sample_num = 0;    // counter for samples
  int enc_index = strlen(PREFIX) - 1;  // index counter for encoded samples
  float time_between_samples = 1000000 / SAMPLE_FREQ;
  int value = 0;
  char raw_samples[3];   // 8-bit raw sample data array
  memset(speech_data, 0, sizeof(speech_data));
  sprintf(speech_data, "%s", PREFIX);
  char holder[5] = {0};
  Serial.println("starting");
  uint32_t text_index = enc_index;
  uint32_t start = millis();
  time_since_sample = micros();
  uint8_t button = digitalRead(PIN_1);
  while ((sample_num < NUM_SAMPLES)&&(!button)) { //read in NUM_SAMPLES worth of audio data
    value = analogRead(AUDIO_IN);  //make measurement
    raw_samples[sample_num % 3] = mulaw_encode(value - 1241); //remove 1.0V offset (from 12 bit reading)
    sample_num++;
    if (sample_num % 3 == 0) {
      base64_encode(holder, raw_samples, 3);
      strncat(speech_data + text_index, holder, 4);
      text_index += 4;
    }
    // wait till next time to read
    while (micros() - time_since_sample <= time_between_samples); //wait...
    time_since_sample = micros();
    button = digitalRead(PIN_1);
  }
  Serial.println(millis() - start);
  sprintf(speech_data + strlen(speech_data), "%s", SUFFIX);
  Serial.println("out");
}


int8_t mulaw_encode(int16_t sample) {
  const uint16_t MULAW_MAX = 0x1FFF;
   const uint16_t MULAW_BIAS = 33;
   uint16_t mask = 0x1000;
   uint8_t sign = 0;
   uint8_t position = 12;
   uint8_t lsb = 0;
   if (sample < 0)
   {
      sample = -sample;
      sign = 0x80;
   }
   sample += MULAW_BIAS;
   if (sample > MULAW_MAX)
   {
      sample = MULAW_MAX;
   }
   for (; ((sample & mask) != mask && position >= 5); mask >>= 1, position--)
        ;
   lsb = (sample >> (position - 4)) & 0x0f;
   return (~(sign | ((position - 5) << 4) | lsb));
}

/*----------------------------------
 * update_info_http Function:
 * makes pull request to get header display information
 */
void update_info_http(){
  char body[200]; //for body;
  sprintf(body,"lat=%f&lon=%f",gps.location.lat(),gps.location.lng());//generate body, posting to User, 1 step
  int body_len = strlen(body); //calculate body length (for header reporting)
  memset(request, sizeof(request), 0);
  sprintf(request,"POST http://608dev.net/sandbox/sc/carolpan/monocular/info_display.py HTTP/1.1\r\n");
  strcat(request,"Host: 608dev.net\r\n");
  strcat(request,"Content-Type: application/x-www-form-urlencoded\r\n");
  sprintf(request+strlen(request),"Content-Length: %d\r\n", body_len); //append string formatted to end of request buffer
  strcat(request,"\r\n"); //new line from header to body
  strcat(request,body); //body
  strcat(request,"\r\n"); //header
  Serial.println(request);
  do_http_request("608dev.net", request, display_response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT,true);
  Serial.println(display_response);
  char* t = strtok(display_response,"&");
  char* d = strtok(NULL, "&");
  char* icon = strtok(NULL, "&");
  char* pch = strtok(t,"=");
  t = strtok(NULL,"=");
  pch = strtok(d,"=");
  d = strtok(NULL,"=");
  pch = strtok(icon,"=");
  icon = strtok(NULL,"=");
  u8g2.clearBuffer();
  if (strcmp(icon,"cloud")==0){
    drawCloud();
  }
  if (strcmp(icon,"sun")==0){
    drawSun();
  }
  if (strcmp(icon,"moon")==0){
    drawMoon();
  }
  uint32_t start = micros(); //MARK BEGINNING OF TIMING!
  float voltage = analogRead(A6) * 2 * 3.3 / 4096; // get Battery Voltage (mV)
  float discharge_amt = discharge_from_voltage(voltage, 0.001); //get discharge amount using battery voltage
  drawBattery(1.0 - discharge_amt);
  u8g2.drawStr(28,13,d);
  u8g2.drawStr(68,13,t);
  u8g2.sendBuffer();
}


void drawSun(){
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.drawDisc(15, 8, 3, U8G2_DRAW_ALL);
  for (uint8_t i = 0; i < 8; i++) {
    u8g2.drawLine(15 + 5 * sin(i * 2 * PI / 8), 8 - 5 * cos(i * 2 * PI / 8), 15 + 6 * sin(i * 2 * PI / 8), 8 - 6 * cos(i * 2 * PI / 8));
  }
}

void drawMoon(){
  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.drawDisc(13, 8, 7, U8G2_DRAW_ALL);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(17, 8, 8, U8G2_DRAW_ALL);
  u8g2.setDrawColor(1);
}

void drawCloud(){
  u8g2.drawDisc(11, 11, 3, U8G2_DRAW_ALL);
  u8g2.drawDisc(19, 11, 3, U8G2_DRAW_ALL);
  u8g2.drawDisc(15, 7, 3, U8G2_DRAW_ALL);
  u8g2.drawDisc(15, 10, 3, U8G2_DRAW_ALL);
}

void drawBattery(float level){
  u8g2.drawLine(104,5,104,13);
  u8g2.drawLine(104,5,124,5);
  u8g2.drawLine(124,5,124,13);
  u8g2.drawLine(104,13,124,13);
  u8g2.drawBox(104+20,7,2,4);
  u8g2.drawBox(104,5,20*level,8);
}
