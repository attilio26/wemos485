//02-04-2020  -Arduino WeMos- MASTER RS485
//Richiede parametri ambientali a stazioni remote su bus RS485 con SN75176.
//Started on 22-05-2017
//Compiled with IDE 1.6.5

/*170702 
 --PRIMO TENTATIVO: (NON funziona)
  Aggiunta libreria Blynk  http://www.blynk.cc/getting-started/
  I folders di Blynk sono:
    C:\MyArduinoWorks\libraries\Blynk
    C:\MyArduinoWorks\libraries\BlynkESP8266_Lib
    C:\MyArduinoWorks\tools\
  da file ----Blynk_Release_v0.4.8.zip----

================
  170705
 --SECONDO TENTATIVO: (OK funziona) 
  Aggiunta la libreria Blynk  https://github.com/blynkkk/blynk-library
  Il folder della libreria e:
   C:\MyArduinoWorks\libraries\blynk-library-master
  da file   ----blynk-library-master.zip---- 
https://www.hackster.io/34909/wifi-security-system-using-wemos-d1mini-esp8266-and-blynk-56c703
*/

/* COLLOQUIO   (A)     
         Master485WeMos              Slave485ProMini
           ?x?x             --->
                            <---        !!xaa.bb;cc;dd;qqqq;rrrr;ee>

 x      byte che identifica la stazione che il master vuole interrogare 
 aa.bb  Lettura sensore DS18B20
 cc     Lettura umidita da DHT11
 dd     Lettura temperatura da DHT11 (nessuna cifra decimale) 
 qqqq   Lettura sensore di fumo AI0 (AnalogInput A0)
 rrrr	spare lettura AI1 (analogInput 1)
 ee     Stato 0 oppure 1 di ciascuno dei rele di stazione Rele1(pin7) e Rele2(pin8) 

              !!428.43;62;28;542;239;01
			        !!^  ^  ; ^; ^; ^ ; ^ ; |---Rele1 LOW  Rele2 HIGH
              !!|  |  ; |; |; | ; |-------Lettura A1 spare                                   
              !!|  |  ; |; |; |-----------Lettura A0 sensore fumi MQ2
              |||  |  ; |; |--------------Temperatura da DHT11
              |||  |  ; |-----------------Umidita da DHT11
              |||  |----------------------Temperatura da DS18B20 xx.yy
              |||-------------------------ID stazione slave
              ||--------------------------Carattere inizio ricezione
              |---------------------------Carattere inizio ricezione
*/

//  200401  Aggiunta connessione per evento Maker su IFTTT
//  170712  Aggiunta gestione dei 2 rele da comando remoto su bus 485
//  170809  Nuovo canale ThingSpeak per inviare lo stato degli 8 rele
//  170831  Aggiunto Slave5
//  180120  Aggiunta lettura Analog input A1
//  18yyxx	Diventa NTP client con protocollo UDP
//  180404  Gauges temperature di stazione a EMONCMS
//	180406	Colloquio con NTPserver
//  180625  Tutte le trasmissioni su bus terminano con \n
//	180706	Aggiunta stazione outlet + termoconvettore cucina
//	1808xx	Informazioni da OpenWeatherMap copiate nella tabella del database

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
  c)  Inserire nella directory delle libraries (C:\MyArduinoWorks\librariess)
        la cartella <espsoftwareserial-master> che attiene alla libreria SoftwareSerial 
		per ESP8266 e non per AVR. Eventualmente riavviare lo ide.
 */
// http://www.maffucci.it/2017/05/13/iot-con-wemos-d1-mini-usando-arduino-ide-e-blynk/comment-page-1/
// http://docs.blynk.cc/#blynk-server

//-------------------Defines
//#define  WithBlynk
//#define WithMQTT
#define SERIAL_DEBUG

#define HowManySlaves 6     //1-letto 2-pranzo  3-cucina  4-salotto 5-caldaia  6-heatPLUG
#define STidle        0     //Macchina a stati  idle
#define STreceiving   1     //Macchina a stati  ricezione in corso stringa da slave
#define STendresponse 2     //Macchina a stati  fine ricezione da slave
#define STsent_ts     3     //Macchina a stati  trasmissione dati verso ThingSpeak
#define STsent_ts_rele 4    //Macchina a stati  trasmissione stato rele verso ThingSpeak
#define CycleTime     540000//ms
#define MaxTalkTime   6000  //Massima attesa risposta da slave
#define TelegramClient  1   //| Da chi proviene la richiesta delle misure ATTUALI
#define BlynkClient     2   //|  
#define ArrayLen      10    //Buffer per ciascuno array di char provenienti dagli slaves
#define Speed         1200  //bps di comunicazione
#define RS485_WeMos   0     //Canale TS 119177 con i campi Celsius ed Rh delle 4 stazioni
#define RS485_Wemos1  1     //Canale TS 314423 con il campo dello stato rele 8bit binary    
//for NTP client
#define seventyYears   2208988800UL			//Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
#define Sec4day   86400L  			        //Secondi ogni giorno 
#define Sec4hour  3600								  //Secondi ogni ora
//To fire event to IFTTT (library DataToMaker is inside this folder)
//   https://maker.ifttt.com/trigger/CasaZie/with/key/iRsU-c2LcNtn4HX6ZSD_BAiJpy6fYs7OSphOb7b2sc1
#define myIFTTTkey "iRsU-c2LcNtn4HX6ZSD_BAiJpy6fYs7OSphOb7b2sc1"    //account: atiliomelucci@gmail.com
#define myIFTTTevent "CasaZie"


#ifdef  WithBlynk  
//For Blynk pourpose
//#define BLYNK_DEBUG // Optional, this enables lots of prints
  #define BLYNK_PRINT   Serial    // Comment this out to disable prints and save space


//-------------------Included Libraries
  #include <BlynkSimpleEsp8266.h>
  #include <SimpleTimer.h>      // occorre la libreria ---SimpleTimer-master.zip--- 
#endif
#ifdef WithMQTT
  #include <PubSubClient.h>
#endif 
//https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/readme.md
//https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/station-class.md#setautoreconnect
#include <ESP8266WiFi.h>      //<--refer to arduinoxyz/libraries/Firmata/examples/StandartFirmataWifi/
#include <SoftwareSerial.h>   //from espsoftwareserial-master.zip (https://github.com/plerup/espsoftwareserial)

//https://www.geekstips.com/arduino-time-sync-ntp-server-esp8266-udp/
#include <WiFiUdp.h>			  //for UDP NTPclient
#include <ArduinoJson.h> 	  //for OpenWeatherMap json parser string
#include "DataToMaker.h"    //in questa stessa cartella per evento su IFTTT

//-------------------collegamenti dei devices
#define Rx485 D2
#define Tx485 D1
#define ReDe  D4    //Led blu sulla scheda
#define DiagLed D3
SoftwareSerial Serial485(Rx485,Tx485,false,256);


