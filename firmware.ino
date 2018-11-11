#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WiFiMulti.h>

#define MAXNUMOFPPL 6
#define NULLPATH "0000000_0000000_00000000000"
#define MSDELAY 290

//WiFi
const char* ssid = "VK_Hackathon_Legacy";
const char* password = "hackathon2018";
ESP8266WiFiMulti WiFiMulti;
WiFiUDP udp;

//Server ip and port
const char* servIp = "95.213.28.151";
//const char* servIp = "172.20.38.86";
const int port = 45050;

//Packets
char incomingPacket[64];
char replyPacket[39];

//Used pins
int Blinkd = D3;
int Rd = D7;
int Bd = D5;
int Gd = D6;

//Time stamps to make timer
unsigned long lastTime;

//Path section
struct path {
  char id[28] = NULLPATH;
  int r = 0;
  int g = 0;
  int b = 0;
};

//Temp
path temp;

//All pathes
path pathesList[MAXNUMOFPPL];

//Index of current path to make color looping
int currentPath = 0;

//Device id
int zone = 7;
int roomNum = 24; //24  or 38
int devNum = 1;   //1   or  3

//Encoded bites to send via led
unsigned int blinkAr;

//WiFi connecting
void wifiSetUp() {

  Serial.begin(115200);
  delay(10);
  
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  
}

//Main setup function
void setup() {
  wifiSetUp();

  pinMode(Blinkd, OUTPUT);
  pinMode(Rd, OUTPUT);
  pinMode(Gd, OUTPUT);
  pinMode(Bd, OUTPUT);

  blinkAr = convert();
  whiteBlinking();
  
  udp.begin(port);
  lastTime = millis();
}

//Getting, parsing, handling packets 
void getPackage() {

  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.printf("Received %d bytes from %s, port %d\n",
            packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    int len = udp.read(incomingPacket, 64);
    if (len > 0) {
      incomingPacket[len] = 0;
    }

    Serial.printf("UDP packet contents: %s|\n", incomingPacket);
    
    if (incomingPacket[0] == 'H') {
      sscanf(incomingPacket, "H;%27s;%d %d %d",
                          &temp.id, &temp.r, &temp.g, &temp.b);

      Serial.printf("|%s|\n", temp.id);
      Serial.printf("%d\n", temp.r);
      Serial.printf("%d\n", temp.g);
      Serial.printf("%d\n", temp.b);

      for(int j = 0; j < MAXNUMOFPPL; ++j) {
        if(strcmp(pathesList[j].id, temp.id) == 0) {
          pathesList[j].r = temp.r;
          pathesList[j].g = temp.g;
          pathesList[j].b = temp.b;
          sprintf(replyPacket, "H;%s;%d_2%d_%d", temp.id, zone, roomNum, devNum);
          sendPack();        
          return;
        }
      }
      
      int i = 0; 
      while(strcmp(pathesList[i].id, NULLPATH) != 0) {
        ++i;
        if (i > MAXNUMOFPPL - 1) {
          Serial.println("ERROR 1");
          return;  
        }  
      }
      memcpy(pathesList[i].id, temp.id, 27);
      pathesList[i].r = temp.r;
      pathesList[i].g = temp.g;
      pathesList[i].b = temp.b;
      
      sprintf(replyPacket, "H;%s;%d_2%d_%d", pathesList[i].id, zone, roomNum, devNum);
      sendPack();
      
      for(int j = 0; j < 12; ++j) {
        Serial.print(pathesList[j].id);
        Serial.print("  ");
        Serial.print(pathesList[j].r);
        Serial.print("  ");
        Serial.print(pathesList[j].g);
        Serial.print("  ");
        Serial.print(pathesList[j].b);
        Serial.println("  ");  
      }
    }
    
    if (incomingPacket[0] == 'L') { 
      Serial.printf("L: %s|\n", incomingPacket+2);

      sscanf(incomingPacket, "L;%27s", &temp.id);
      
      int i = 0;
      sprintf(replyPacket, "L;%s;%d_2%d_%d", temp.id, zone, roomNum, devNum);
      sendPack();
      for(; i < MAXNUMOFPPL; ++i) {
        Serial.printf("H: %s|\n", pathesList[i].id);
        if(strcmp(pathesList[i].id, temp.id) == 0) {
          memcpy(pathesList[i].id, NULLPATH, sizeof(pathesList[i].id)); 
          return;
        }
      }
      if (i == MAXNUMOFPPL) {
        Serial.println("ERROR");
        return;
      }      
    }
  }
}

//Set rgb color
void setRGB(int r = 0, int g = 0, int b = 0) {
    digitalWrite(Rd, r);
    digitalWrite(Gd, g);
    digitalWrite(Bd, b);
}

//Changing color in loop over all active sessions
void changeColor() {
  currentPath++;
  for(int i = 0; i < MAXNUMOFPPL + 1; ++i, ++currentPath) {
    if (currentPath > MAXNUMOFPPL - 1) currentPath = 0;
    if (strcmp(pathesList[currentPath].id, NULLPATH) != 0) {
      Serial.printf("Num is: %d\n", currentPath);
      setRGB(pathesList[currentPath].r, pathesList[currentPath].g, pathesList[currentPath].b);
      return;
    }
  }
  setRGB();
}

//Blinking to send data via led
//1 - high signal
//0 - low signal
void whiteBlinking() {
  for (int i = (sizeof(blinkAr)*8)-2; i >= 0; i--) {
        if (blinkAr & (1 << i)) {
            digitalWrite(Blinkd, HIGH);
            delayMicroseconds(MSDELAY);
        } else {
            digitalWrite(Blinkd, LOW);
            delayMicroseconds(MSDELAY);
        }
    }
}

//Send packet to server
void sendPack() {
  udp.beginPacket(servIp, port);
  udp.write(replyPacket);
  udp.endPacket();  
}

//MFM-encoding(modified)
unsigned int convert() {
    int r = (zone << 9) | ((roomNum << 3) | devNum);
    int count = 0;
    for(int i = 0; i < 13; ++i) {
        if (1U & (r >> i)) {
            count++; 
        }
    }
    r = r << 1;
    if (count % 2 == 1) r |= 1 << 0;
    int a = r;
    int c = ~(a | a << 1);
    unsigned int res = 0;
    for (int i = 12; i >= 0; --i) {
        if ((a >> i) & 1ULL) {
            res |= 1ULL << i * 2 + 1;
        }
        if ((c >> i) & 1ULL) {
            res |= 1ULL << i * 2;
        }
    }
    res = res << 2;
    res |= 1U << 30;
    res |= 1U << 0;
    return res;
}

//Main loop
void loop() { 
  whiteBlinking();
  if (abs(lastTime - millis()) > 3000) {
    sprintf(replyPacket, "A;0000000_0000000_00000000;%d_2%d_%d", zone, roomNum, devNum);
    sendPack();
    lastTime = millis();
    getPackage();
    changeColor();
    while (WiFiMulti.run() != WL_CONNECTED) {
      Serial.println("Wifi disconected. Reconnectig");
      wifiSetUp();
    }
  }
  
}
