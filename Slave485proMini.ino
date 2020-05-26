//23-02-2020   -Arduino Pro Mini- SLAVE RS485
//Started on 17-05-2017

//--- V E D I   N O T A  (1)

/* COLLOQUIO   (A)     
         Master485WeMos              Slave485ProMini
           ?x?x             --->
                            <---        !!xaa.bb;cc;dd;qqqq;ee>

 x      byte che identifica la stazione che il master vuole interrogare 
 aa.bb  Lettura sensore DS18B20
 cc     Lettura umidita da DHT11
 dd     Lettura temperatura da DHT11 (nessuna cifra decimale 
 qqqq   Lettura sensore di fumo MQ2 AI0
 rrrr   Lettura LM35 sensore temperatura 
 ee     Stato 0 oppure 1 di ciascuno dei rele di stazione Rele1(pin7) e Rele2(pin8)

              !!428.43;62;28;542;239;01
              !!^  ^  ; ^; ^; ^ ; ^ ; |---Rele1 LOW  Rele2 HIGH
              !!|  |  ; |; |; | ; |-------Lettura A1 LM35                                   
              !!|  |  ; |; |; |-----------Lettura A0 sensore fumi MQ2
              |||  |  ; |; |--------------Temperatura da DHT11
              |||  |  ; |-----------------Umidita da DHT11
              |||  |----------------------Temperatura da DS18B20 xx.yy
              |||-------------------------ID stazione slave
              ||--------------------------Carattere inizio ricezione
              |---------------------------Carattere inizio ricezione
*/
//  200223  Stazione5 DS18B20 e LM35 funzionano insieme
//	200121	Stazione5 caldaia con sonda LM35 per DS18B20 rotta fino al 200222
//  170712  Aggiunta gestione dei 2 rele da comando remoto su bus 485
//  180105  Aggiunta lettura Analog input AI1
//  180109  Risolta situazione relè con EEprom che contiene INTERI

/* COLLOQUIO   (B)     
         Master485WeMos              Slave485ProMini
           Cxfg             --->

 x      byte che identifica la stazione a cui il master invia il comando
 f      0 oppure 1 comando Rele1 
 g      0 oppure 1 comando Rele2
*/

/*
  a)  Scegliere come scheda su IDE 1.6.5 Arduino Pro or Pro Mini
  b)  Collegare con spezzoni maschio-femmina la scheda Arduino-Uno originale,
        a cui ho tolto lo ATmega328 dallo zoccolo al connettore FTDI del ProMini
        come segue:  ArduinoUNO RX(0) -> ProMini FTDI RXI
                                TX(1) ->              TX0
                                GND   ->              GND
                                5v    ->              Vcc
                                RESET ->              GRN
  c)  In alcune stazioni (es. ID3) il caricamento dello sketch puo avvenire solo se si
      stacca la scheda Pro-Mini dallo zoccolo su cui sono collegati i sensori e lo SN75176.
  d)  Dalla directory delle libraries (C:\MyArduinoWorks\librariess)
        spostare momentaneamente fuori la cartella <espsoftwareserial-master>
        che attiene alla libreria SoftwareSerial per ESP8266 e non per AVR.
        Eventualmente riavviare lo ide.
  e)  Ricordarsi di cambiare il valore di SlaveID, poichè ogni stazione ha codice unico
 */


//Misura corrrente da ACS712-20 su slave5 (caldaia)
// http://henrysbench.capnfatz.com/henrys-bench/arduino-current-measurements/acs712-arduino-ac-current-tutorial/
// http://www.instructables.com/id/Simplified-Arduino-AC-Current-Measurement-Using-AC/
// https://github.com/muratdemirtas/ACS712-arduino-1  (libreria Arduino  ACS712-arduino-1-master.zip)
// https://forum.arduino.cc/index.php?topic=171367.0


//-------------------Defines
#define IdleSt  0           //-Stato di riposo
#define ReqSt1  1           //-Primo carattere '?' ricevuto
#define ReqSt2  2           //-Secondo carattere SlaveID coincidente con quello di questa stazione
#define ReqSt3  3           //-Terzo carattere '?' ricevuto
#define CmdSt1  8           //-Primo carattere 'C' ricevuto (comando)(B)
#define CmdSt2  9           //-Secondo carattere SlaveID coincidente con quello di questa stazione (comando)(B)
#define CmdSt3  10          //-Terzo carattere 0 oppure 1 comando Rele1 (comando)(B)
#define SentSt  4           //-Tempo minimo tra 2 risposte consecutive
#define Speed   1200        //bps di comunicazione