//-------------------global variables declarations
// Your WiFi credentials.Set password to "" for open networks.
const char* ssid = "znirpUSN";
const char* password = "ta269179";

//Stringhe per ThingSpeack
String apiKey = "4YU81B4O8AX7QLWK";         //API writeKey for RS485_WeMos ID 119177
String apiKey1 = "C9538G3H8CNGHCBN";        //API writeKey for RS485_Wemos1 ID 314423
const char* thingspeak = "api.thingspeak.com";

//E M O N C M S (we haven't DNS)
const char* emonIP =  "80.243.190.58";        // emoncms.org
//Read & Write:
  String apikey2 = "af32a215cbf0dc398627f3b3189262c9";
//ReadOnly:
//apikey= 896e2f2a1a1e1d926084bbbe66ba100a
//da browser web i dati per emoncms si aggiornano digitando la riga di URL
// http://80.243.190.58/api/post?apikey=af32a215cbf0dc398627f3b3189262c9&json={T:21.01,LC:22,Rh:23,Pr:244,w:25}
// oppure  
// http://emoncms.org/input/post?json={T:21.01,LC:22,Rh:23,Pr:244,w:25}&apikey=af32a215cbf0dc398627f3b3189262c9
// oppure
// http://80.243.190.58/input/post?json={T:21.01,LC:22,Rh:23,Pr:244,w:25}&apikey=af32a215cbf0dc398627f3b3189262c9
int node = 0; //if 0 for EMONCMS, not used

//Stringhe per -----OpenWheaterMap
const char* OpnWthMp =  "api.openweathermap.org";        // openweathermap.org
String apiKey3 = "842941aeb62129ebb1f279ad43af4b11";

//Broker cloud per MQTT
#ifdef  withMQTT
  const char* mqtt_server = "broker.mqtt-dashboard.com";
#endif

// You should get Auth Token in the Blynk App.Go to the Project Settings (simbolo del dado).
// Auth Token for Master485weMos project and device Master485weMos:
#ifdef  WithBlynk
const char* BlynkAuth = "badbfc240fba480384ab40c83dbcd415";
const char* BlynkServer = "blynk-cloud.com";
#endif

//For NTP communications
unsigned int localPort = 2390;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; 		// NTP time stamp is in the first 48 bytes of the message
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; 				// time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
byte packetBuffer[NTP_PACKET_SIZE]; 	//buffer to hold incoming and outgoing packets

unsigned long previous, just1sec, JustNow;
char ByteIN;
String Buffer;
//https://forum.arduino.cc/index.php?topic=363595.0
//IMPORTANTE:  In un array l'ultima componente di ogni dimensione è il carattere NULL
char Tds[HowManySlaves][ArrayLen];          //Celsius from DS18B20
char Rdh[HowManySlaves][ArrayLen];          //umidity from DHT11
char Tdh[HowManySlaves][ArrayLen];          //Celsius from DHT11
char AI0[HowManySlaves][ArrayLen];          //Smoke Level from MQ2 sensor AnalogInput0
char AI1[HowManySlaves][ArrayLen];          //spare AnalogInput1
char ReleStatus[HowManySlaves][ArrayLen/2]; //00 01 10 11 stato coppia di rele di stazione da inviare SOLO al client Blynk
unsigned int StatoRele;                		// (c)    16bit per Stato rele inviati a TS canale -RS485_Wemos1- ID 314423

uint8_t SlaveID;
int8_t  Status;
int8_t  FirstSemicolonPos, SecondSemicolonPos, ThirdSemicolonPos, FourthSemicolonPos, FifthSemicolonPos, SixthSemicolonPos;
int8_t  StringOk;   // 0 -> Stringa ricevuta tutta scorretta
                    // 1 -> Stringa ricevuta tipo !!xaa.bb;cc (solo prima parte corretta)
                    // 2 -> Stringa ricevuta tipo !!xaa.bb;cc;dd;qqq.qq (corretta solo fino alla lettura di AI0)
                    // 3 -> Stringa ricevuta tipo !!xaa.bb;cc;dd;qqq.qq;rrr (corretta solo fino alla lettura di AI1)
                    // 4 -> Stringa ricevuta tipo !!xaa.bb;cc;dd;qqq.qq;rrr;rrr (corretta fino alla seconda lettura di AI1)
                    // 5 -> Stringa ricevuta tutta corretta !!xaa.bb;cc;dd;qqq.qq;rrr;rrr;ee>
uint8_t BlynkMenuItem;
int8_t	hour, minute; //for NTP server
              

//-------------------Classes
// Create an instance of the server, specify the port to listen on as an argument
WiFiServer server(80);
WiFiClient client;
WiFiUDP udp;			// A UDP instance to let us send and receive packets over UDP
DataToMaker IFTTTevent(myIFTTTkey, myIFTTTevent);    //for IFTTT pourpose

#ifdef  WithBlynk
// Attach virtual serial terminal to Virtual Pin V1
  WidgetTerminal terminal(V1);
// V3 LED Widget represents the physical rele1 state
  WidgetLED LedRele1(V3);
// V4 LED Widget represents the physical rele2 state
  WidgetLED LedRele2(V4);
  SimpleTimer timer;
#endif

//-------------------------StartUP routine
void setup(){
  Serial.begin(Speed);          // Open native serial communications:
  Serial485.begin(Speed);       // comunicazione con SN75176
  pinMode(DiagLed,OUTPUT);      // Led rosso diagnostica 
  pinMode(ReDe, OUTPUT);        // SN75176  High TX   Low RX
  delay(10);
  digitalWrite(ReDe,LOW);       // In ascolto sul bus
  ESPdiag();
// Connect to WiFi network
  WiFiConnection();
  Serial.println("Ask");
/* for wdog manipulation
https://techtutorialsx.com/2017/01/21/esp8266-watchdog-functions/
  ESP.wdtDisable();
  ESP.wdtEnable();
  ESP.wdtFeed();
*/  
  ESP.wdtEnable(6000); 
  SlaveID = 1;
  
//===Blynk settings  
  #ifdef  WithBlynk   
    Blynk.config(BlynkAuth,BlynkServer, 8442);
    Blynk.connect(20000);       //TimeOut connection to Blynk Server
//===or   
  //Blynk.connectWiFi(ssid,password); //If the server is the local router
//===or
  //Blynk.begin(BlynkAuth, ssid, password);
//===or You can also specify server:
  //Blynk.begin(BlynkAuth, ssid, password, BlynkServer, 8442);
  //Blynk.begin(BlynkAuth, ssid, password, IPAddress(192,168,1,22), 8442);
/* Set of Blynk menu items from arduino side:
   La App di Android manda a WeMos l'indice della voce di menu selezionata, associandolo al  
   valore del pin virtuale V0. Il valore minimo dell'indice è 1.
*/
  //Blynk.setProperty(V0, "labels", "letto", "pranzo", "cucina", "salotto");
  #endif
//init UDP
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());  
}


