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

//for stepcounter code
#include <mpu9255_esp32.h>
#include<string.h> //for printing step values n things if needed... might not be needed
#define IDLE 0
#define PEAK 1
uint8_t stepstate; //used to be state
float old_acc_mag; 
float older_acc_mag;
float x,y,z; //variables for grabbing x,y,and z values
MPU9255 imu; //imu object called, appropriately, imu
//end of stuff needed for step counter code

const int DELAY = 1000;
const int SAMPLE_FREQ = 8000;                          // Hz, telephone sample rate
const int SAMPLE_DURATION = 5;                        // duration of fixed sampling (seconds)
const int NUM_SAMPLES = SAMPLE_FREQ * SAMPLE_DURATION;  // number of of samples
const int ENC_LEN = (NUM_SAMPLES + 2 - ((NUM_SAMPLES + 2) % 3)) / 3 * 4;  // Encoded length of clip
const int STORAGE_SIZE = 40;

const uint16_t RESPONSE_TIMEOUT = 6000;
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
const uint16_t IN_BUFFER_SIZE = 100;
char speech_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request for Google Speech
char display_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response for serverside
char get_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response for serverside
char post_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response for serverside
char request[IN_BUFFER_SIZE];

char storage[STORAGE_SIZE];

const int DISPLAY_UPDATE = 30000;//1800000; //30 second delay between each call for time
uint32_t display_timer;
uint32_t second_timer;
int t[3] = {0};
char* d;
char* icon;

/* CONSTANTS */
//Prefix to POST request:
const char PREFIX[] = "{\"config\":{\"encoding\":\"MULAW\",\"sampleRateHertz\":8000,\"languageCode\": \"en-US\",\"speechContexts\": [{\"phrases\": [\"hungry\", \"shuttle\", \"steps\", \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", \"8\", \"9\", \"10\", \"11\", \"12\", \"13\", \"14\", \"15\", \"16\", \"17\", \"18\", \"19\", \"20\", \"21\", \"22\", \"23\", \"24\", \"25\", \"26\", \"27\", \"28\", \"29\", \"30\", \"31\", \"food\" ]}]}, \"audio\": {\"content\":\"";
const char SUFFIX[] = "\"}}"; //suffix to POST request
const int AUDIO_IN = A0; //pin where microphone is connected
const char API_KEY[] = "AIzaSyD6ETx_Ammh1jgbfxG_wggm8eGa08yzQzQ"; //don't change this

const uint8_t PIN_1 = 16; //button 1

/* Global variables*/
uint8_t button_state; //used for containing button state and detecting edges
int old_button_state; //used for detecting button edges
uint32_t time_since_sample;      // used for microsecond timing

char speech_data[ENC_LEN + 200] = {0}; //global used for collecting speech data
const char* NETWORK =       "6s08";     //  "MIT"; your network SSID (name of wifi network)
const char* PASSWORD =       "iesc6s08"; // "";  your network password
const char*  SERVER = "speech.google.com";  // Server URL

uint8_t old_val;
uint32_t timer;

bool hungry;
bool shuttle;
bool stepchecker;
uint16_t steps;
uint8_t day;

WiFiClientSecure client; //global WiFiClient Secure object 

HardwareSerial gps_serial(2);
TinyGPSPlus gps; //gps object

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2 (U8G2_R0, A5, A4);

void setup() {
  Serial.begin(115200);               // Set up serial port
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  hungry = false; // variables to remember if hungry has been said
  shuttle = false; // variables to remember if shuttle has been said
  stepchecker = false; // variables to remember if step has been said
  day = 0;
  display_timer = -1800000;
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
  // for IMU start-up
  if (imu.setupIMU(1)){
    Serial.println("IMU Connected!");
  }else{
    Serial.println("IMU Not Connected :/");
    Serial.println("Restarting");
    ESP.restart(); // restart the ESP (proper way)
  }
  timer = millis();
  second_timer = millis();
  old_val = digitalRead(PIN_1);
  analogSetAttenuation(ADC_6db); //set to 6dB attenuation for 3.3V full scale reading.
  steps = 0; //initialize steps to zero!
  older_acc_mag = 0;
  old_acc_mag = 0;
}