#define MaxTalkTime 6000   //Massimo tempo di colloquio
// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor


//-------------------Included Librarie 
#include <SoftwareSerial.h>     //La libreria SoftwareSerial è quella per AVR
#include <OneWire.h>            //per DS18B20, DHT11
#include "DHT.h"                //per il DHT11 sensore temperatura & umidità 
//gestione della EEprom https://www.arduino.cc/en/Tutorial/EEPROMWrite
#include <EEPROM.h>             //the EEPROM sizes are powers of two


//-------------------collegamenti dei devices
// Pin della porta seriale software per la comunicazione con SN75176 (RX d3) (TX d2)
#define Rx485 3
#define Tx485 2
#define ReDe  4
SoftwareSerial Serial485(Rx485,Tx485);
//Pin del sensore di temperatura
#define ds18_pin  5
#define DHTPIN  6 
#define MQ2  A0        //Lettura sensore fumo MQ2 su pin A0
#define LM35 A0        //Lettura LM35 sensore temperatura su stazione 5 (Caldaia)
#define Rele1   7
#define Rele2   8


//++++++++++++++++++    Different For each Slave station
const char SlaveID = '5'; //---I M P O R T A N T E,  cambiare per ogni stazione slave  (1)
#define NOTout  //  <--Per le stazioni con scheda rele Gaoxing Tech. (1,3,4,5,6)
                //  Sulla stazione 2 (dinner) uso la mia schedina in logica npn senza inversione.
//#define DHT11x2   // (1) In alcune stazioni il DHT11 segna la metà del dovuto (Es. Stazione1)
//++++++++++++++++++


//-------------------global variables declarations
byte InByte;
byte Stato;
long previous;
float fTempDS18;        
String dummyStr;
int8_t dhtCelsius, dhtRh;  
char Buffer[10];
int8_t ByteCount;       //contatore caratteri provenienti da bus 485
int  MemRele;           //Aux Memoria stato rele in EEprom

/*
 * Senza ByteCount quando il processore è in ascolto sulla seriale, ricevendo
 * anche i caratteri trasmessi da altri slave, nel frattempo interrogati, può
 * interpretare erroneamente di essere stato interrogato lui. Per questo, al
 * primo '?' ricevuto ByteCount viene azzerato e si attendono solo 4 caratteri 
 * dal bus per decidere se rispondere oppure no.
 */

//-------------------Classes
OneWire ds(ds18_pin);
DHT MyDht(DHTPIN, DHTTYPE);


//-------------------------StartUP routine
void setup(){
  Serial.begin(Speed);          // Open native serial communications:
  Serial485.begin(Speed);       // comunicazione con SN75176
  pinMode(ReDe, OUTPUT);        // SN75176  High TX   Low RX
  pinMode(LED_BUILTIN, OUTPUT); // initialize digital pin LED_BUILTIN as an output.
  digitalWrite(ReDe,LOW);       //<-- In ascolto sulla seriale software
  pinMode(Rele1, OUTPUT);
  pinMode(Rele2, OUTPUT);
  Serial485.listen();
  Stato = IdleSt;
  MemRele = EEPROM.read(0);
  Serial.print("MemRele= ");
  Serial.println(MemRele);
  Serial.print("ID:  ");
  Serial.println(SlaveID);
  
  #ifndef  NOTout
//Stato rele1 richiamato da EEprom locazione 0
  digitalWrite(Rele1,LOW);
  if(bitRead(MemRele,0)) digitalWrite(Rele1,HIGH);
//Stato rele2 richiamato da EEprom locazione 0
  digitalWrite(Rele2,LOW);
  if(bitRead(MemRele,1)) digitalWrite(Rele2,HIGH);
  #endif

  #ifdef  NOTout
//Stato rele1 richiamato da EEprom locazione 0
  digitalWrite(Rele1,HIGH);
  if(bitRead(MemRele,0)) digitalWrite(Rele1,LOW);
//Stato rele2 richiamato da EEprom locazione 0
  digitalWrite(Rele2,HIGH);
  if(bitRead(MemRele,1)) digitalWrite(Rele2,LOW);
  #endif

  delay(1000);
  MyDht.begin();  
  Serial.println("From 485---");
//Only for diagnostic
//  fTempDS18 = ReadFromDS18B20();
//  Serial.println(dtostrf(fTempDS18,2,2,Buffer));  
}