//-------------------------Cyclic Jobs
void loop(){
  CheckWiFiConnection();
  // GetOpenWeatherMap();
  AskMeasureFromSlave(SlaveID);
  StringOk = 0;
// In CycleTime viene --effettuata la chiamata verso uno slave,
//                    --ricevuta la stringa di risposta (se lo slave è presente),
//                    --inviata la risposta a ThingSpeak ed EmonCMS

///----Loop del programma
  while((millis()- previous)< CycleTime){ 
    ESP.wdtFeed();
    if(!WiFi.isConnected()){          //Returns true if Station is connected to an access point or false if not.
      Serial.println("connection lost");
      ESP.restart();     
      return;
    }
    while(Serial485.available()&& (Status < STendresponse)){
      if (ComposeMeasureFromSlave(SlaveID) > 0 ){
        Serial.print("main...:");
        Serial.println(SlaveID);
        ScriviThingSpeak(RS485_WeMos);        	//<--- Write on TS channel 119177
		    if(SlaveID == 3) ScriviEmonCMS_MQ2(); 	//invio a EmonCMS valore di MQ2 sensore gas in cucina
        Status = STsent_ts;
        Serial.println("Misura OK");         
      }
    }
    if((Status == STidle) && ((millis() - previous) > MaxTalkTime)) (Status = STsent_ts);
    if(Status >= STsent_ts){
      if((SlaveID == HowManySlaves) && (Status == STsent_ts)){
      //composizione della variabile StatoRele  (c)
        StatoRele = 0;
        //Rele dello Slave1
        if(ReleStatus[0][1] == '1') StatoRele = 2048;      
        if(ReleStatus[0][0] == '1') StatoRele = StatoRele + 1024;
        //Rele dello Slave2
        if(ReleStatus[1][1] == '1') StatoRele = StatoRele + 512;
        if(ReleStatus[1][0] == '1') StatoRele = StatoRele + 256;
        //Rele dello Slave3
        if(ReleStatus[2][1] == '1') StatoRele = StatoRele + 128;
        if(ReleStatus[2][0] == '1') StatoRele = StatoRele + 64;          
        //Rele dello Slave4
        if(ReleStatus[3][1] == '1') StatoRele = StatoRele + 32;
        if(ReleStatus[3][0] == '1') StatoRele = StatoRele + 16;           
        //Rele dello Slave5
        if(ReleStatus[4][1] == '1') StatoRele = StatoRele + 8;
        if(ReleStatus[4][0] == '1') StatoRele = StatoRele + 4;  
        //Rele dello Slave6
        if(ReleStatus[5][1] == '1') StatoRele = StatoRele + 2;
        if(ReleStatus[5][0] == '1') StatoRele = StatoRele + 1;  
        ScriviThingSpeak(RS485_Wemos1);     //<--- Write on TS channel 314423  
        ScriviEmonCMS_celsius();            //<--- Aggiorna i gauges delle temperature di stazione su EMONCMS
//colloquio con NTP server
		    WiFi.hostByName(ntpServerName, timeServerIP);		//get a random server from the pool
		    sendNTPpacket(timeServerIP); 						        //send an NTP packet to a time server
		    delay(1000);										                //wait to see if a reply is available
        if (!udp.parsePacket()) Serial.println("no packet yet");
        else NTPpacketReceived();
//END colloquio con MNTp server

        Status = STsent_ts_rele;
      }
      NowAsServer();
       
      #ifdef  WithBlynk 
      Blynk.run();
      #endif
    }
  }
///---

// Scaduto CycleTime si torna nello stato IDLE e si punta alla stazione successiva
  previous = millis();
  Serial.println();
//<--Sweep to next slave station  
  SlaveID++ ;          
  if(SlaveID > HowManySlaves) SlaveID = 1;
}


//$$$$$$$$$$$$$$$$$$$  S U B R O U T I N E S  $$$$$$$$$$$$$$$$$$$$$$

