#include <sha256.h>
#include <ArduinoJWT.h>

#include <WiFiServerST.h>
#include <WiFiClientST.h>
#include <ArduinoHttpClient.h>
//#include <HttpClient.h>
//#include <aREST.h>
//aREST rest = aREST();

#include <ArduinoJson.h>

#include "wifi.h"
#include "crypino.h"
#include "modbus.h"
#include "b64.h"

#include <Wire.h>
#include <M24SR.h>

/*  NFC M24SR Module Conifguration */
#define I2C2_SCL        PB10
#define I2C2_SDA        PB11
#define M24SR_ADDR      0xAC
#define GPO_PIN         PE4
#define RF_DISABLE_PIN  PE2

TwoWire dev_i2c(I2C2_SDA, I2C2_SCL);
M24SR nfcTag(M24SR_ADDR, &dev_i2c, NULL, GPO_PIN, RF_DISABLE_PIN);
/*----------------------------*/

ATCAIfaceCfg *gCfg = &cfg_ateccx08a_i2c_default;
ATCA_STATUS crypino_status = ATCA_GEN_FAIL;

char serverAddress[] = "middleware.riddleandcode.com";
int port = 8000;


WiFiClient wificlient;
WiFiServer wifiserver(80);
//HttpClient client = HttpClient(wificlient, serverAddress, port);

ArduinoJWT jwt1 = ArduinoJWT("");

DynamicJsonBuffer jsonBuffer;
JsonObject& jsonTX = jsonBuffer.createObject();


unsigned int reg[10];
char msg[42];
uint8_t pk[64]={0};
uint8_t pubkey[65]={0x04};
String keystr = "";

String byte2string(uint8_t* bytarr, uint8_t len);
String post_http(const char* path,const char* pubkey,String postData);
void hex2byte(const char* respc, uint8_t* hx, uint8_t len);

double kwh0,kwh1;
String postData,response;
 
void setup() {

  
  // initialize serial communication:
  Serial.begin(115200);
  while (!Serial) {
     // wait for serial port to connect. Needed for native USB port only
  }

//  rest.set_id("008");
//  rest.set_name("dapper_drake");
  //rest.function("balance_check",blc_check );
  
  Serial.println("\n\r-----------------------------------------\n\rStart");
  
  // Intialize NFC module
  dev_i2c.begin();
  if(nfcTag.begin(NULL) != 0) Serial1.println("NFC Init failed!");
  else Serial1.println("NFC init successful.");
  
  crypino_status = atcab_init(gCfg);  
  
  modbus_init(); 
  kwh0 = modbus_read_kwh();
  
  Serial.println(kwh0);

//  uint8_t i = 0;
  do{
     atcab_get_pubkey(0, pk);
  }while(pk[13]==0);      //while(crypino_status != ATCA_SUCCESS);

  Serial.println("secure element:");
  
  String pubkeystr = byte2string(pk,64); 
  Serial.println(pubkeystr);  
  
  uECC_compute_public_key(pk, pubkey+1 , uECC_secp256r1() );
   
  keystr = byte2string(pubkey,65);
  
  String ip = wifi_init();
  
  
  Serial.print("-----my pubkey computed:");
  Serial.println(keystr);
  Serial.print("IP Address:");
  Serial.println(ip);
  String nfc_text = keystr + "," + ip;
  if(nfcTag.writeTxt(nfc_text.c_str()) == false) Serial1.println("NFC Write Failed!");  
  else Serial.println("Pubkey is written on the NFC"); 
  
  Serial.println("-----");
  
  
  jsonTX["public_key"]= keystr;  
 
  jsonTX.printTo(postData); 
 
  response = post_http("/v1/key-exchange", 0 , postData.c_str() );

  JsonObject& jsonRX = jsonBuffer.parseObject( response );
  
  Serial.println("----server's pubkey is received:");
//  jsonRX.prettyPrintTo(Serial);

  Serial.println(jsonRX["data"]["public_key"].as<String>());

  const char* respc = jsonRX["data"]["public_key"];

  
  
  uint8_t hx[65]={0x04};
  uint8_t secret[32]={0};

//  char resp[174]={0};
//  response.toCharArray(resp,174);
  hex2byte(respc,hx,65);
  uECC_shared_secret( hx+1, pk , secret, uECC_secp256r1());

  ArduinoJWT jwt1 = ArduinoJWT("");
  jwt1.setPSK(byte2string(secret,32));

  Serial.print("\n\rsecret: ");
  Serial.println(byte2string(secret,32)); 
}

