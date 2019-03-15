#include <sha256.h>
#include <ArduinoJWT.h>

#include <WiFiServerST.h>
#include <WiFiClientST.h>
#include <ArduinoHttpClient.h>

//#define WITH_MODBUS
//#define SECURE_ELEMENT
#define WITH_SERIAL
//#define EXT_NFC

#include <ArduinoJson.h>
#include "wifi.h"

#ifdef SECURE_ELEMENT
#include "crypino.h"
#endif

#ifdef WITH_MODBUS
#include "modbus.h"
#endif


#include <Wire.h>
#include <M24SR.h>
//#include "bigchaindb.h"

/*  NFC M24SR Module Conifguration */
#define M24SR_ADDR      0xAC

#ifdef EXT_NFC
   #define I2C2_SCL        PB8//PB10
   #define I2C2_SDA        PB9//PB11
   #define GPO_PIN         PB4//PE4
   #define RF_DISABLE_PIN  PB2//PE2
#else
  #define I2C2_SCL        PB10
  #define I2C2_SDA        PB11
  #define GPO_PIN         PE4
  #define RF_DISABLE_PIN  PE2
#endif

TwoWire dev_i2c(I2C2_SDA, I2C2_SCL);
M24SR nfcTag(M24SR_ADDR, &dev_i2c, NULL, GPO_PIN, RF_DISABLE_PIN);

//char serverAddress[] = "middleware.riddleandcode.com";
char serverAddress[] = "ipdb-eu2.riddleandcode.com";
int port = 80;

//HardwareSerial Serial3(PA1 , PA0);
//HardwareSerial Serial1(PB7 , PB6);

/*
extern http_response_t response;
extern http_request_t request;

String myget( String url)
{
  request.hostname = serverAddress;
  request.path = url;
  request.port = port;
  doGetRequest();

  return response.body;
}
*/

/*----------------------------*/
#ifdef SECURE_ELEMENT
ATCAIfaceCfg *gCfg = &cfg_ateccx08a_i2c_default;
ATCA_STATUS crypino_status = ATCA_GEN_FAIL;
#endif




WiFiClient wificlient;
WiFiServer wifiserver(80);
//HttpClient client = HttpClient(wificlient, serverAddress, port);


ArduinoJWT jwt1 = ArduinoJWT("");

//DynamicJsonBuffer jsonBuffer;
StaticJsonBuffer<5000> jsonBuffer;
JsonObject& jsonTX = jsonBuffer.createObject();

unsigned int reg[10]={0};
char msg[42]={0};
uint8_t pk[64]={0};
uint8_t pubkey[65]={0x04};
char keystr[130] = {0};
//String keystr = "";

double kwh0 = 0.0 ,kwh1 = 0.0;
String postData = "",response = "";

String txpk = "";

String byte2string(uint8_t* bytarr, uint8_t len);
void byte2char(uint8_t* bytarr, uint8_t len , char* char_array);
void hex2byte(const char* respc, uint8_t* hx, uint8_t len);

void setup() {


  // initialize serial communication:
  Serial1.begin(115200);

#ifdef WITH_SERIAL
  while (!Serial1); // wait for serial port to connect. Needed for native USB port only
#endif
//  rest.set_id("008");
//  rest.set_name("dapper_drake");
  //rest.function("balance_check",blc_check );

  Serial1.println("\n\r-----------------------------------------\n\rStart");
  

    // Intialize NFC module
  dev_i2c.begin();
  if(nfcTag.begin(NULL) != 0) Serial1.println("NFC Init failed!");
  else Serial1.println("NFC init successful.");

#ifdef SECURE_ELEMENT

while(crypino_status == ATCA_GEN_FAIL) crypino_status = atcab_init(gCfg);
//  uint8_t i = 0;
  do{
     crypino_status = atcab_get_pubkey(0, pk);
  }while(crypino_status != ATCA_SUCCESS); //while(pk[13]==0);

  // char str[] = "saeed";
  // Serial1.print(sizeof(str));

  //String pubkeystr = byte2string(pk,64);
  char pubkeystr[sizeof(pk) * 2] = {0};
  byte2char( pk , 64 , pubkeystr );

  Serial1.println("secure element:");
  Serial1.println(pubkeystr);

  uECC_compute_public_key(pk, pubkey+1 , uECC_secp256r1() );

  
  //keystr = byte2string(pubkey,65);
  byte2char( pubkey , 65 , keystr );
  Serial1.print("-----my pubkey computed:");
  Serial1.println(keystr);

#endif
#ifdef WITH_MODBUS
  modbus_init();
  kwh0 = modbus_read_kwh();
  
  Serial1.print(kwh0);
  Serial1.println(" kWh");
#endif
  
  Serial1.print("  ");
  wifi_init();
  
  char ip_arr[15]= {0};
  ip2char(ip_arr);


  Serial1.print("IP Address:");
  Serial1.println(ip_arr);
  char nfc_text[146] = {0};
  strcat(nfc_text , keystr);
  strcat(nfc_text , ",");
  strcat(nfc_text , ip_arr);
  // memcpy(nfc_text , keystr , 130);
  // nfc_text[130] = ',' ;
  // memcpy(nfc_text+131 , ip_arr , 15);
  // //String nfc_text = String(keystr) + "," + ip;
  if(nfcTag.writeTxt(nfc_text) == false) Serial1.println("NFC Write Failed!");
  else Serial1.println("Pubkey is written on the NFC");

  
  
}