////////////////////  Ascolta richieste su http://dario95.ddns.net:8083
void NowAsServer(){
//Toggle led for diagnostic
  if ((millis() - just1sec) > 500){
    digitalWrite(DiagLed,!digitalRead(DiagLed));
    just1sec = millis();
  }
// Check if a client has connected
  client = server.available();
  if (!client){
    //ESP.restart();      //<---Messo qui il modulo si resetta continuamente
    return;
  }
// Wait until the client sends some data
  while(!client.available()){
    ESP.wdtFeed();
  }
// Read the first line of the request
  client.setTimeout(MaxTalkTime);
  String req = client.readStringUntil('\r');
  client.flush();
// Verifica quale richiesta e' stata fatta

//----To Slave1 
//  http://dario95.ddns.net:8083/mis/1 (pagina web) 
  if(req.indexOf("/mis/1") != -1){              //<-- La stringa nello http request finisce con mis/1
    SendHtmlPage(1);                            //invia i dati della stazione 1
    return;
  }
//  http://dario95.ddns.net:8083/letto (telegram)
  if(req.indexOf("/letto") != -1){              //<-- La stringa nello http request finisce con /letto
    //SendTelegram(1);      //<-- richiesta misure della ultima scansione
    NowForClient(1,TelegramClient);             //<-- richiesta misure ORA alla stazione
  }
//  http://dario95.ddns.net:8083/rele/1/0
  if(req.indexOf("/rele/1/0") != -1){           //<-- La stringa nello http request finisce con /rele/1/0
    Send485Rele(1,0);                           //manda i comandi di rele1 OFF   e   rele2 OFF  alla stazione1
    return;
  }
//  http://dario95.ddns.net:8083/rele/1/1
  if(req.indexOf("/rele/1/1") != -1){           //<-- La stringa nello http request finisce con /rele/1/1
    Send485Rele(1,1);                          //manda i comandi di rele1 ON   e   rele2 OFF   alla stazione1
    return;
  }
//  http://dario95.ddns.net:8083/rele/1/2
  if(req.indexOf("/rele/1/2") != -1){           //<-- La stringa nello http request finisce con /rele/1/2
    Send485Rele(1,2);                          //manda i comandi di rele1 OFF   e  rele2 ON    alla stazione1
    return;
  }
//  http://dario95.ddns.net:8083/rele/1/3
  if(req.indexOf("/rele/1/3") != -1){           //<-- La stringa nello http request finisce con /rele/1/3
    Send485Rele(1,3);                          //manda i comandi di rele1 ON    e  rele2 ON    alla stazione1
    return;
  }
  
//----To Slave2 
//  http://dario95.ddns.net:8083/mis/2 (pagina web) 
  if(req.indexOf("/mis/2") != -1){              //<-- La stringa nello http request finisce con mis/2
    SendHtmlPage(2);                            //invia i dati della stazione 2
    return;
  }
//  http://dario95.ddns.net:8083/pranzo (telegram)
  if(req.indexOf("/pranzo") != -1){             //<-- La stringa nello http request finisce con /pranzo
    //SendTelegram(2);      //<-- richiesta misure della ultima scansione
    NowForClient(2,TelegramClient);              //<-- richiesta misure ORA alla stazione    
    return;
  }
//  http://dario95.ddns.net:8083/rele/2/0
  if(req.indexOf("/rele/2/0") != -1){           //<-- La stringa nello http request finisce con /rele/2/0
    Send485Rele(2,0);                           //manda i comandi di rele1 OFF   e   rele2 OFF  alla stazione2
    return;
  }
//  http://dario95.ddns.net:8083/rele/2/1
  if(req.indexOf("/rele/2/1") != -1){           //<-- La stringa nello http request finisce con /rele/2/1
    Send485Rele(2,1);                          //manda i comandi di rele1 ON   e   rele2 OFF   alla stazione2
    return;
  }
//  http://dario95.ddns.net:8083/rele/2/2
  if(req.indexOf("/rele/2/2") != -1){           //<-- La stringa nello http request finisce con /rele/2/2
    Send485Rele(2,2);                          //manda i comandi di rele1 OFF   e  rele2 ON    alla stazione2
    return;
  }
//  http://dario95.ddns.net:8083/rele/2/3
  if(req.indexOf("/rele/2/3") != -1){           //<-- La stringa nello http request finisce con /rele/2/3
    Send485Rele(2,3);                          //manda i comandi di rele1 ON    e  rele2 ON    alla stazione2
    return;
  }
  
//----To Slave3 
//  http://dario95.ddns.net:8083/mis/3 (pagina web) 
  if(req.indexOf("/mis/3") != -1){              //<-- La stringa nello http request finisce con mis/3
    SendHtmlPage(3);                            //invia i dati della stazione 3
    return;
  }
//  http://dario95.ddns.net:8083/cucina (telegram)
  if(req.indexOf("/cucina") != -1){             //<-- La stringa nello http request finisce con /cucina
    //SendTelegram(3);      //<-- richiesta misure della ultima scansione
    NowForClient(3,TelegramClient);             //<-- richiesta misure ORA alla stazione    
    return;
  }
//  http://dario95.ddns.net:8083/rele/3/0
  if(req.indexOf("/rele/3/0") != -1){           //<-- La stringa nello http request finisce con /rele/3/0
    Send485Rele(3,0);                           //manda i comandi di rele1 OFF  e   rele2 OFF  alla stazione3
    return;
  }
//  http://dario95.ddns.net:8083/rele/3/1
  if(req.indexOf("/rele/3/1") != -1){           //<-- La stringa nello http request finisce con /rele/3/1
    Send485Rele(3,1);                          //manda i comandi di rele1 ON    e   rele2 OFF   alla stazione3
    return;
  }
//  http://dario95.ddns.net:8083/rele/3/2
  if(req.indexOf("/rele/3/2") != -1){           //<-- La stringa nello http request finisce con /rele/3/2
    Send485Rele(3,2);                          //manda i comandi di rele1 OFF   e  rele2 ON    alla stazione3
    return;
  }
//  http://dario95.ddns.net:8083/rele/3/3
  if(req.indexOf("/rele/3/3") != -1){           //<-- La stringa nello http request finisce con /rele/3/3
    Send485Rele(3,3);                          //manda i comandi di rele1 ON    e  rele2 ON    alla stazione3
    return;
  }
  
//----To Slave4
//  http://dario95.ddns.net:8083/mis/4 (pagina web)
  if(req.indexOf("/mis/4") != -1){              //<-- La stringa nello http request finisce con mis/4
    SendHtmlPage(4);                            //invia i dati della stazione 4
    return;
  }
//  http://dario95.ddns.net:8083/salotto (telegram)
  if(req.indexOf("/salotto") != -1){            //<-- La stringa nello http request finisce con /salotto
    //SendTelegram(4);      //<-- richiesta misure della ultima scansione
    NowForClient(4,TelegramClient);             //<-- richiesta misure ORA alla stazione    
    return;
  }
//  http://dario95.ddns.net:8083/rele/4/0
  if(req.indexOf("/rele/4/0") != -1){           //<-- La stringa nello http request finisce con /rele/4/0
    Send485Rele(4,0);                           //manda i comandi di rele1 OFF  e   rele2 OFF  alla stazione4
    return;
  }
//  http://dario95.ddns.net:8083/rele/4/1
  if(req.indexOf("/rele/4/1") != -1){           //<-- La stringa nello http request finisce con /rele/4/1
    Send485Rele(4,1);                          //manda i comandi di rele1 ON    e   rele2 OFF   alla stazione4
    return;
  }
//  http://dario95.ddns.net:8083/rele/4/2
  if(req.indexOf("/rele/4/2") != -1){           //<-- La stringa nello http request finisce con /rele/4/2
    Send485Rele(4,2);                          //manda i comandi di rele1 OFF   e  rele2 ON    alla stazione4
    return;
  }
//  http://dario95.ddns.net:8083/rele/4/3
  if(req.indexOf("/rele/4/3") != -1){           //<-- La stringa nello http request finisce con /rele/4/3
    Send485Rele(4,3);                          //manda i comandi di rele1 ON    e  rele2 ON    alla stazione4
    return;
  }
  
//----To Slave5
//  http://dario95.ddns.net:8083/mis/5 (pagina web)
  if(req.indexOf("/mis/5") != -1){              //<-- La stringa nello http request finisce con mis/5
    SendHtmlPage(5);                            //invia i dati della stazione 5
    return;
  }
//  http://dario95.ddns.net:8083/caldaia (telegram)
  if(req.indexOf("/caldaia") != -1){            //<-- La stringa nello http request finisce con /caldaia
    //SendTelegram(5);      //<-- richiesta misure della ultima scansione
    NowForClient(5,TelegramClient);             //<-- richiesta misure ORA alla stazione    
    return;
  }
//  http://dario95.ddns.net:8083/rele/5/0
  if(req.indexOf("/rele/5/0") != -1){           //<-- La stringa nello http request finisce con /rele/5/0
    Send485Rele(5,0);                           //manda i comandi di rele1 OFF  e   rele2 OFF  alla stazione5
    return;
  }
//  http://dario95.ddns.net:8083/rele/5/1
  if(req.indexOf("/rele/5/1") != -1){           //<-- La stringa nello http request finisce con /rele/5/1
    Send485Rele(5,1);                          //manda i comandi di rele1 ON    e   rele2 OFF   alla stazione5
    return;
  }
//  http://dario95.ddns.net:8083/rele/5/2
  if(req.indexOf("/rele/5/2") != -1){           //<-- La stringa nello http request finisce con /rele/5/2
    Send485Rele(5,2);                          //manda i comandi di rele1 OFF   e  rele2 ON    alla stazione5
    return;
  }
//  http://dario95.ddns.net:8083/rele/5/3
  if(req.indexOf("/rele/5/3") != -1){           //<-- La stringa nello http request finisce con /rele/5/3
    Send485Rele(5,3);                          //manda i comandi di rele1 ON    e  rele2 ON    alla stazione5
    return;
  }

//----To Slave6
//  http://dario95.ddns.net:8083/mis/6 (pagina web)
  if(req.indexOf("/mis/6") != -1){              //<-- La stringa nello http request finisce con mis/6
    SendHtmlPage(6);                            //invia i dati della stazione 6
    return;
  }
//  http://dario95.ddns.net:8083/heatplug (telegram)
  if(req.indexOf("/heatplug") != -1){            //<-- La stringa nello http request finisce con /heatplug
    //SendTelegram(6);      //<-- richiesta misure della ultima scansione
    NowForClient(6,TelegramClient);             //<-- richiesta misure ORA alla stazione    
    return;
  }
//  http://dario95.ddns.net:8083/rele/6/0
  if(req.indexOf("/rele/6/0") != -1){           //<-- La stringa nello http request finisce con /rele/6/0
    Send485Rele(6,0);                           //manda i comandi di rele1 OFF  e   rele2 OFF  alla stazione6
    return;
  }
//  http://dario95.ddns.net:8083/rele/6/1
  if(req.indexOf("/rele/6/1") != -1){           //<-- La stringa nello http request finisce con /rele/6/1
    Send485Rele(6,1);                          //manda i comandi di rele1 ON    e   rele2 OFF   alla stazione6
    return;
  }
//  http://dario95.ddns.net:8083/rele/6/2
  if(req.indexOf("/rele/6/2") != -1){           //<-- La stringa nello http request finisce con /rele/6/2
    Send485Rele(6,2);                          //manda i comandi di rele1 OFF   e  rele2 ON    alla stazione6
    return;
  }
//  http://dario95.ddns.net:8083/rele/6/3
  if(req.indexOf("/rele/6/3") != -1){           //<-- La stringa nello http request finisce con /rele/6/3
    Send485Rele(6,3);                          //manda i comandi di rele1 ON    e  rele2 ON    alla stazione6
    return;
  }

//----To ALL slaves
//  http://dario95.ddns.net:8083/rele/0/0
  if(req.indexOf("/rele/0/0") != -1){           //<-- La stringa nello http request finisce con /rele/0/0
		for (uint8_t i=1; i<HowManySlaves +1; i++){
			Send485Rele(i,0);													//manda i comandi di rele1 OFF  e   rele2 OFF  a TUTTE le stazioni
			delay(7000);
		}	
    return;
  }


//----------
  else if(req.indexOf("/reboot") != -1){
    Serial.println("reboot...");
    ESP.restart();
    return; 
  }
//Richiesta non riconosciuta  
  else {
    client.stop();
  }    
}  