void loop() {

//  char text_read[100]={'\0'};
//  nfcTag.readTxt(text_read);
//  Serial.print("Message content: ");
//  Serial.println(text_read);
//  
//  
//  delay(1000);
//  return;

  //---------------------------------------------------------------------------------------
  String tkn;
  WiFiClient wcl = wifiserver.available();       // listen for incoming clients
  
  char user_pk[129]={0};
  if(wcl){
      uint8_t buf[1000]={0};
      int c = 0, cur = 0;
      while(wcl.connected()){
          if(wcl.available()){
            Serial.println("New Client.");       
            
            do{
                c = wcl.read(buf + cur, 500);
                cur += c;    
                if(c<500) break;                            
            }while(c != 0);
            
            buf[cur] = 0;
            char* charge = strstr((char*)buf,"GET /charge/");
            char* invoice = strstr((char*)buf,"GET /invoice/");
            if (charge != 0) {
              
              Serial.println("charge request.");
              memcpy((uint8_t*)(user_pk),(uint8_t*)(charge+12),128);
              //user_pk[128]=0;
              //Serial.println((unsigned int)charge,HEX);
              Serial.println(user_pk);
              String user_pk_str = String(user_pk);
              postData = "{\"token\":\"" + jwt1.encodeJWT(user_pk_str) + "\"}";
              response = post_http("/v1/validate" , keystr.c_str() , postData);
              jsonBuffer.clear();
              JsonObject& jsonRX8 = jsonBuffer.parseObject(response);
              tkn = jsonRX8["token"].as<String>();
              jwt1.decodeJWT(tkn,response);
              JsonObject& jsonRX9 = jsonBuffer.parseObject(response);
              if( jsonRX9["metadata"]["balance"].as<String>().toFloat() != 0.0 ){
                wcl.println("HTTP/1.1 201 OK");
              }else wcl.println("HTTP/1.1 299 OK");
              break;
              
            }else if(invoice != 0){
              
              Serial.println("invoice request.");
              kwh1 = modbus_read_kwh();
              float price = (kwh1 - kwh0) * 0.2;
              wcl.println("HTTP/1.1 201 OK");
              wcl.println("Content-type:application/json");
              wcl.println("Connection: close");
              wcl.println();
              wcl.println("{\"price\":\"" + String(price) + "€\"}}");
              Serial.println(price);
              kwh0 = kwh1;
              break;
              
            }
          }
                      
      }
      wcl.stop();
      wifiserver.begin();        
  }
  
//  if(wcl){                                  // If a new client connects,
//    Serial.println("New Client.");          // print a message out in the serial port
//    String currentLine = "";                // make a String to hold incoming data from the client
//    while (wcl.connected()) {            // loop while the client's connected
//      if (wcl.available()) {             // if there's bytes to read from the client,
//        int c = wcl.read();             // read a byte, then
//        Serial.print(buf);                    // print it out the serial monitor
//        header += c;
//        if (c == '\n') {                    // if the byte is a newline character
//          // if the current line is blank, you got two newline characters in a row.
//          // that's the end of the client HTTP request, so send a response:
//          if (currentLine.length() == 0) {
//            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
//            // and a content-type so the client knows what's coming, then a blank line:
//            
//            
//            
//            // turns the GPIOs on and off
//            
////            if (header.indexOf("GET /charge/") >= 0) {
////              int idx = header.indexOf("GET /charge/");
////              user_pk = header.substring(idx, idx +130);
////              postData = "{\"token\":\"" + jwt1.encodeJWT(user_pk) + "\"}";
////              response = post_http("/v1/validate" , keystr.c_str() , postData);
////              jsonBuffer.clear();
////              JsonObject& jsonRX8 = jsonBuffer.parseObject(response);
////              tkn = jsonRX8["token"].as<String>();
////              jwt1.decodeJWT(tkn,response);
////              JsonObject& jsonRX9 = jsonBuffer.parseObject(response);
////              if(jsonRX9["metadata"]["balance"] != "0" ){
////                wcl.println("HTTP/1.1 201 OK");
////              }else wcl.println("HTTP/1.1 299 OK");
//              wcl.println("HTTP/1.1 200 OK");
//              wcl.println("Content-type:text/html");
//              wcl.println("Connection: close");
//              wcl.println();
//              wcl.println("<!DOCTYPE HTML>");
//              wcl.println("<html>");
//              wcl.println("aaaaaaaaah");
//              wcl.println("</html>");
//              
////              check_balance(user_pk);
////              digitalWrite(output5, HIGH);
//            } else if (header.indexOf("GET /invoice/") >= 0) {
//                kwh1 = modbus_read_kwh();
//                float price = (kwh1 - kwh0) * 0.2;
//                 wcl.println("HTTP/1.1 201 OK");
//                 wcl.println("Content-type:application/json");
//                 wcl.println("Connection: close");
//                 wcl.println();
//                 wcl.println("{\"price\":\"" + String(price) + "\"€}}");
//                 Serial.println(price);
//                 kwh0 = kwh1;
////              make_invoice(user_pk);
//            }
//            
//          }
//        }
//      }
//    }
//  }  
  //rest.handle(wcl);
  return;  
  

 //-----------------------------------Get Credentials-------------------------------------------- 
  String str5 = "{}";
  postData = "{\"token\":\"" + jwt1.encodeJWT(str5) + "\"}";

  
  response = post_http("/v1/get-credentials", keystr.c_str() , postData);
  
  
  jsonBuffer.clear();
  JsonObject& jsonRX1 = jsonBuffer.parseObject(response.c_str());
  tkn= jsonRX1["token"].as<String>();
  jwt1.decodeJWT(tkn,response);
  

  jsonBuffer.clear();
  JsonObject& jsonRX2 = jsonBuffer.parseObject(response.c_str());
  //jsonRX2.prettyPrintTo(Serial);
  //JsonObject& jsonRX3 = jsonRX2["data"];
  Serial.println("\n\r---------Get Credentials:");
  jsonRX2.prettyPrintTo(Serial);

//-----------------------------------Provision--------------------------------------------
  
  String txpk = jsonRX2["data"]["public_key"].as<String>();

  postData = "{\"public_key\": \"" + txpk + "\", \"metadata\": {\"Kilowatthours\":\"" + String(msg) + "\"}}";

  Serial.print("postData: ");
  Serial.println(postData);

  
  
  postData = "{\"token\":\"" + jwt1.encodeJWT(postData) + "\"}";
  

  response = post_http("/v1/provision" , keystr.c_str() , postData);
  jsonBuffer.clear();
  JsonObject& jsonRX4 = jsonBuffer.parseObject(response);
  //jsonRX4.prettyPrintTo(Serial);
  tkn= jsonRX4["token"].as<String>();
  jwt1.decodeJWT(tkn,response);

  jsonBuffer.clear();
  JsonObject& jsonRX5 = jsonBuffer.parseObject(response.c_str());
  Serial.println("\n\r---------Provision:");
  jsonRX5.prettyPrintTo(Serial);


//-----------------------------------Validation--------------------------------------------  
  postData ="{\"public_key\":\"" + txpk + "\"}";
  postData = "{\"token\":\"" + jwt1.encodeJWT(postData) + "\"}";
  response = post_http("/v1/validate" , keystr.c_str() , postData);
  jsonBuffer.clear();
  JsonObject& jsonRX6 = jsonBuffer.parseObject(response);
  tkn = jsonRX6["token"].as<String>();
  jwt1.decodeJWT(tkn,response);
  JsonObject& jsonRX7 = jsonBuffer.parseObject(response);
  Serial.println("\n\r---------Validation:");
  jsonRX7.prettyPrintTo(Serial);




  
  delay(5000);
  
}