int resp_read(char* response, uint32_t *response_size, uint32_t *header_size);
int get_http_2(char* path , char* response);
String get_http_ext( String path );
String get_http(String path);
long get_user_balance(char * user_pk);

String post_http(const char* path,const char* pubkey,String postData);

void loop() {
  String tkn;
  wifiserver.begin();
  WiFiClient wcl = wifiserver.available();       // listen for incoming clients

  char user_pk[129]={0};
  if(wcl){
      Serial1.println("New server available.");
      uint8_t buf[1000]={0};
      int c = 0, cur = 0;
      while(wcl.connected()){
          if(wcl.available()){
            Serial1.println("New Client.");

            do{
                c = wcl.read(buf + cur, 500);
                cur += c;
                if(c==0) break;
            }while(c != 0);

            buf[cur] = 0;
            
            Serial1.print("wifi buffer:");
            Serial1.println((char*)buf);
            char* charge = strstr((char*)buf,"GET /charge/");
            char* invoice = strstr((char*)buf,"GET /invoice/");

            if (charge != 0) {
              
              //postData = "";
              Serial1.println("output request.");
              memcpy((uint8_t*)(user_pk),(uint8_t*)(charge+20),44);
              //user_pk[128]=0;
              //Serial1.println((unsigned int)charge,HEX);

              Serial1.println(user_pk);
              //String user_pk_str = String(user_pk);
              
              long balance = get_user_balance(user_pk);
              
              Serial1.print("balance=");
              Serial1.println( balance );
              String status = "299";
              String message = "Your balance is too low.";
              if(  balance > 10 ){
                wcl.println("HTTP/1.1 201 OK");
                message = "Your balance is ok.";
                status = "201";
              }else wcl.println("HTTP/1.1 299 OK");

              wcl.println("Content-type:application/json");
              wcl.println("Connection: close");
              wcl.println();
              String pre("{ \"status\":\"");
              String message_tag("\",\"message\":\"");
              String suffix("\"}\"");
              Serial1.println(pre + status + message_tag + message + suffix );
              wcl.println( pre + status + message_tag + message + suffix );
              //wcl.println("{ \"status\":\"201\", \"message\": \"Your balance is ok.\", \"data\": \"Your balance is ok.\"}");
#ifdef WITH_MODBUS              
              kwh0 = modbus_read_kwh();
              Serial1.print(kwh0);
              Serial1.println(" kWh");
#endif              
              break;

            }else if(invoice != 0){
              Serial1.println("invoice request.");
              //float price = 0.0;
              //int price = 0;
#ifdef WITH_MODBUS
              kwh1 = modbus_read_kwh();
              Serial1.print("diff:");
              Serial1.println( (double)(kwh1 - kwh0)  );
              //float price = (kwh1 - kwh0) * 0.2;   //the real price calculator
              double price = kwh1 * 10 - kwh0 * 10;
              Serial1.println("price: "+ String(price));
              price = kwh1 * 10;
              kwh0 = kwh1;
#else
              double price = 10.0;
#endif

              wcl.println("HTTP/1.1 201 OK");
              wcl.println("Content-type:application/json");
              wcl.println("Connection: close");
              wcl.println();
              String status = "201";
              String pre("{ \"status\":\"");
              String data_tag("\",\"data\":");
              String suffix("}");
              String price_obj = "{\"price\":\"" + String((int)price) + "\"}";
              Serial1.println(pre + status + data_tag + price_obj + suffix );
              wcl.println( pre + status + data_tag + price_obj + suffix );
              //wcl.println("{\"price\":\"" + String(price) + "€\"}");
              Serial1.println("{\"price\":\"" + String((int)price) + "€\"}");;


              break;

            }
          }

      }
      delay(1);
     wcl.stop();
//      wifiserver.begin();
  }
  // wcl.stop();
  //wifiserver.begin();
  
}