////////////////////  Invio comandi ai rele delle stazioni su bus 485 
void Send485Rele(int8_t IDstat, int8_t WhichRele){
//compongo la stringa di comando
//         Master485WeMos              Slave485ProMini
//           Cxfg             --->
//
// x      byte che identifica la stazione a cui il master invia il comando
// f      0 oppure 1 comando Rele1 
// g      0 oppure 1 comando Rele2
//Flush the input buffer
  while( Serial485.available() > 0) Serial485.read(); 
  digitalWrite(ReDe,HIGH);        // In possesso del bus 485
  delay(500);
  Serial.print('C');
  Serial485.print('C');
  delay(200);
  Serial.print(IDstat); 
  Serial485.print(IDstat);  
  delay(500);
  switch (WhichRele){           //Rele2   Rele1
    case 0:{                    // OFF    OFF
      Serial.println("00");
      Serial485.println("00"); 
      IFTTTevent.setValue(1,String(IDstat)+"--00");
      if(IFTTTevent.connect()) IFTTTevent.post();  
      break;
    }
    case 1:{                    // OFF    ON
      Serial.println("01");        
      Serial485.println("01");
      IFTTTevent.setValue(1,String(IDstat)+"--01");
      if(IFTTTevent.connect()) IFTTTevent.post();    
      break;
    }
    case 2:{                    // ON     OFF
      Serial.println("10");
      Serial485.println("10"); 
      IFTTTevent.setValue(1,String(IDstat)+"--10");
      if(IFTTTevent.connect()) IFTTTevent.post();   
      break;
    }
    case 3:{                    // ON     ON
      Serial.println("11");
      Serial485.println("11");
      IFTTTevent.setValue(1,String(IDstat)+"--11");
      if(IFTTTevent.connect()) IFTTTevent.post();   
      break; 
    }
  }
  Serial.println("");    
  digitalWrite(ReDe,LOW);         // In ascolto sul bus   
  delay(2000);
  HtmlHeader(IDstat); 
  client.println("</html>"); 
  client.stop(); 
}


