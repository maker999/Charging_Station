#include <SPI.h>
#include <WiFiST.h>


SPIClass SPI_3(PC12, PC11, PC10);
WiFiClass WiFi(&SPI_3, PE0, PE1, PE8, PB13);

char ssid[] = "rddl";       // your network SSID (name)
char pass[] = "code&riddle";    // your network password (use for WPA, or use as key for WEP)

//char ssid[] = "UPC-Home";       // your network SSID (name)
//char pass[] = "Blank#t$100$";    // your network password (use for WPA, or use as key for WEP)


int keyIndex = 0;                  // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the static IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // static IP for Google (no DNS)

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.println("\tWiFi network status:");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi device's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the MAC address of your WiFi module:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i != 5) {
      Serial.print(":");
    } else {
      Serial.println();
    }
  }

  // print the received signal strength (RSSI):
  int32_t rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}

String wifi_init() {
  // initialize the WiFi module:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi module not detected");
    // don't continue:
    while (true);
  }

  // print firmware version:
//  String fv = WiFi.firmwareVersion();
//  Serial.print("Firwmare version: ");
//  Serial.println(fv);
//
//  if (fv != "C3.5.2.3.BETA9") {
//    Serial.println("Please upgrade the firmware");
//  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WiFi network ");
    Serial.print(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("...Connected.");
  IPAddress ip = WiFi.localIP();
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  //printWifiStatus();
}