//----------------------------------------------------------------------------------------------------------
long get_user_balance(char *user_pk){
  int c = 1,cur = 0;
  int retries = 0;
  char* end_pos = 0;
  char tx_id[65] = {0};
  const char prefix_path[] = "/api/v1/outputs?public_key=";
  char path[128+ sizeof(prefix_path)] = {0};
  strcat(path, prefix_path);
  strcat(path, user_pk);
  
  if(!wificlient.connected()) wificlient.connect(serverAddress, port);
  wificlient.print("GET ");
  wificlient.print(path);
  wificlient.println(" HTTP/1.1");
  wificlient.print("Host: ");
  wificlient.println(serverAddress);
  wificlient.println("Connection: Close");
  wificlient.println();

  char buf[2000]={0};
  char wbuf[1000]= {0};
  
  while(c){
    c = wificlient.read((uint8_t *)wbuf, sizeof(wbuf));
    Serial1.print(c);
    memmove( buf , buf + c , sizeof(buf) - c );
    memcpy( buf+ sizeof(buf) - c , wbuf , c );
  }
  wificlient.stop();

  Serial1.print("buf: ");
  Serial1.println((char*)buf);

  end_pos = strchr(buf , ']');
  Serial1.println((int)end_pos,HEX);
  memcpy(tx_id , end_pos - 64 - 21, 64);
  Serial1.println(tx_id);
  
  
  const char prefix_path_tx[] = "/api/v1/transactions/";
  char path_tx[64+ sizeof(prefix_path_tx)] = {0};
  strcat(path_tx, prefix_path_tx);
  strcat(path_tx, tx_id);

  if(!wificlient.connected()) wificlient.connect(serverAddress, port);
  wificlient.print("GET ");
  wificlient.print(path_tx);
  wificlient.println(" HTTP/1.1");
  wificlient.print("Host: ");
  wificlient.println(serverAddress);
  wificlient.println("Connection: Close");
  wificlient.println();
  
  //String response = get_http( String("/api/v1/transactions/") + String(tx_id) );

  char response[3000] = {0};
  //int r = get_http_2(path_tx , response);
  //Serial.println(r);
  uint32_t response_size=0;
  uint32_t header_size=0;

  resp_read(response, &response_size, &header_size);
  wificlient.stop();

  Serial1.println(response);

  jsonBuffer.clear();
  JsonObject& jsonRX9 = jsonBuffer.parseObject(&response[header_size]);
  long balance = 0;
  if(jsonRX9.success()) balance = jsonRX9["outputs"][0]["amount"].as<String>().toInt();
  return balance;
}

int get_http_2(char* path , char* response){
  if(!wificlient.connected()) wificlient.connect(serverAddress, port);
  wificlient.print("GET ");
  wificlient.print(path);
  wificlient.println(" HTTP/1.1");
  wificlient.print("Host: ");
  wificlient.println(serverAddress);
  wificlient.println("Connection: Close");
  wificlient.println();
      
  char buf[10000]={0};
  uint32_t response_size=0;
  uint32_t header_size=0;

  resp_read(buf, &response_size, &header_size);
  wificlient.stop();

  //Serial1.println(response_size);
  //Serial1.flush();
  //Serial1.print("buf: ");
  //Serial1.write(buf,4999);
  //Serial1.println(buf);
  //Serial1.peek();
  //Serial1.flush();

  
  Serial1.println("*...");
  //Serial1.peek();
  memcpy(response , buf+header_size, response_size);
  // for(int i=0; i<response_size; i++){
  //   response[i] = buf[i+header_size];
  // }
  response[response_size] = 0;
  //Serial1.println("...--");
  return response_size;
}

