/*
Programet representerar en kontrollenhet för SmartShade.

Kontrollenheten är tänkt som en central punkt för ett SmartShade-system, placerat på valfri plats
i hemmet där anslutning till nätverk och sträm (USB eller elnät) finns. Kontrollenheten vidarebefordrar
all information mellan server och SmartShade-rullgardin.

Programmet är gjort för projektarbetet för årskurs 1 på Malmö Högskola 2014.
*/



// ********** Import av bibliotek **********
#include <SPI.h>               // Import av bibliotek för serie-kommunikation.
#include <Ethernet.h>          // Import av bibliotek för Ethernet-kommunikation.
#include <SoftwareSerial.h>    // Import av bibliotek för Bluetooth-kommunikation.



// -------------------------------------------------------------------------------------------------------------------------------

// ********** Tilldelning av funktion till kopplingspinnar **********
const int btInPin = 7;                                              // Pinne för inkommande data från BT-modul.
const int btOutPin = 8;                                             // Pinne för utgående data från BT-modul.

// ********** Variabler för nätverks- och serveranslutning **********
IPAddress arduinoIp ( 10, 228, 0, 4 );                              // Ethernet-shieldens IP-adress (MAH-tilldelning).
byte arduinoMac[] = { 0x90, 0xA2, 0xDA, 0x0C, 0x00, 0x94 };         // Ethernet-shieldens MAC-adress.
IPAddress serverIp ( 10, 228, 0, 204 );                             // Serverns IP-adress (för intern anslutning).
int serverPort = 6000;                                              // Serverns port för kommunikation.
EthernetClient client;                                              // Klient som möjliggör kommunikation.

// ********** Variabler för BT-anslutning **********
SoftwareSerial bluetooth = SoftwareSerial( btInPin, btOutPin );     // BT-modul som möjliggör kommunikation.
boolean btStartBitRecived = false;                                  // Markerar om startbit mottagits från ansluten BT-enhet.
char startString[] = "END\r";                                       // Sträng representerar en startbit.
byte startBitCounter = 0;                                           // Räknare för upptäckt av startbit.

// ********** Variabler för dataöverföring **********
String shadeID = "";                                                // Mottaget rullgardinens-ID från servern.
char commandToShade;                                                // Mottaget rullgardins-kommando från servern. 
String dataToShade = "";                                            // Mottagen data från servern.
byte decoderCount = 0;                                              // Räknare för avkodning av data från servern.
int shadePosFrequency = 700;                                        // Frekvens för uppdatering av rullgardinens position.

// -------------------------------------------------------------------------------------------------------------------------------



// ********** Funktionen kommunicerar med BT-modulen **********
void sendToBT( char* msg, boolean newLine = true ) {
  if( newLine ) {
    bluetooth.println( msg ); 
  } else {
    bluetooth.print( msg );
  }
}



// ********** Funktionen initierar BT-modulen och ansluter till angiven BT-enhet **********
void initBT() {
  // Seriekommunikation med 115200 bps upprättas mot BT-modulen.
  bluetooth.begin( 115200 );

  // Seriekommunikationen i BT-modulen ändras till 9600 bps.
  sendToBT( "$$$", false ); 
  delay( 10 );  
  sendToBT( "U,9600,N" );  
  delay( 10 );

  // Seriekommunikation med 9600 bps upprättas mot BT-modulen.
  bluetooth.begin( 9600 );
  delay( 10 );

  // BT-modulen upprättar anslutning till BT-enhet med angiven MAC-adress.
  sendToBT( "$$$", false );  
  delay( 10 );  
  sendToBT( "C,000666644282" );
  delay( 10 );
  
  // CMD avslutas i BT-modulen.
  sendToBT( "---" );
  delay( 10 );
  
  Serial.println( "Connected to bluetooth." );
}



// ********** Funktionen ansluter kontrollenheten till servern **********
void connectToServer() {
  if( client.connect(serverIp, serverPort) ) {
    Serial.println( "Connected to server." );
  } else {
    Serial.println( "Server connection failed." );
  }
}



// ********** Funktionen inväntar startbit från BT-enhet **********
void awaitServerCommunication() {
  char b = bluetooth.read();
    
  if( startBitCounter == 4 ) {
    btStartBitRecived = true;
    Serial.println();
    Serial.println( "OK to send and recive from server." );
    Serial.println();
  } else {
    if( b == startString[startBitCounter] ) {
      startBitCounter++;
    } else {
      startBitCounter = 0;
    }
  }
}



// ********** Funktionen skickar rullgardins-kommando till styrenheten och avlyssnar respons via BT **********
void sendToShade( char command ) {  
  bluetooth.print( command );
  delay( 100 );
  
  // Angivet kommando styr vad som händer.
  if( command == '2' ) {                          // Om mottaget kommando är 2 (stoppar rullgardinen)
    Serial.println( "Shade has been stoped." );
  } else if( command == '3' || command == '4' ) {   // Om mottaget kommando är 3 eller 4 (styr rullgardin ner/upp)
    Serial.println( "Shade is moving:" );
    getShadePos();  
  } else if( command == '5' ) {                   // Om mottaget kommando är 5 (uppdaterar info).
    Serial.print( "Shade info: " );
    getShadeInfo();
    Serial.println();
  } else if( command == '8' ) {                   // Om mottaget kommande är 8 (sätter temperatur och solljus).
    Serial.println( "Temperature and/or light has been set for shade." );
    Serial.println();
    setTempAndLight();
  }
}