//-------------------------Cyclic Jobs
void loop(){
  if(Serial485.available()){
    InByte = Serial485.read();
    Serial.print(char(InByte));
    ByteCount++;
    if (Stato == IdleSt && InByte == '?'){
      previous = millis();
      Stato = ReqSt1;          //<--Primo byte '?' ricevuto correttamente
      ByteCount = 0;
    }
    if (Stato == IdleSt && InByte == 'C'){
      previous = millis();
      Stato = CmdSt1;         //<--Primo byte 'C' ricevuto correttamente  (comando)(B)
      ByteCount = 0;
    }
    if(Stato == ReqSt1 && InByte == SlaveID){
      previous = millis();
      Stato = ReqSt2;         //<--Secondo byte -SlaveID- ricevuto correttamente
    }
    if (Stato == ReqSt2 && InByte == '?'){
      previous = millis();
      Stato = ReqSt3;         //<--Terzo byte '?' ricevuto correttamente
    }
    if(Stato == ReqSt3 && InByte == SlaveID && ByteCount == 3){
      MyAnswer();
      previous = millis();
      Stato = SentSt;         //<--Risposta inviata. Attendo fino a MaxTalkTime
    }
    if(Stato == CmdSt1 && InByte == SlaveID && ByteCount == 1){
      previous = millis();
      Stato = CmdSt2;         //<--Secondo byte -SlaveID- ricevuto correttamente  (comando)(B)
    }
    if(Stato == CmdSt2 && ByteCount == 2){
      if(InByte == '1'){
        #ifndef NOTout 
        digitalWrite(Rele1,HIGH);
        #endif
        #ifdef NOTout
        digitalWrite(Rele1,LOW);
        #endif      
        bitSet(MemRele,0);
        Stato = CmdSt3;           //<--Terzo byte Rele1 ON (B)
      }  
      if(InByte == '0'){
        #ifndef NOTout
        digitalWrite(Rele1,LOW); 
        #endif
        #ifdef NOTout
        digitalWrite(Rele1,HIGH);
        #endif
        bitClear(MemRele,0);
        Stato = CmdSt3;           //<--Terzo byte Rele1 OFF (B)
      }  
      //else Stato = IdleSt;      //<--Il terzo carattere deve essere '1' oppure '0' 
    }
    if(Stato == CmdSt3 && ByteCount == 3){
      if(InByte == '1'){
        #ifndef NOTout
        digitalWrite(Rele2,HIGH);  
        #endif
        #ifdef NOTout
        digitalWrite(Rele2,LOW);
        #endif
        bitSet(MemRele,1);
        WriteEE(0,MemRele);
      }
      if(InByte == '0'){
        #ifndef NOTout
        digitalWrite(Rele2,LOW); 
        #endif
        #ifdef NOTout
        digitalWrite(Rele2,HIGH);
        #endif
        bitClear(MemRele,1);
        WriteEE(0,MemRele);
        
      }
      Stato = IdleSt;
    }  
  }
//Ricezione errata: si ritorna a riposo  
  if(((millis() - previous) > MaxTalkTime) && (Stato > IdleSt)){
    Stato = IdleSt; 
  }
//  float Ampere = ReadAmpere(A1, 30, false, 5, 2.5, 100); //pin, num letture, AC/DC, Vcc, Zero, sensibilità
//  Serial.println(Ampere);
}