String get_http(String path)
{
  if(!wificlient.connected()) wificlient.connect(serverAddress, port);
  wificlient.print("GET ");
  wificlient.print(path.c_str());
  wificlient.println(" HTTP/1.1");
  wificlient.print("Host: ");
  wificlient.println(serverAddress);
  wificlient.println("Connection: Close");
  wificlient.println();
  

  char buf[3000]={0};
  uint32_t response_size=0;
  uint32_t header_size=0;

  resp_read(buf, &response_size, &header_size);
  //Serial1.print("buf: ");
  //Serial1.println(buf);

  wificlient.stop();

  String response;
  for(int i=0; i<response_size; i++){
    response.concat(buf[i+header_size]);
  }
  //Serial1.flush();
  //Serial1.println("**");
  return response;
  
}

String post_http(const char* path,const char* pubkey,String postData){
  HttpClient client = HttpClient(wificlient, serverAddress, port);
  client.beginRequest();
  client.post(path);
  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", postData.length() );
  if(pubkey != NULL ) client.sendHeader("X-Public-Key", pubkey );
  client.print(postData);
  client.endRequest();

  char buf[10000]={0};
  uint32_t response_size=0;
  uint32_t header_size=0;

  resp_read(buf, &response_size, &header_size);
  //Serial1.print("buf=");
  //Serial1.println(buf);

  String response;
  for(int i=0; i<response_size; i++){
    response.concat(buf[i+header_size]);
  }
  client.stop();
  return response;

}

int resp_read(char* response, uint32_t *response_size, uint32_t *header_size){
  uint8_t *buf = (uint8_t *)response;
  int c = 0, retries = 20, cur = 0, length;
  while (1)
    {
        c = wificlient.read(buf + cur, 1000);
        cur += c;

        if (c == 0)
        {
            if (retries-- == 0)
            {
                //buf[cur] = 0;
                break;
            }
            else
            {
                Serial1.println(retries);
            }
        }
        else
        {
            retries = 0;
        }
    }
    buf[cur] = 0;
    wificlient.flush();
    length = cur;

    // Check status
    if (memcmp("HTTP/", buf, 5) != 0 || memcmp("200", buf + 9, 3) != 0)
    {
        Serial1.println("Invalid HTTP response.");
        Serial1.println(length);
        char status_code[4] = {0};

        memcpy(status_code, buf + 9, 3);
        for (uint8_t j = 0; j < 200; j++)
        {
            Serial1.print((char)buf[j]);
        }
        return 1;
    }
    //Serial1.println(" 478 ");
    // Find where body starts
    cur = 0;
    for (uint64_t i = 0; i < length - 4; i++)
    {
        if (memcmp(&buf[i], "\r\n\r\n", 4) == 0)
        {
            cur = i + 4;
            break;
        }
    }

    if (cur == 0)
    {
        return 1;
        Serial1.println("\ncur=0");
    }

    *response_size = length - cur;
    *header_size = cur;


    return 0;

}




String byte2string(uint8_t* bytarr, uint8_t len){
  String str="";
  for (int i = 0; i < len; i++){
     static char tmp[2] = {};
     sprintf(tmp, "%02X", bytarr[i]);
     str += tmp;
  }
  return str;
}

void byte2char(uint8_t* bytarr, uint8_t len , char* char_array){
  for (int i = 0; i < len; i++)
    sprintf(&char_array[i*2], "%02X", bytarr[i]);
}

void hex2byte(const char* respc, uint8_t* hx, uint8_t len){
  int tmp1 = 0;
  int tmp2 = 0;
  for (int i = 0; i < (len*2); i++){
    if(i%2 == 0){
      if(respc[i] > 64) tmp2=9+respc[i]-64 ;
      else if(respc[i] < 58) tmp2=respc[i]-48;
    }
    else{
      if(respc[i] > 64) tmp1=9+respc[i]-64 ;
      else if(respc[i] < 58) tmp1=respc[i]-48;
      hx[i/2]= tmp2*16 + tmp1;
    }
  }

}



