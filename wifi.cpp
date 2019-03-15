#include <SPI.h>
#include <WiFiST.h>


SPIClass SPI_3(PC12, PC11, PC10);
WiFiClass WiFi(&SPI_3, PE0, PE1, PE8, PB13);

//char ssid[] = "rddl";       // your network SSID (name)
//char pass[] = "code&riddle";    // your network password (use for WPA, or use as key for WEP)

// char ssid[] = "UPC-Home";       // your network SSID (name)
// char pass[] = "Blank#t$100$";    // your network password (use for WPA, or use as key for WEP)

//char ssid[] = "AndroidAP9810";       // your network SSID (name)
//char pass[] = "6eafd874ac3d";    // your network password (use for WPA, or use as key for WEP)

//char ssid[] = "AndroidAP";       // your network SSID (name)
//char pass[] = "hcny2412";    // your network password (use for WPA, or use as key for WEP)

//char ssid[] = "R3C2";
//char pass[] = "code!riddle&";

char ssid[] = "Niob";
char pass[] = "riddle&code&demo";


int keyIndex = 0;                  // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the static IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // static IP for Google (no DNS)

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial1.println("\tWiFi network status:");
  Serial1.print("SSID: ");
  Serial1.println(WiFi.SSID());

  // print your WiFi device's IP address:
  IPAddress ip = WiFi.localIP();
  Serial1.print("IP Address: ");
  Serial1.println(ip);

  // print the MAC address of your WiFi module:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial1.print("MAC address: ");
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      Serial1.print("0");
    }
    Serial1.print(mac[i], HEX);
    if (i != 5) {
      Serial1.print(":");
    } else {
      Serial1.println();
    }
  }

  // print the received signal strength (RSSI):
  int32_t rssi = WiFi.RSSI();
  Serial1.print("Signal strength (RSSI): ");
  Serial1.print(rssi);
  Serial1.println(" dBm");
}

int wifi_init() {
  // initialize the WiFi module:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial1.println("WiFi module not detected");
    // don't continue:
    while (true);
  }

  // print firmware version:
//  String fv = WiFi.firmwareVersion();
//  Serial1.print("Firwmare version: ");
//  Serial1.println(fv);
//
//  if (fv != "C3.5.2.3.BETA9") {
//    Serial1.println("Please upgrade the firmware");
//  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial1.print("Attempting to connect to WiFi network ");
    Serial1.print(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial1.println("...Connected.");
  return status;
  //IPAddress ip = WiFi.localIP();
  //return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  //printWifiStatus();
}

void ip2char(char* ip_arr){
  IPAddress ip = WiFi.localIP();
  sprintf( ip_arr , "%d.%d.%d.%d" , ip[0] , ip[1] , ip[2] , ip[3] );
}