//°°°°°°°°°°°°°°°°°°°°°°°°°°°°° S U B R O U T I N E S °°°°°°°°°°°°°°°°°°°
//////////////////
void MyAnswer(){
  Serial.print("  wait...");
  delay(500); //attesa che il bus sia libero
  fTempDS18 = ReadFromDS18B20();
  ReadFromDHT11();
  digitalWrite(LED_BUILTIN,HIGH);
  digitalWrite(ReDe,HIGH);        //<-- trasmetto sulla seriale software 
  
  delay(500);  
  dummyStr = "!!";
  dummyStr += SlaveID ;
  Serial.print(dummyStr);         //<-- Only for diag
  Serial485.print(dummyStr);

//Temperatura da DS18B20
  delay(500);   
  dummyStr = dtostrf(fTempDS18,2,2,Buffer); 
  dummyStr += ";";
  Serial.print(dummyStr);         //<-- Only for diag
  Serial485.print(dummyStr);

//umidita da DHT11
  delay(500);     
  dummyStr = String(dhtRh);
  dummyStr += ";";
  Serial.print(dummyStr);         //<-- Only for diag
  Serial485.print(dummyStr);

//Temperatura da DHT11
  dummyStr = String(dhtCelsius);
  dummyStr += ";";  
  Serial.print(dummyStr);         //<-- Only for diag
  Serial485.print(dummyStr);

//Lettura analogica LM35 temperatura su AnalogInput0  (per stazione 5 caldaia)
//      VOUT = 10 mv/°C × T
  delay(200);
  int val = analogRead(LM35);
  float mv = ( val/1024.0)*5000;
  dummyStr = String(mv/10);       //<--Celsius
  /*  float cel = mv/10;
      float farh = (cel*9)/5 + 32;
  */
  dummyStr += ";"; 
  Serial.print(dummyStr);         //<-- Only for diag
  Serial485.print(dummyStr);
  
//Lettura analogica AI1 spare su AnalogInput1  
  delay(200);
  dummyStr = String(analogRead(A1));
  dummyStr += ";"; 
  Serial.print(dummyStr);         //<-- Only for diag
  Serial485.print(dummyStr);

//Stato del Rele2 su pin8
  #ifndef NOTout
  if(digitalRead(Rele2)){
    dummyStr += "1";
  }
  else {
    dummyStr += "0";
  }
//Stato del Rele1 su pin7
  if(digitalRead(Rele1)){
    dummyStr += "1";
  }
  else {
    dummyStr += "0";
  }
  #endif
//-------------------------------------
  #ifdef NOTout
  if(digitalRead(Rele2)){
    dummyStr += "0";
  }
  else {
    dummyStr += "1";
  }
//Stato del Rele1 su pin7
  if(digitalRead(Rele1)){
    dummyStr += "0";
  }
  else {
    dummyStr += "1";
  }
  #endif

  
//Carattere chiusura stringa di risposta
  dummyStr += ">";
  Serial.println(dummyStr);         //<-- Only for diag
  Serial485.println(dummyStr);

  delay(500);
  digitalWrite(ReDe,LOW);       //<-- in ricezione sulla seriale software  
  digitalWrite(LED_BUILTIN,LOW);
}


//////////////////
float ReadFromDS18B20(){
//returns the temperature from one DS18B20 in DEG Celsius
  byte data[12];
  byte addr[8];
  if ( !ds.search(addr)) {
      //no more sensors on chain, reset search
      ds.reset_search();
      return 99;
  }
  if ( OneWire::crc8( addr, 7) != addr[7]) {
      //Serial.println("CRC is not valid!");
      return 99;
  }
  if ( addr[0] != 0x10 && addr[0] != 0x28) {
      //Serial.print("Device is not recognized");
      return 99;
  }
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1); 
// start conversion, with parasite power on at the end 
  byte present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE); // Read Scratchpad
// we need 9 bytes 
  for (int i = 0; i < 9; i++) { 
    data[i] = ds.read();
  }
  ds.reset_search();
  byte MSB = data[1];
  byte LSB = data[0];
//using two's compliment 
  float tempRead = ((MSB << 8) | LSB); 
  float TemperatureSum = tempRead / 16;
  return TemperatureSum;
}


//////////////////
void  ReadFromDHT11(){
// Reading temperature or humidity takes about 250 milliseconds!
// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  #ifndef DHT11x2
  dhtRh = MyDht.readHumidity();
  #endif
  #ifdef  DHT11x2
  dhtRh = MyDht.readHumidity()*2;
  #endif
  if(dhtRh < 10) dhtRh = 99;
// Read temperature as Celsius 
  dhtCelsius = MyDht.readTemperature();  
  if(dhtCelsius < 10) dhtCelsius = 99;
  // Check if any reads failed and exit early (to try again).
  if (isnan(dhtRh) || isnan(dhtCelsius)) {
    Serial.println("DHT error");
  }
}


//////////////////  https://forum.arduino.cc/index.php?topic=171367.0
float ReadAmpere(byte apin, unsigned int cycle, boolean current, float vcc, float zero, float sens) {
  float amp = 0;
  if (current == true){
    for(int i = 0; i < cycle; i++){
      amp += analogRead(apin);
      delay(1);
    }
    amp /= cycle;  
  }
  else {
    for(int i = 0; i < cycle; i++){
      amp = max(analogRead(apin), amp);
      delay(1);
    }
    amp /= 0.707106;  
  }
  amp = (vcc * amp/1023.0 - zero)/(sens/1000.0);
  return amp;
}


//////////////////http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html#eeprom
void WriteEE(int loc,int value){
  EEPROM.write(loc,value);      //<--aggiorna la EEprom con lo stato dei rele  
}