//main body of code ////////////////////// LOOP START //////////////////////// 
void loop(){
  
//Step Counter Code
imu.readAccelData(imu.accelCount);
  x = imu.accelCount[0]*imu.aRes;
  y = imu.accelCount[1]*imu.aRes;
  z = imu.accelCount[2]*imu.aRes;
  float acc_mag = sqrt(x*x+y*y+z*z);
  float avg_acc_mag = (acc_mag + old_acc_mag + older_acc_mag) / 3.0;
  older_acc_mag = old_acc_mag;
  old_acc_mag = acc_mag;
  
  //finite state machine
  //Serial.println("Acceleration Value is...");
  //Serial.println(avg_acc_mag);
 switch(stepstate){
    case IDLE:
      if (avg_acc_mag > 1.2){  //values need to be tested/changed
      stepstate = PEAK;
      steps ++;
        }
      break; 
    case PEAK:
      if (avg_acc_mag < 1.0){  //values need to be tested/changed
      stepstate = IDLE;
      displayHeader();
        }
      break;
      } 
//End of Step Counter Code
  
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
  if (millis() - second_timer > 1000){
    t[2] += 1;
    if (t[2] >= 60){
     Serial.println(t[1]);
      t[2] -= 60;
      t[1] += 1;
      displayHeader();
    }
    if (t[1] >= 60){
      t[1] -= 60;
      t[0] += 1; 
    }
    second_timer = millis();
  }
  //maintain display every 30s
  if (millis() - display_timer > DISPLAY_UPDATE){
    update_info_http();
    if (steps>0){Serial.println("POSTING STEPS");
    post_steps_http(); // posts current step updates database
    Serial.println("DONE POSTING STEPS");
    }
    display_timer = millis();  
  }
}


///////////////////////// STEP CODE ////////////////////////////////// ///////////////////////// STEP CODE //////////////////////////////////
void get_steps_http(){
  Serial.println("GETTING!");
  sprintf(request, "GET /sandbox/sc/sfgordon/stepdb.py?day=%d HTTP/1.1\r\n", day);
  strcat(request, "Host: 608dev.net\r\n\r\n");
  do_http_request("608dev.net", request, get_response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
  Serial.println("get response");
  Serial.println(get_response);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 17, 150, 100);
  u8g2.setDrawColor(1);
  u8g2.setCursor(5, 24);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.print(get_response);
  u8g2.sendBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tf); // choose a suitable font
}