//----------------------------------------------------------------------------------------------------------


String post_http(const char* path,const char* pubkey,String postData){
  HttpClient client = HttpClient(wificlient, serverAddress, port);
  client.beginRequest();
  client.post(path);
  client.sendHeader("Content-Type", "application/json");  
  client.sendHeader("Content-Length", postData.length() );
  if(pubkey != NULL ) client.sendHeader("X-Public-Key", pubkey );
  client.print(postData);
  client.endRequest();

   
  char buf[1000]={0};
  uint32_t response_size=0;
  uint32_t header_size=0;
  resp_read(buf, &response_size, &header_size);
  //Serial.print("buf=");
  //Serial.println(buf);
  
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
//  Serial.println(" line417 ");
  do
    {
        c = wificlient.read(buf + cur, 100);
        cur += c;    
        
//        Serial.print("cur = ");
//        Serial.println(cur);
        if(c<100) break;
                    
    }while(c != 0);

    
    buf[cur] = 0;

//    wificlient.flush();
    length = cur;

    // Check status
    if (memcmp("HTTP/", buf, 5) != 0 || memcmp("200", buf + 9, 3) != 0)
    {
        Serial.println("Invalid HTTP response.");
        Serial.println(length);
        char status_code[4] = {0};

        memcpy(status_code, buf + 9, 3);
        for (uint8_t j = 0; j < 200; j++)
        {
            Serial.print((char)buf[j]);
        }
        return 1;
    }
    //Serial.println(" 478 ");
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
        Serial.println("\ncur=0");
    }

    *response_size = length - cur;
    *header_size = cur;

 
    return 0;
    
}

void post_http2(const char* path,const char* pubkey,String postData){
  HttpClient client = HttpClient(wificlient, serverAddress, port);
  client.beginRequest();
  client.post(path);
  client.sendHeader("Content-Type", "application/json");  
  client.sendHeader("Content-Length", postData.length() );
  if(pubkey != NULL ) client.sendHeader("X-Public-Key", pubkey );
  client.print(postData);
  client.endRequest();
  //client.stop(); 
  //Serial.println("waiting for response2");
  
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
