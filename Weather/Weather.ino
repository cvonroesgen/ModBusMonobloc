// WiFiWeatherStation.ino
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "WiFiSSLClient.h"

ArduinoLEDMatrix matrix;

#include "WiFiS3.h"
#include <Arduino_JSON.h>

#include <assert.h>

#define SECRET_SSID "121PageBrookRoad"
#define SECRET_PASS "1254019243"

unsigned long lastConnectionTime = 0;              // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 5L * 1000L;  // delay between updates, in milliseconds

unsigned char frame[8][12];
int dewpoint = 0;
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;           // your network key index number (needed only for WEP)

int status = WL_IDLE_STATUS;

char server[] = "api.weather.gov";  // name address for api.weaather.gov (using DNS)
// https://api.weather.gov/stations/KBED/observations/latest

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 443 is default for HTTPS):
WiFiSSLClient client;

void displayDec(int number) {
  // Make it scroll!
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(number);
  matrix.endText();
  matrix.endDraw();
}

/* -------------------------------------------------------------------------- */
void setup() {

  /* -------------------------------------------------------------------------- */
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }


  matrix.begin();

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
  }
  // you're connected now, so print out the status:
  printWifiStatus();
}

/* just wrap the received data up to 80 columns in the serial print*/
/* -------------------------------------------------------------------------- */
void read_response() {
  /* -------------------------------------------------------------------------- */
  uint32_t data_num = 0;

  char jsonData[6000];
  bool inHeader = true;
  while (client.available() && data_num < 6000) {
    while (inHeader) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        inHeader = false;
      }
    }
    /* actual data reception */
    char c = client.read();
    jsonData[data_num++] = c;
  }

  if (data_num == 0) {
    return;
  } else {
    jsonData[data_num] = 0;
  }
  client.stop();
  JSONVar apiJSON = JSON.parse(jsonData);
  if (JSON.typeof(apiJSON) == "undefined") {
    Serial.println("Parsing input failed!");
    Serial.println(jsonData);
    return;
  }
  Serial.print("Dew Point C: ");
  if (apiJSON.hasOwnProperty("properties")) {
    dewpoint = (double)apiJSON["properties"]["dewpoint"]["value"];
    Serial.println(dewpoint);
    displayDec(dewpoint);
    Serial.print("Quality Control: ");
    Serial.println(apiJSON["properties"]["dewpoint"]["qualityControl"]);
  }
}

/* -------------------------------------------------------------------------- */
void loop() {
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:

  // if postingInterval milliseconds have passed since your last connection,
  // then connect again and send data:
  if (millis() - lastConnectionTime > postingInterval) {
    lastConnectionTime = millis();
    http_request();
  }
  //displayDec((millis() - lastConnectionTime) / 1000);
  read_response();
}

/* -------------------------------------------------------------------------- */
void printWifiStatus() {
  /* -------------------------------------------------------------------------- */
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}



void http_request() {

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 443)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    client.println("GET /stations/KBED/observations/latest HTTP/1.1");
    client.println("Host: api.weather.gov");
    client.println("user-agent: (vonroesgen.com, claude@vonroesgen.com)");
    client.println("Connection: close");
    client.println();

  } else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }
}