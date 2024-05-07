#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h> 
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// DATOS A CAMBIAR
const char* url = "https://www.ieslamarisma.net/marisol/update.php";
//SHA1 finger print of certificate use web browser to view and copy
//const char fingerprint[] PROGMEM = "AE E2 33 AD AE 6E D9 BE 6A FA 5B 6F C9 62 35 BB A6 43 D4 31"; // ANTERIOR
const char fingerprint[] PROGMEM = "85 99 63 4F BB 18 67 72 1E B5 98 C5 04 6C A6 4E 3A 37 AB 00"; 

//Add WIFI data
const char* ssid = "marisol";              //Add your WIFI network name 
const char* password =  "12345678";       //Add WIFI password

//Variables used in the code
String LED_id = "1";                  //Just in case you control more than 1 LED
String data_to_send = "";             //Text data to send to the server

// Tiempos de refesco entre peticiones
unsigned int Actual_Millis, Previous_Millis;
unsigned int refresh_time = 2000;     //Refresh rate of connection to website (recommended more than 1s)

// Tiempos de refesco entre parpadeo - blink
unsigned int Actual_Millis_blink, Previous_Millis_blink;
unsigned int refresh_time_blink = 1000;     //Refresh rate of connection to website (recommended more than 1s)

const int httpsPort = 443;            //HTTPS= 443


// NodeCMU Pinout https://www.luisllamas.es/esp8266-nodemcu/
const int LED = 2;                      // GPIO2 - D4  . LED
const int Rele_Pin = 0;                 // GPIO0 - D3  - Relay
// GPS
const int RX = 5;                       // GPIO5 D1 - conectado a TX NEO 6M GPS
const int TX = 4;                       // GPIO4 D2 - conectado a RX NEO 6M GPS

// Latitud y longitud
String latitude;
String longitud;    

// Led blink
bool led_blink = true;
bool led_is_on = false;

// The TinyGPSPlus object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial ss(TX, RX);

void setup() {
  Serial.begin(9600);                   //Start monitor
  ss.begin(9600);

  pinMode(LED, OUTPUT);                 // Set pin 2 as OUTPUT
  pinMode(Rele_Pin, OUTPUT);            // Set Rele_Pin as OUTPUT
  pinMode(TX, OUTPUT);                  // Set TX as OUTPUT
  pinMode(RX, INPUT);                   // Set RX as INPUT

  // Rele is off when output is HIGH
  digitalWrite(LED, HIGH);  
  digitalWrite(Rele_Pin, HIGH); 

  WiFi.begin(ssid, password);             //Start wifi connection
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) { //Check for the connection
    delay(500);
    Serial.print(".");
  }

  Serial.print("Connected, my IP: ");
  Serial.println(WiFi.localIP());

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // tiempo de refresco entre peticiones
  Actual_Millis = millis();               //Save time for refresh loop
  Previous_Millis = Actual_Millis; 
  
  // tiempo de refresco de parpadeo
  Actual_Millis_blink = millis();               //Save time for refresh loop
  Previous_Millis_blink = Actual_Millis_blink;   
}


void loop() {  
  WiFiClientSecure client;    //Declare object of class 
  //WiFiClient client;
  HTTPClient http;
  char gpsdata;

  //We make the refresh loop using millis() so we don't have to sue delay();
  // TODO añadir datos lectura GPS
  Actual_Millis = millis();
  Actual_Millis_blink = millis();
  if(Actual_Millis - Previous_Millis > refresh_time){
    Previous_Millis = Actual_Millis;
    Serial.print("Comprobando conexion ...");  
    
    if(WiFi.status()== WL_CONNECTED){                   //Check WiFi connection status  
      // conexión segura =============================================
      client.setFingerprint(fingerprint);
      client.setTimeout(15000); // 15 Seconds
      delay(1000);
  
      Serial.print("HTTPS Connecting");
      int r=0; //retry counter
      while((!client.connect("ieslamarisma.net", 443)) && (r < 3)){
        delay(100);
        Serial.print(".");
        r++;
      }
      if(r==3) {
        Serial.println("Connection failed");
      }
      else {
        Serial.println("Connected to web");
      }
      //conexion segura =============================================

      // Leemos datos GPS
      if (ss.available()) {
        gpsdata = ss.read();
        // Mostrar datos en crudo NMEA
        Serial.print(gpsdata);
        if (gps.encode(gpsdata)) {
          if (gps.location.isValid()) {
            latitude = gps.location.lat();
            longitud = gps.location.lng();
            // Mostramos en consola
            Serial.print("Location: "); 
            Serial.print(gps.location.lat(), 6);
            Serial.print(",");
            Serial.println(gps.location.lng(), 6);
          } else {
            latitude = "Invalid";
            longitud = "Invalid";
            Serial.print("Invalid");
          }
        }
      } else {
        latitude = "Not-Available";
        longitud = "Not-Available";
        Serial.print("Not-Available");
      }

      // Datos a enviar
      data_to_send = "check_LED_status=" + LED_id;    //If button wasn't pressed we send text: "check_LED_status"
      
      // Agregamos temperatura inventada
      data_to_send = data_to_send + "&temp=" + 30;
      // Agregasmos latitud y longitud
      data_to_send = data_to_send + "&lat=" + latitude;
      // Agregamos humedad
      data_to_send =  data_to_send + "&lon=" + longitud;
      
      //Begin new connection to website   
      Serial.print("--Sending POST=");    
      Serial.println(data_to_send);
      http.begin(client,url);   //Indicate the destination webpage 
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");//Prepare the header    
      int response_code = http.POST(data_to_send);                        //Send the POST. This will giveg us a response code

      String response_body = http.getString();        
      Serial.print("Server reply: ");                                     //Print data to the monitor for debug
      Serial.println(response_body);      
      
      //If the code is higher than 0, it means we received a response
      if(response_code > 0){
        Serial.println("HTTP code " + String(response_code));             //Print return code

        if(response_code == 200){                                         //If code is 200, we received a good response and we can read the echo data
          //String response_body = http.getString();                        //Save the data comming from the website
          //Serial.print("Server reply: ");                                 //Print data to the monitor for debug
          //Serial.println(response_body);

          //If the received data is LED_is_on, we set LOW the LED & Relay pin
          if(response_body == "LED_is_on"){
            led_is_on = true;
            // LED & Rele are on when output is LOW
            digitalWrite(LED, LOW);  
            digitalWrite(Rele_Pin, LOW);  
          } 
          //If the received data is LED_is_off, we set HIGH the LED & Relay pin
          if(response_body == "LED_is_off"){
            led_is_on = false;
            // LED & Rele are off when output is HIGH
            digitalWrite(LED, HIGH);  
            digitalWrite(Rele_Pin, HIGH);      
          }  
        }//End of response_code = 200
      } else { //END of response_code > 0
        Serial.print("Error sending POST, code: ");
        Serial.println(response_code);
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(response_code).c_str());
      }
      http.end(); //End the connection
    } else { //END of WIFI connected
      Serial.println("WIFI connection error");
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();     
      WiFi.begin(ssid, password);
    }
  }
}