////////////////////  Richiesta misure da stazione slave  ?whichID
void AskMeasureFromSlave(uint8_t whichID){
  Status = STidle;
  Buffer = "";
//Flush the input buffer
  while( Serial485.available() > 0) Serial485.read(); 
  digitalWrite(ReDe,HIGH);        // In possesso del bus 485
  delay(500);
  for (uint8_t i=0; i<2; i++){
    Serial.print('?');
    Serial485.print('?');
    delay(200);
    Serial.print(whichID); 
    Serial485.print(whichID); 
  }
  Serial.println("");    
  digitalWrite(ReDe,LOW);         // In ascolto sul bus
//Attendo una risposta dalla stazione remota tipo !!123.38;29;25
/*                                                !!^  ^  ; ^; ^
                                                  ^^|  |  ; |; |---Temperatura da DHT11
                                                  ^^|  |  ; |------Umidita da DHT11
                                                  ^^|  |-----------Temperatura da DS18B20 xx.yy
                                                  ^^|--------------ID stazione slave
*/
}
////////////////////  Risposta attesa: v nota (A) restituisce 3 se stringa tutta corretta, 2,1 o 0 stringa errata
//                    parzialmente o del tutto. 
//  !!xaa.bb;cc;dd;qqq.qq;rrr;rrr;ee>
int8_t ComposeMeasureFromSlave(uint8_t SlaveAddr){
  //Serial.print("nella routine...:");
  //Serial.println(SlaveAddr);
  ByteIN = Serial485.read(); 
  if(ByteIN == '!') Status = STreceiving; 
  if(ByteIN == '>' && (Status == STreceiving)) Status = STendresponse;
  if(Status == STreceiving) Buffer += ByteIN;
  if(Status == STendresponse){
//indexOf() returns The index of val within the String, or -1 if not found.
    Serial.println(Buffer);
    FirstSemicolonPos = Buffer.indexOf(';');   //see (A) 
    if(FirstSemicolonPos > 5){
      SecondSemicolonPos = Buffer.indexOf(';',FirstSemicolonPos+1); 
      Buffer.substring(3,FirstSemicolonPos).toCharArray(Tds[SlaveAddr-1],ArrayLen); 
    }
    if(SecondSemicolonPos > 7){
      StringOk = 1;   // at least the first 2 measures are correct
      ThirdSemicolonPos = Buffer.indexOf(';',SecondSemicolonPos+1); 
      Buffer.substring(FirstSemicolonPos+1,SecondSemicolonPos).toCharArray(Rdh[SlaveAddr-1],ArrayLen);          
    }
    if(ThirdSemicolonPos >9){
 //<--https://www.arduino.cc/en/Reference/StringToCharArray
      StringOk = 2;   //all the measures are correct
      FourthSemicolonPos = Buffer.indexOf(';',ThirdSemicolonPos+1);
      Buffer.substring(SecondSemicolonPos+1,ThirdSemicolonPos).toCharArray(Tdh[SlaveAddr-1],ArrayLen);
      Buffer.substring(ThirdSemicolonPos+1,FourthSemicolonPos).toCharArray(AI0[SlaveAddr-1],ArrayLen);
    }
    if(FourthSemicolonPos >11){
      StringOk = 3;   //Up to AI1 read correctly
      FifthSemicolonPos = Buffer.indexOf(';',FourthSemicolonPos+1);
      Buffer.substring(FourthSemicolonPos+1,FifthSemicolonPos).toCharArray(AI1[SlaveAddr-1],ArrayLen);
    }
    //qui si salta la seconda lettura di AI1
    if(FifthSemicolonPos >16){
      StringOk = 4;   //Up to rele status read correctly
			SixthSemicolonPos = Buffer.indexOf(';',FifthSemicolonPos+1);
      Buffer.substring(SixthSemicolonPos+1).toCharArray(ReleStatus[SlaveAddr-1],ArrayLen/2);
      //ReleStatus di 3 caratteri perchè in 'C' l'ultimo carattere di una stringa è /n
    }
  }
  return StringOk;
}


////////////////////Telegram(client1) o Blynk(client2) richiede le misure ADESSO a una stazione
void NowForClient(uint8_t statID,uint8_t clientID){
  AskMeasureFromSlave(statID);
  StringOk = 0;
  JustNow = millis();  
  //Serial.print("qui");
  Serial.println(statID);
  while((millis()- JustNow)< MaxTalkTime){
    ESP.wdtFeed(); 
    while(Serial485.available()){
      if (ComposeMeasureFromSlave(statID) >0){ 
        if(clientID == TelegramClient){
          Serial.print("NowForTel...:");
          Serial.println(statID);
          SendTelegram(statID);               //invia i dati della stazione a Telegram
          return;
        }   
      }    
    }
  }
}


////////////////////  Stato connessione al router WiFi
void CheckWiFiConnection(){
  ESP.wdtFeed();
  if(!WiFi.isConnected()){  //Returns true if Station is connected to an access point or false if not.
    ESP.restart();     
    return;
  }  
}


////////////////////  https://media.readthedocs.org/pdf/arduino-esp8266/docs_to_readthedocs/arduino-esp8266.pdf
void ESPdiag(){
  Serial.println(ESP.getChipId());                //returns the ESP8266 chip ID as a 32-bit integer.
  Serial.println(ESP.getFlashChipSpeed());        //returns the flash chip frequency, in Hz.
  Serial.println(ESP.getFreeSketchSpace());       //returns the free sketch space as an unsigned 32-bit integer
}