// ********** Funktionen avläser rullgardinens position **********
void getShadePos() {
  boolean getPos = true;
  String shadePos;
  String lastShadePos = "last";
    
  // Förfrågan avseende rullgardinens position skickas till styrenheten.
  while( getPos ) {
    bluetooth.print( 6 );
    delay( 100 );
      
    // Rullgardinens position inhämtas från styrenheten.
    while( bluetooth.available() ) {
      char b = bluetooth.read();
      shadePos += b;
      delay( 10 );
    }
      
    // Eventuell data från servern avlyssnas för att upptäcka stopp av rullgardin.
    while( client.available() ) {
      if( client.read() == '2' ) {
        bluetooth.print( 2 );    
        delay( 100 );
        getPos = false;
      }
    }
      
    // Kontrollerar om rullgardinen står still annars sänds positionen till servern.
    if( shadePos.equals(lastShadePos) ) {
      getPos = false;
      Serial.println( "Shade has reached ending point." );
      Serial.println();
    } else {
      Serial.println( shadePos );
      client.println( shadePos );
      delay( 10 );
    }
      
    // Arbetsvariabler "nollställs".
    lastShadePos = shadePos;
    delay(10);
    shadePos = "";
    
    // Paus innan ny hämtning av rullgardinens position.
    delay( shadePosFrequency );
  }
}



// ********** Funktionen avläser rullgardinens värden **********
void getShadeInfo() {
  String shadeInfo = "";
  
  // Rullgardinens info inhämtas från styrenheten.
  while( bluetooth.available() ) {
    char b = bluetooth.read();
    shadeInfo += b;
    delay( 10 );
  }
    
  // Rullgardinens info vidarebefordras till servern.
  Serial.println( shadeInfo );
  client.println( shadeInfo );
}



// ********** Funktionen sätter min- och maxtemperatur samt solskydd för rullgardinen **********
void setTempAndLight() {
  String tempDown = "";
  String tempUp = "*";
  String ligthActive = "";
  byte counter = 0;
  
  // Kontrollerar och läser av tecken för tecken.
  for( byte i = 0; i < dataToShade.length(); i++ ) {
    char c = dataToShade.charAt( i );
    
    // Delar upp och lagrar mottagen data från servern.
    if( c != ':' && counter == 0 ) {
      tempDown += c;
    } else if( c != ':' && counter == 1 ) {
      tempUp += c;
    } else if( c != ':' && counter == 2 ) {
      ligthActive += c;
    } else {
      counter++;
    }
    
    // Värden vidarebefordras till styrenheten.
    if( counter == 3 ) {      
      bluetooth.print( tempDown );
      delay( 100 );
      bluetooth.print( tempUp );
      delay( 100 );
      bluetooth.print( ligthActive );
      delay( 100 );
    }
  }
}



// ********** Funktionen initierar programmet **********
void setup() {
  // Seriekommunikation upprättas med 9600 bps.
  Serial.begin( 9600 );
  delay( 1000 );

  Serial.println( "Connecting to network and server..." );
  Serial.println();

  // Nätverksanslutning upprättas utifrån angiven MAC- och IP-adress.
  Ethernet.begin( arduinoMac, arduinoIp );
  Serial.println( "Connected to network." );
  
  // Serveranslutning upprättas mot angiven IP-adress och port.
  connectToServer();
  
  // BT-modulen initieras.
  initBT();  
}



// ********** Funktionen kör programmet **********
void loop() {
  
  // Inväntar startbit från BT-enhet för att påbörja sänding till server.
  while( bluetooth.available() && !btStartBitRecived ) {
    awaitServerCommunication();
  }
  
  // Avlyssnar inkommande data över Serial.
  while( Serial.available() ) {
    char s = Serial.read();
    sendToShade( s );   
  }
  
  // Avlyssnar inkommande data från servern.
  while( client.available() ) {
    char c = client.read();
    
    // Delar upp och lagrar mottagen data från servern.
    if( c != ';' && decoderCount == 0 ) {
      shadeID += c;
    } else if( c != ';' && decoderCount == 1 ) {
      commandToShade = c;
    } else if( c != ';' && decoderCount == 2 ) {
      dataToShade += c;
    } else if( c == ';' ) {
      decoderCount++;
    }  
    
    // Kontrollerar om data från servern är redo att skickas till styrenheten.
    if( decoderCount == 3 ) {
      sendToShade( commandToShade );
      
      // Nollställer variabler.
      shadeID = "";
      dataToShade = "";
      decoderCount = 0;
    }
  }
}