void post_steps_http(){
  Serial.println("POSTING!");
  char body[200]; //for body;
  sprintf(body,"step=%d",steps);//generate body, posting to User, 1 step
  int body_len = strlen(body); //calculate body length (for header reporting)
  memset(request, sizeof(request), 0);
  sprintf(request,"POST http://608dev.net/sandbox/sc/sfgordon/stepdb.py HTTP/1.1\r\n");
  strcat(request,"Host: 608dev.net\r\n");
  strcat(request,"Content-Type: application/x-www-form-urlencoded\r\n");
  sprintf(request+strlen(request),"Content-Length: %d\r\n", body_len); //append string formatted to end of request buffer
  strcat(request,"\r\n"); //new line from header to body
  strcat(request,body); //body
  strcat(request,"\r\n"); //header
  Serial.println(request);
  do_http_request("608dev.net", request, post_response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT,true);
  Serial.println(post_response);
  
}
 ///////////////////////// STEP CODE ////////////////////////////////// ///////////////////////// STEP CODE /////////////

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
      const char stepupper[10] = "Step";
      const char steplower[10] = "step";
      char *stepss = strstr(transcript, stepupper); //looks to see if someone said "Step"
      char *stepsss = strstr(transcript, steplower); //looks to see if someone said "step"
      
      if ((hung!=NULL)||(hun!=NULL)||(foodd!=NULL)||(food!=NULL)){
        hungry = true;
        Serial.println("Found Hungry or hungry or Food or food!!");
       }
      if ((shutt!=NULL)||(shut!=NULL)){
        shuttle = true;
        Serial.println("Found Shuttle or shuttle!!");
       }
       if ((stepss!=NULL)||(stepsss!=NULL)){
        stepchecker = true;
        Serial.println("Found Step or step!!");
       }

      if ((hungry&&shuttle)||(hungry&&stepchecker)||(shuttle&&stepchecker)){
        Serial.println("Please only give one command at a time.");
        hungry = false;
        shuttle = false;
        stepchecker = false;
      } else if (hungry) {
        get_restaurants();
        Serial.println("Here's some restaurants near you");
        hungry = false;
      } else if (shuttle) {
        get_shuttles();
        Serial.println("Here's the MIT shuttle schedule");
        shuttle = false;
      } else if (stepchecker){ 
      Serial.println("Here's your total steps");
        stepchecker = false;
        const char one[3] = "1";
        char *onee = strstr(transcript, one);
        const char two[3] = "2";
        char *twoo = strstr(transcript, two);
        const char three[3] = "3";
        char *threee = strstr(transcript, three);
        const char four[3] = "4";
        char *fourr = strstr(transcript, four);
        const char five[3] = "5";
        char *fivee = strstr(transcript, five);
        const char six[3] = "6";
        char *sixx = strstr(transcript, six);
        const char seven[3] = "7";
        char *sevenn = strstr(transcript, seven);
        const char eight[3] = "8";
        char *eightt = strstr(transcript, eight);
        const char nine[3] = "9";
        char *ninee = strstr(transcript, nine);
        const char ten[4] = "10";
        char *tenn = strstr(transcript, ten);
        const char eleven[4] = "11";
        char *elevenn = strstr(transcript, eleven);
        const char twelve[4] = "12";
        char *twelvee = strstr(transcript, twelve);
        const char thirteen[4] = "13";
        char *thirteenn = strstr(transcript, thirteen);
        const char fourteen[4] = "14";
        char *fourteenn = strstr(transcript, fourteen);
        const char fifteen[4] = "15";
        char *fifteenn = strstr(transcript, fifteen);
        const char sixteen[4] = "16";
        char *sixteenn = strstr(transcript, sixteen);
        const char seventeen[4] = "17";
        char *seventeenn = strstr(transcript, seventeen);
        const char eighteen[4] = "18";
        char *eighteenn = strstr(transcript, eighteen);
        const char nineteen[4] = "19";
        char *nineteenn = strstr(transcript, nineteen);
        const char twenty[4] = "20";
        char *twentyy = strstr(transcript, twenty);
        if(tenn!=NULL){
          day = 10;
          }else if(elevenn!=NULL){
          day = 11;
          }else if(twelvee!=NULL){
          day = 12;
          }else if(thirteenn!=NULL){
          day = 13;
          }else if(fourteenn!=NULL){
          day = 14;
          }else if(fifteenn!=NULL){
          day = 15;
          }else if(sixteenn!=NULL){
          day = 16;
          }else if(seventeenn!=NULL){
          day = 17;
          }else if(eighteenn!=NULL){
          day = 18;
          }else if(nineteenn!=NULL){
          day = 19;
          }else if(twentyy!=NULL){
          day = 20;
          }else if(onee!=NULL){
          day = 1;
          }else if(twoo!=NULL){
          day = 2;
          }else if(threee!=NULL){
          day = 3;
          }else if(fourr!=NULL){
          day = 4;
          }else if(fivee!=NULL){
          day = 5;
          }else if(sixx!=NULL){
          day = 6;
          }else if(sevenn!=NULL){
          day = 7;
          }else if(eightt!=NULL){
          day = 8;
          }else if(ninee!=NULL){
          day = 9;
          }
          if(day==0){
            Serial.println("Please give a specific day number. Try asking again by holding the button.");
            }else{
          get_steps_http();
          day = 0;  
              }
      }else{
        Serial.println("Sorry, I didn't hear a command. Try asking again by holding the button.");
      }         
    }
    Serial.println("-----------");
    client.stop();
    Serial.println("done");
  }
}

void get_restaurants() {
  //call server-side script
  sprintf(request, "GET /sandbox/sc/kevinren/monocular/monocular.py?type=yelp&lat=%f&lon=%f HTTP/1.1\r\n",
          gps.location.lat(), gps.location.lng());
  strcat(request, "Host: 608dev.net\r\n\r\n");
  do_http_request("608dev.net", request, display_response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);

  //clear the body of the screen
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 17, 150, 100);
  u8g2.setDrawColor(1);

  int pos_y = 24;
  u8g2.setCursor(5, pos_y);
  //Parse response
  char *x = strtok (display_response, "\n");
  int num = atoi(x); //number of restaurants retrieved
  for (int i = 0; i < num; i++) {
    char *rest_name = strtok (NULL, "\n"); //restaurant name
    char *star_str = strtok(NULL, "\n"); //rating
    int stars = star_str[0] - '0';
    bool half_star = (star_str[1] == '.' && star_str[2] == '5');
    char *type = strtok(NULL, "\n"); //category
    char *loc = strtok(NULL, "\n"); //location
    char *dist = strtok(NULL, "\n"); //distance from current user
    u8g2.setFont(u8g2_font_5x7_tf);
    memset(storage, 0, STORAGE_SIZE);
    snprintf(storage, 18, "%d. %s", i + 1, rest_name);
    u8g2.print(storage); //prints "n. " and then first 15 characters of name

    u8g2.setFont(u8g2_font_unifont_t_symbols);
    for (int j = 0; j < stars; j++) { //draw stars corresponding to rating
      u8g2.drawGlyph(90 + 7 * j, pos_y + 3, 0x2605);
    }
    if (half_star) { //half star
      u8g2.drawGlyph(90 + 7 * stars, pos_y + 3, 0x2605);
      u8g2.setDrawColor(0);
      u8g2.drawBox(95 + 7 * stars, pos_y - 6, 3, 7);
      u8g2.setDrawColor(1);
    }
    pos_y += 7;
    u8g2.setFont(u8g2_font_u8glib_4_tf); //smaller font
    u8g2.setCursor(5, pos_y);
    memset(storage, 0, STORAGE_SIZE);
    snprintf(storage, 35, "%s, %s", type, loc);
    u8g2.print(storage); //print category and address
    pos_y += 8;
    u8g2.setCursor(5, pos_y);
  }
  u8g2.sendBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tf); // restore the font
}

