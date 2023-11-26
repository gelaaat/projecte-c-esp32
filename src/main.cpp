#include <Arduino.h>
#include <Wire.h>
#include <Wifi.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>
#include <Math.h>
#include <soc/rtc.h>
extern "C"{
  #include <esp_clk.h>
}

//Variables que necessitarem per connexió WiFi

//WiFiServer server(80);

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 5

RTC_DATA_ATTR uint64_t sleepTime; //Aquesta variable és per mesurar el temps que està dormint
                                  //La guardem a RTC_DATA per a que no s'esborri mentre està dormint
int8_t BME= 0x76;

uint64_t wakeup_reason=esp_sleep_get_wakeup_cause(); 


//Declarem variables per a funcions d'abans del setup
double temperatura;
double pressio;
double humitat;
uint32_t p;
uint32_t altitud;
float IRMS;
float h;
uint32_t durada; //La durada del sleepTime
uint32_t a;

float llegir_corrent(){ //Aquesta funció retorna el corrent eficaç llegit per la pinça
  float voltatge=0;
  uint16_t Valorsensor=0;
  uint16_t Valorsensoract=0;
  float corrent=0;
  long tempsmostreig=millis();

  while(millis()-tempsmostreig<100){
    Valorsensoract=analogRead(GPIO_NUM_34);

    if(Valorsensor<Valorsensoract){           //Busquem un valor més alt fins arribar al pic
      Valorsensor=analogRead(GPIO_NUM_34);

      voltatge=analogRead(GPIO_NUM_34)*(3.3/4095); //Definim com llegirà el analog read amb la seva resolució
      corrent=abs(voltatge-1.51)*(5/1.51);   //El 4.2854 és el guany que ha de tenir l'ona de 1.65V fins al valor de 7.07A
      delay(1);
    }

  }

  return(corrent); 
}


//Wifi parameters
const char* ssid = "MSI_GELAAAT";
const char* password = "Mx83@983";
WiFiServer server(80);

//Websocket  parameters
using namespace websockets;
const char* websockets_server_host = "ws://192.168.137.1:8000"; //Canvieu per la vostra adreça IP de la zona WiFi
WebsocketsClient websocket_client;

void setup() {
  Serial.begin(115200);
  
  //Configurem el senyal que li donarem al switch MAX
  pinMode(GPIO_NUM_32,OUTPUT) ;    
  digitalWrite(GPIO_NUM_32,HIGH); //Donem alimentació al pin 32
  Wire.begin(SDA,SCL,1000);
  Serial.println("");
  Serial.println("He despertat");

  uint64_t tempsActual;
  tempsActual=rtc_time_slowclk_to_us(rtc_time_get(),esp_clk_slowclk_cal_get()); //Llegim el temps actual
  durada=tempsActual-sleepTime; //Restem el tempsActual al sleepTime, així sabrem la diferència que és la durada d'aquest SleepTime
  
  //Wifi
  WiFi.mode(WIFI_STA); //Definim el mode de WiFi a Access Point
  WiFi.begin(ssid, password);
  Serial.print("Conectando a:\t");
  Serial.println(ssid);
  // Esperar a que nos conectemos
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print('.');
  }
  
  Serial.println("");
  Serial.print("Connectat a:\t");
  Serial.println(WiFi.SSID()); 
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  server.begin();
  

  Serial.printf("S'ha assignat a ESP32 la IP %s" " \n", WiFi.localIP().toString().c_str());


  

  Wire.beginTransmission(BME); //Configurem el ctrl_meas
  Wire.write(0xF2);
  Wire.write(0x01);
  Wire.endTransmission();

  //Configurem el BME per a indoor measurament
  Wire.beginTransmission(BME);
  Wire.write(0xF4); //Posicionem a ctrl_meas
  Wire.write(0x57); // Decidim oversampling i mode
  Wire.write(0x10); //temps standby i filtre
  Wire.endTransmission();

  //Per configurar el ctrl_meas, haurem de configurar el oversampling de la humitat abans
  //segons el datasheet. Aquesta configuració ho fa el codi anterior

 


}