////////////////////  Connessione al router con indirizzo fisso
void WiFiConnection(){
  /*  
   * Viene impostato il modo station (differente da AP o AP_STA) 
   * La modalità STA consente all'ESP8266 di connettersi a una rete Wi-Fi 
   * (ad esempio quella creata dal router wireless), mentre la modalità AP  
   * consente di creare una propria rete e di collegarsi 
   * ad altri dispositivi (ad esempio il telefono). 
   */  
  WiFi.mode(WIFI_STA); 
  /*  
   *  Inizializza le impostazioni di rete della libreria WiFi e fornisce lo stato corrente della rete, 
   *  nel caso in esempio ha come parametri ha il nome della rete e la password. 
   *  Restituisce come valori: 
   *   
   *  WL_CONNECTED quando connesso al network 
   *  WL_IDLE_STATUS quando non connesso al network, ma il dispositivo è alimentato 
  */  
  WiFi.begin(ssid, password); 
//Impostazioni wifi: Commentare se si usa il dhcp del router)
//WiFi.config(ip,ateway, subnet);
  WiFi.config(IPAddress(192, 168, 1, 22), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
/*  
   *  fino a quando lo non si è connessi alla WiFi 
   *  compariranno a distanza di 250 ms dei puntini che 
   *  evidenziano lo stato di avanzamento della connessione 
*/    
  while (WiFi.status() != WL_CONNECTED) {  
    delay(250);  
    Serial.print("."); 
    digitalWrite(DiagLed,!digitalRead(DiagLed));
  }  
/*  https://media.readthedocs.org/pdf/arduino-esp8266/docs_to_readthedocs/arduino-esp8266.pdf
If connection is established, and then lost for some reason, ESP will automatically reconnect to last used access
point once it is again back on-line. This will be done automatically by Wi-Fi library, without any user intervention.  */
// se connesso alla WiFi stampa sulla serial monitor  
// nome della rete e stato di connessione  
  Serial.println("");  
  Serial.print("connected: ");  
  Serial.println(ssid);  
  Serial.println("WiFi ok");  
// Avvia il server  
  server.begin();  
  Serial.println("Server avviato");  
// Stampa l'indirizzo IP  
  Serial.print("URL : ");  
  Serial.print("http://");  
  Serial.print(WiFi.localIP()); // Restituisce lo IP della scheda  
  Serial.println("/");   
} 


////////////////////  Invio a Telegram misure dello slave come erano nell'ultimo ciclo di scansione
void SendTelegram(uint8_t slave){
  client.flush();
  Serial.print("ToTelegram StatID..");
  Serial.println(slave);
//-----------------restituisce la risposta
// intestazione pagina html  
  client.println("HTTP/1.1 200 OK");  
  client.println("Content-Type: text/html");  
  client.println(""); //  non dimenticare questa linea  
  client.print("SlaveID:  ");
  client.println(slave);
//Celsius from DS18B20
  client.print("Tds  ");
  client.println(Tds[slave-1]);
//humidity from DHT11
  client.print("Rdh  ");
  client.println(Rdh[slave-1]);  
//Celsius from DHT11   
  client.print("Tdh  ");
  client.println(Tdh[slave-1]);
//Smoke sensor MQ2 or LM35 on analog pin A0
  client.print("LM35  ");
  client.println(AI0[slave-1]);
//spare AI1
  client.print("AI1  ");
  client.println(AI1[slave-1]);
//Rele2 and Rele1 Status
  client.print("21Rele   ");
  client.print(ReleStatus[slave-1][1]);
  client.println(ReleStatus[slave-1][0]);
//ROME time
	client.print("last ROMEtime ");
	client.print(hour);
	client.print(":");
	client.println(minute);
//Livello segnale RF
  long rssi = WiFi.RSSI();
  client.print("RSSI  ");
  client.println(rssi);
  // chiusura connessione  
  delay(1);  
  Serial.println("Conn KO");  
  Serial.println(""); 
  client.stop(); 
  Status = STsent_ts;
}

  
////////////////////  Invio pagina HTML con le misure dello slave come erano nell'ultimo ciclo di scansione
void SendHtmlPage(uint8_t slave){
  client.flush();
  Serial.print("MyHTML..");
  Serial.println(slave);
  HtmlHeader(slave);
  // includiamo tutto il testo in un div in cui il font è impostato a 20 px  
  // N.B. ricorda che per poter considerare le " come stringa e' necessario farle precedere da uno \  
  client.print("<div style=\"font-size: 20px;\">");  
// stampa una riga separatrice  
  client.println("<hr>");  
//Celsius from DS18B20
  client.print("Tds  ");
  client.println(Tds[slave-1]);
//humidity from DHT11
  client.print("Rdh  ");
  client.println(Rdh[slave-1]);  
//Celsius from DHT11   
  client.print("Tdh  ");
  client.println(Tdh[slave-1]);
//Smoke sensor MQ2 on analog pin A0
  client.print("AI0  ");
  client.println(AI0[slave-1]);
//spare AI1
  client.print("AI1  ");
  client.println(AI1[slave-1]);
//Last Rele Status
  client.print("21Rele  ");
  client.print(ReleStatus[slave-1][1]);
  client.print(ReleStatus[slave-1][0]);
//Close HTML page
  client.print("</div>");  
  client.println("</html>"); 
// chiusura connessione  
  delay(1);  
  Serial.println("Conn KO");  
  Serial.println(""); 
  client.stop(); 
}


////////////////////  Header HTML page
void HtmlHeader(uint8_t slav){
  client.println("HTTP/1.1 200 OK");  
  client.println("Content-Type: text/html");  
  client.println(""); //  non dimenticare questa linea  
  client.println("<!DOCTYPE HTML>");  
  client.println("<html>");  
// titolo della pagina  
  client.print("<h2>Sensori casa ZIE  SlaveID:  ");
  client.print(slav);
  client.println("</h2>");  
}


////////////////////  http request POST invio coppie <Tdsx,Rdhx> a ThingSpeak
void ScriviThingSpeak(boolean ChSwitch) {
//---  
//pubblicazione su canale RS485_WeMos  ID:119177  WriteAPIkey: 4YU81B4O8AX7QLWK
//provato invio con  https://api.thingspeak.com/update?api_key=4YU81B4O8AX7QLWK&field1=20&field2=40
//---
//pubblicazione su canale RS485_Wemos1  ID:314423 WriteAPIkey: C9538G3H8CNGHCBN
////provato invio con  https://api.thingspeak.com/update?api_key=C9538G3H8CNGHCBN&field1=226
//---
// Sample of HTTP call  https://it.mathworks.com/help/thingspeak/thinghttp-app.html
  String postStr;
  if (client.connect(thingspeak,80)){
    if(ChSwitch == RS485_WeMos){
		  float fTds = atof(Tds[SlaveID-1]);
			float fRdh = atof(Rdh[SlaveID-1]);  
			if (fTds < 0 || fTds > 45) return ;     //<--controllo range di temperatura da inviare a TS 
			if (fRdh < 20 || fRdh > 120) return ;   //<--controllo range di umidita da inviare a TS   
			delay(2000);
			postStr = apiKey;
      postStr +="&field";
      if(SlaveID <5)    postStr += SlaveID*2-1; //scrive i charts 1,3,5,7 (Tds) di letto,pranzo,cucina,salotto
      if(SlaveID == 5)  postStr += 8; //scrive il chart8 (Tds) di caldaia
      postStr += "=";
      postStr += Tds[SlaveID-1];
      if(SlaveID < 4){
        postStr +="&field";
        postStr += SlaveID*2;               //scrive i charts 2,4,6 (Rdh) di letto,pranzo,cucina
        postStr += "=";
        postStr += Rdh[SlaveID-1];
      }  
    }
    if(ChSwitch == RS485_Wemos1){  
      Serial.print("StatoRele..");
      Serial.println(StatoRele);
      postStr = apiKey1;  
      postStr +="&field1"; 
      postStr += "="; 
      postStr += StatoRele;   // bit    B A     9 8     7 6     5 4     3 2     1 0  (c)   
							                //	      | |     | |     | |     | |     |_|_____|_|______[Slave=6][1][0] 
							                //	      | |     | |     | |     | |     |_|______________[Slave=5][1][0] 
                              //        | |     | |     | |     |_|______________________[Slave=4][1][0]
                              //        | |     | |     |_|______________________________[Slave=3][1][0] 
                              //        | |     |_|______________________________________[Slave=2][1][0]
                              //        |_|______________________________________________[Slave=1][1][0]
    }
    postStr += "\r\n\r\n";
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    if(ChSwitch == RS485_WeMos) client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    if(ChSwitch == RS485_Wemos1) client.print("X-THINGSPEAKAPIKEY: "+apiKey1+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    Serial.println(postStr);
    delay(1000);
    client.stop();
  }
    else{
      Serial.println("TS ko");
    }
}


void ScriviEmonCMS_MQ2(){
// Invio record a canale EMONCMS 
// da browser web tipo    http://80.243.190.58/api/post?apikey=af32a215cbf0dc398627f3b3189262c9&json={MQ2:123}
  if (client.connect(emonIP,80)){
	  //client.print("GET /api/post?apikey=af32a215cbf0dc398627f3b3189262c9&json={MQ2:");
    client.print("GET /api/post?apikey=");
    client.print(apikey2);
    client.print("&json={MQ2:");
		client.print(AI0[3-1]);
		client.print("}\r\n");
		delay(1000);
		client.stop();		
	}
}


void ScriviEmonCMS_celsius(){
// Invio record a canale EMONCMS 
// da browser web tipo    http://80.243.190.58/api/post?apikey=af32a215cbf0dc398627f3b3189262c9&json={zc1:12.34,zc2:21.33,zc3:9.88,zc4:1.88,zc5:14.55}
//GET /api/post?apikey=af32a215cbf0dc398627f3b3189262c9&json={zc1:14.56&,zc2:14.94&,zc3:14.81&,zc4:14.25&,zc5:12.56}
  if (client.connect(emonIP,80)){
	  client.print("GET /api/post?apikey=");
    debug("GET /api/post?apikey=");
    client.print(apikey2);
    debug(apikey2);
    if(node > 0){
      client.print("&node=");
      client.print(node);
    }
		client.print("&json={zc1:");
    debug("&json={zc1:");
    client.print(Tds[0]);
    debug(Tds[0]);
    client.print(",zc2:");
    debug(",zc2:");
    client.print(Tds[1]);
    debug(Tds[1]);
    client.print(",zc3:");
    debug(",zc3:");
    client.print(Tds[2]);
    debug(Tds[2]);
    client.print(",zc4:");
    debug(",zc4:");
    client.print(Tds[3]);
    debug(Tds[3]);
    client.print(",zc5:");
    debug(",zc5:");
    client.print(Tds[4]);
    debug(Tds[4]);
		client.print("}\r\n");
    debug("}\r\n");
		delay(1000);
		client.stop();		
	}
}


////////////////// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address){
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
// all NTP fields have been given values, now you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}


//////////////////We've received a packet, read the data from it
void NTPpacketReceived(){
  udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer  
//the timestamp starts at byte 40 of the received packet and is four bytes, 
// or two words, long. First, esxtract the two words:
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
// combine the four bytes (two words) into a long integer this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  Serial.print("Sec since 1-1-1900 = " );
  Serial.println(secsSince1900);
//now convert NTP time into everyday time:
  Serial.print("Unix time = ");
// subtract seventy years:
  unsigned long epoch = secsSince1900 - seventyYears;
// print Unix time:
  Serial.println(epoch);
// print the hour and minute :
  Serial.print("ROME Time  ");                // UTC is the time at Greenwich Meridian (GMT)
	hour = (epoch % Sec4day)/Sec4hour + 1;			// + 1 for ROME time 
  Serial.print(hour);   										 	// print the hour (86400 equals secs per day)
  Serial.print(':');
	minute = (epoch % Sec4hour) / 60;  
  if (minute < 10){
// In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print((epoch % Sec4hour) / 60); 			// print the minute (3600 equals secs per minute)
}


//////////////////Read weather data from openweathermap.org   see JsonWeather.png   JsonForecast.png
void GetOpenWeatherMap(){
	if (client.connect(OpnWthMp,80)){
	//references:  	https://gist.github.com/ericoporto/fb9c975a290a822edca9
	//							https://www.hackster.io/officine/getting-weather-data-655720
	//http://api.openweathermap.org/data/2.5/weather?q=Crispiano,IT&APPID=842941aeb62129ebb1f279ad43af4b11
		client.print("GET /data/2.5/weather?q=Crispiano,IT");
		client.println("&APPID="+apiKey3);
		client.print("Host: api.openweathermap.org\n");
    client.print("Connection: close\n\n");		
		delay(1000);
		String line = "";
    previous = millis();
		while(client.connected() && (millis() - previous) < MaxTalkTime){
			line = client.readStringUntil('\n'); 
			Serial.println(line);
			//create a json buffer where to store the json data 
			StaticJsonBuffer<5000> jsonBuffer; 
			JsonObject& root = jsonBuffer.parseObject(line); 
			if (!root.success()){ 
				Serial.println("parseKO"); 
				return; 
			}
      //get the data from the json tree
      String mbar = root["main"]["pressure"]; 
      String clouds = root["weather"]["description"];
      String windsp = root["wind"]["speed"];
      String winddr = root["wind"]["deg"];       
		} 
	}
	client.stop();
}


  #ifdef  WithBlynk
//=========== BLYNK routines ============================
////////////////////  Variabile V0 proveniente da widget MENU di Blynk
//                    Variabile V2 associata a widget BUTTON di Blynk
BLYNK_WRITE(V0) {
  switch (param.asInt()){   //param.asInt() in case you want to send a value to V0 from the app.
    case 1: // Item 1
      Blynk.setProperty(V2, "onLabel","ONletto");
      Blynk.setProperty(V2, "offLabel","OFFletto");
      NowForClient(1,BlynkClient);
      delay(500);
      SendBlynkTerm(1);
      break;
    case 2: // Item 2
      Blynk.setProperty(V2, "onLabel","ONpranzo");
      Blynk.setProperty(V2, "offLabel","OFFpranzo");
      NowForClient(2,BlynkClient);
      delay(500);
      SendBlynkTerm(2);
      break;
    case 3: // Item 3
      Blynk.setProperty(V2, "onLabel","ONfari");
      Blynk.setProperty(V2, "offLabel","OFFfari");
      NowForClient(3,BlynkClient);
      delay(500);
      SendBlynkTerm(3);
      break;
    case 4: // Item 4
      Blynk.setProperty(V2, "onLabel","ONsalotto");
      Blynk.setProperty(V2, "offLabel","OFFsalotto");
      NowForClient(4,BlynkClient);
      delay(500);
      SendBlynkTerm(4);
      break;    
    default:
      Serial.println("Unknown item selected");
  }
}


////////////////////  Invio variabile V1 al widget TERMINAL di Blynk le misure dello slave
void SendBlynkTerm(uint8_t BlynkSlave){
  String content = "";  //null string constant ( an empty string )
  BlynkMenuItem = BlynkSlave;
  content = "SlaveID: ";
  content += BlynkSlave;
  content += "   ";
  Blynk.virtualWrite (V1, content);
//Celsius from DS18B20
  content = "Tds ";
  content += Tds[BlynkSlave-1];
  content += "   ";
  Blynk.virtualWrite (V1, content);
//humidity from DHT11
  content = "Rdh ";
  content += Rdh[BlynkSlave-1];
  content +="\r\n";
  Blynk.virtualWrite (V1, content);
// Invio variabile V3 e V4 ai widgets LED di Blynk per stato dei rele1(pin7) e rele2(pin8)
  LedRele1.off();
  LedRele2.off();
  if(ReleStatus[BlynkSlave-1][0] == '1') LedRele1.on();  //ReleStatus a 2 char substring from slave
  if(ReleStatus[BlynkSlave-1][1] == '1') LedRele2.on();
// chiusura connessione  
  delay(1);  
  Serial.println("KO Conn"); 
  client.stop(); 
  Status = STsent_ts;
}
//=========== END of BLYNK routines =======================
  #endif


void debug(String message){
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}
void debugc(char message){
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}
void debugi(int message){
#ifdef SERIAL_DEBUG
  Serial.print(message);
#endif
}
void debugln(String message){
#ifdef SERIAL_DEBUG
  Serial.println(message);
#endif
}
void debugln(){
#ifdef SERIAL_DEBUG
  Serial.println();
#endif
}