void get_shuttles() {
  //call server-side script
  sprintf(request, "GET /sandbox/sc/kevinren/monocular/monocular.py?type=shuttle&lat=%f&lon=%f HTTP/1.1\r\n",
          gps.location.lat(), gps.location.lng());
  strcat(request, "Host: 608dev.net\r\n\r\n");
  do_http_request("608dev.net", request, display_response, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);

  //clear the body of the screen
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 17, 150, 100);
  u8g2.setDrawColor(1);

  //Parse the response
  char *stop_name = strtok (display_response, "\n");
  if (strcmp(stop_name, "0") == 0) { //No shuttles
    u8g2.drawStr(10, 24, "Error: No shuttles");
  } else { //Shuttles!
    int pos_y = 24;
    u8g2.setCursor(5, pos_y); //starting cursor
    u8g2.setFont(u8g2_font_5x7_tf); //nice font
    memset(storage, 0, STORAGE_SIZE); //clear storage buffer
    snprintf(storage, 25, "%s", stop_name);
    u8g2.print(storage); //print first 25 chars of name of stop
    char *pch = strtok(NULL, "\n");
    while (pch != NULL) { //parse the arrival times
      pos_y += 10;
      u8g2.setCursor(5, pos_y); //move the cursor down
      u8g2.print(pch);
      pch = strtok(NULL, "\n"); //get the next arrival time
    }
  }
  u8g2.sendBuffer(); //send changes to screen
  u8g2.setFont(u8g2_font_ncenB08_tf); // restore the font
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
  //Serial.println(display_response);
  char* time = strtok(display_response,"&");
  d = strtok(NULL, "&");  
  icon = strtok(NULL, "&");
  char* pch = strtok(time,"=");
  time = strtok(NULL,"=");
  pch = strtok(d,"=");
  d = strtok(NULL,"=");
  t[0] = atoi(strtok(time,":"));
  t[1] = atoi(strtok(NULL,":"));
  t[2] = atoi(strtok(NULL,":"));
  pch = strtok(icon,"=");
  icon = strtok(NULL,"=");
  displayHeader();
}

void displayHeader(){
  //u8g2.clearBuffer();
  //u8g2.setFont(u8g2_font_ncenB08_tf); // choose a suitable font
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 150, 17);
  u8g2.setDrawColor(1);
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
  drawSteps();
  char timer[6] = {0};
  if (t[1] < 10){
    sprintf(timer,"%d:0%d",t[0],t[1]);
  }
  else{
    sprintf(timer,"%d:%d",t[0],t[1]);
  }
  Serial.println(timer);
  u8g2.drawStr(28,13,timer);
  u8g2.sendBuffer();
}


void drawSun(){
  //u8g2.clearBuffer();          // clear the internal memory
  u8g2.drawDisc(15, 8, 3, U8G2_DRAW_ALL);
  for (uint8_t i = 0; i < 8; i++) {
    u8g2.drawLine(15 + 5 * sin(i * 2 * PI / 8), 8 - 5 * cos(i * 2 * PI / 8), 15 + 6 * sin(i * 2 * PI / 8), 8 - 6 * cos(i * 2 * PI / 8));
  }
}

void drawMoon(){
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

void drawSteps(){
  u8g2.drawFilledEllipse(70, 7, 2, 5, U8G2_DRAW_ALL);
  u8g2.drawDisc(70, 13, 2, U8G2_DRAW_ALL);
  u8g2.drawFilledEllipse(78, 5, 2, 5, U8G2_DRAW_ALL);
  u8g2.drawDisc(78, 11, 2, U8G2_DRAW_ALL);
  char num[5] = {0};
  sprintf(num,"%d",steps);
  u8g2.drawStr(84, 13, num);
}