void loop() {

  // Aagafem els trimming parametres
  Wire.beginTransmission(BME);
  Wire.write(0x88);
  Wire.endTransmission();
  Wire.requestFrom(BME,24);

  uint16_t dig_T1 =Wire.read();
           dig_T1|=Wire.read()<<8;
  int16_t dig_T2= Wire.read();
          dig_T2|= Wire.read()<<8;
  int16_t dig_T3= Wire.read();
          dig_T3|=Wire.read()<<8;

  uint16_t dig_P1 = Wire.read();
           dig_P1|=Wire.read()<<8;
  int16_t dig_P2=Wire.read();
           dig_P2|=Wire.read()<<8;
  int16_t dig_P3 =Wire.read();
           dig_P3|=Wire.read()<<8;
  int16_t dig_P4= Wire.read();
           dig_P4|= Wire.read()<<8;
  int16_t dig_P5= Wire.read();
           dig_P5|=Wire.read()<<8;
  int16_t dig_P6 =Wire.read();
           dig_P6|=Wire.read()<<8;
  int16_t dig_P7= Wire.read();
           dig_P7|= Wire.read()<<8;
  int16_t dig_P8= Wire.read();
           dig_P8|=Wire.read()<<8;
  int16_t dig_P9= Wire.read();
           dig_P9|=Wire.read()<<8;

  Wire.beginTransmission(BME);
  Wire.write(0xA1);
  Wire.endTransmission();
  Wire.requestFrom(BME,1);
  unsigned char dig_H1 = Wire.read();

  Wire.beginTransmission(BME);
  Wire.write(0xE1);
  Wire.endTransmission();
  Wire.requestFrom(BME,8);

  int16_t dig_H2 = Wire.read();
           dig_H2|=Wire.read()<<8;
  unsigned char dig_H3=Wire.read();

  int16_t dig_H4 =Wire.read()<<8;
  uint8_t  dig_H4s=Wire.read(); //E5
  uint8_t auxiliarH4s=dig_H4s<<4;
           dig_H4|=auxiliarH4s;
           dig_H4=dig_H4>>4;
          
  int16_t dig_H5= Wire.read()<<8; //E6
           dig_H5|=dig_H4s;
           dig_H5>>=4;
  signed char dig_H6= Wire.read();


  //*****************TEMPERATURA*****************//

  Wire.beginTransmission(BME);
  Wire.write(0xF7);
  Wire.endTransmission();
  Wire.requestFrom(BME,8);

  //Com que el datasheet recomana que agafem les dades seguides, aprofitarem per agafar les de la pressió i humitat
  uint8_t press_MSB = Wire.read();
  uint8_t press_LSB = Wire.read();
  uint8_t press_xLSB = Wire.read();
  uint8_t temp_MSB = Wire.read();
  uint8_t temp_LSB = Wire.read();
  uint8_t temp_xLSB = Wire.read();
  uint8_t hum_MSB = Wire.read();
  uint8_t hum_LSB = Wire.read();  

  //Càlcul per a TEMPERATURA segons datasheet
  int32_t var1=0, var2=0;
  int32_t t_fine;
  

  uint32_t adc_T=temp_MSB<<16;
  adc_T|=temp_LSB<<8;
  adc_T|=temp_xLSB;
  adc_T=adc_T>>4; //Eliminem els 0's que no ens interesen dels bits de la temperatura

  var1 = ((((adc_T>>3) - (dig_T1<<1))) * (dig_T2)) >> 11;
  var2 = (((((adc_T>>4) - (dig_T1)) * ((adc_T>>4)- (dig_T1)))>> 12) *(dig_T3)) >> 14;
  t_fine = var1 + var2;
  long T = (t_fine * 5 + 128) >> 8;
  temperatura=  T/100.;
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" ºC");



  //*****************PRESSIÓ*****************//

  int32_t adc_P=press_MSB<<16;
  adc_P|=press_LSB<<8;
  adc_P|=press_xLSB;
  adc_P>>=4; //Eliminem els 0's que no ens interesen dels bits de la pressió

  uint32_t p;
  int32_t var3, var4;

  var3 = (((int32_t)t_fine)>>1) - (int32_t)64000;
  var4 = (((var3>>2) * (var3>>2)) >> 11 ) * ((int32_t)dig_P6); 
  var4 = var4 + ((var3*((int32_t)dig_P5))<<1);
  var4 = (var4>>2)+(((int32_t)dig_P4)<<16);
  var3 = (((dig_P3 * (((var3>>2) * (var3>>2)) >> 13 )) >> 3) + ((((int32_t)dig_P2) * var3)>>1))>>18;
  var3 =((((32768+var3))*((int32_t)dig_P1))>>15);

  if (var3 == 0)
  {
    Serial.println("Lectura deshabilitada"); // Evites la divisió per 0
  }

  p = (((uint32_t)(((int32_t)1048576)-adc_P)-(var4>>12)))*3125; 

  if (p < 0x80000000)
  {
    p = (p << 1) / ((uint32_t)var3);
  }

  else
  {
    p = (p / (uint32_t)var3) * 2;
  }

  var3 = (((int32_t)dig_P9) * ((int32_t)(((p>>3) * (p>>3))>>13)))>>12; 
  var4 = (((int32_t)(p>>2)) * ((int32_t)dig_P8))>>13;

  p = (uint32_t)((int32_t)p + ((var3 + var4 + dig_P7) >> 4)); 

  pressio=(p/1000.);     //*********************KILO******************//
  Serial.print("Pressió: ");
  Serial.print(pressio);
  Serial.println(" kPa");



  //*****************HUMITAT*****************//

  uint16_t adc_H=hum_MSB<<8;

  adc_H|=hum_LSB;
  float h;
  if(adc_H==0x8000){
    Serial.println("La lectura no esta habilitadaaa");
  }

  int32_t var5;

  var5 = (t_fine - (int32_t(76800)));
  var5 = (((((adc_H << 14) - ((dig_H4) << 20) - ((dig_H5) * var5)) + ((int32_t)16384)) >> 15) * (((((((var5 *(dig_H6)) >> 10) * (((var5 * (dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * (dig_H2) + 8192) >> 14));
  var5 = (var5 - (((((var5 >> 15) * (var5 >> 15)) >> 7) * (dig_H1)) >> 4));
  var5 = (var5 < 0 ? 0 : var5);
  var5 = (var5 > 419430400 ? 419430400 : var5); 
  h=(var5>>12);
  humitat=(h/1024.0);
  Serial.print("Humitat: ");
  Serial.print(humitat);
  Serial.println(" %");


  //*****************ALTITUD*****************//
  //Per el càlcul de l'altura, si la pressió és més gran que la pressió del mar, la órmula proporciona
  //un resultat descabellat, per tant, si la pressió és més alta, l'altura serà igual a 0

  if(pressio>1013.25){
    altitud=0;
    Serial.print("Altitud: ");
    Serial.print(altitud);
    Serial.println(" m");
  }

  if(pressio<1013.25){
    altitud=44330*(1-pow(((p/100.)/1013.25),(1/5.255)));
    Serial.print("Altitud: ");
    Serial.print(altitud);
    Serial.println(" m");
  }


  //*****************CONSUM*****************//
  //Llegim el senyal de la pinça que ve donada per la funció de llegir_corrent(); definida a dalt del setup
  IRMS=llegir_corrent();
  Serial.print("Intensitat: ");
  Serial.print(IRMS);
  Serial.println(" A");


  //*****************WiFi*****************//
  // try to connect to Websockets server
  bool connected = websocket_client.connect(websockets_server_host);

  if(connected) {
    Serial.println("Websocket connected!");

    String str = String("{\"temperatura\":") + String(temperatura) + ", \"humitat\":" + String(humitat) +  ", \"pressio\":" + String(pressio) + ", \"altitud\":" + String(altitud) + ", \"IRMS\":" + String(IRMS) + ", \"durada\":"+ String(durada/uS_TO_S_FACTOR) + "}";
    websocket_client.send(str);
    delay(500);
  } else {
    Serial.println("Websocket not Connected!");
  }


  //*****************DeepSleep*****************//
  //És hora posar l'ESP32 a dormir, primer definim com es pot aixecar i que iniciï el timer

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35,1);  //Definim aixecar-se per polsador abans d'entrar als if's, 

  sleepTime=rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());

  if(wakeup_reason==4){ //Es desperta per timer

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR); //Definim el temps que ha de dormir
    Serial.println("Vaig a dormir");
    Serial.flush();
    esp_deep_sleep_start();

  }

  int64_t tmr2=millis();
  if(wakeup_reason==2){  //Es desperta per polsador

    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR-durada-tmr2*1000);  //Restem la dura del SleepTime al temps que ha de dormir
    Serial.println("Vaig a dormir");                                     //per tal d'aconseguir un wakeup time de 5 segons cíclic
    Serial.flush();  //Serial flush és per abans d'anar a dormmir, executi totes les tasques anteriors
    esp_deep_sleep_start();
  }

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR-tmr2*1000); //Afegim això abans d'anar al deepSleep perquè després de
  Serial.println("Vaig a dormir");                             //bolcar el programa no es despertarà ni per timer ni per polsador
  Serial.flush();
  esp_deep_sleep_start();


}