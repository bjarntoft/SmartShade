/*
Programmet representerar en styrenhet till en rullgardin.
 Programmet låter användaren styra gardinen upp eller ner med hjälp
 av en anpassad androidapplikation. 
 Användaren kan styra gardinen med hjälp av knapparna "up" eller "down" i klienten, 
 men gardinen kan även styras med hjälp av temperaturstyrning, ljusstyrning och
 tidsstyrning.
 Programmet är gjort för projektarbetet för årskurs 1 på Malmö Högskola 2014.
 */


// ********** Import av bibliotek **********
#include <SoftwareSerial.h>
#include <Servo.h> 
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>



// ********** Tilldelning av funktion till kopplingspinnar **********
const int btOutPin = 4;                // TX på BT - modul.
const int btInPin = 5;                 // RX på BT - modul.
const int servoPin = 10;               // utgång där servon får signal för rotationsriktning.
const int rotationPinA = 2;            // Ingång för rotationsavkodaren pin A.                      
const int rotationPinB = 3;            // Ingång för rotationsavkodaren pin B.
const int lightPin = A1;               // Analog ingång för ljus.



// ********** Variabler för BT-anslutning **********
SoftwareSerial bluetooth( btOutPin, btInPin );
char btReceivedCommand;                // Variabel som tar emot data via bluetooth.



// ********** Variabler för temperaturmätning **********
float tempCelsius;                     // Variabel där den avlästa temperaturen lagras.
float tempMax =-1;                     // Variabel som talar om den inställda maxtemperaturen.
float tempMin =-1;                     // Variabel som talar om den inställda mintemperaturen.
long tempLastCheckTime = 0;            // Variabel för senast avlästa temperatur i tid.
const int tempUpdateTime = 9755;       // Variabel som talar om väntetiden mellan varje avläsning av temperatur.
String tempCelsiusString;              // Lagrar den inlästa temperaturen som en string.
#define ONE_WIRE_BUS 9                 // Tilldela digital utgång 9 till oneWire Bus.
OneWire oneWire(ONE_WIRE_BUS);         // Sätter upp en oneWire instans att kommunicera med alla oneWire - enheter.
DallasTemperature sensors(&oneWire);   // Skickar vidare oneWire referensen till Dallas Temperature.



// ********** Variabler för ljusmätning **********
int lightRawValue;                     // Variabel som lagrar det inlästa ljusvärdet.
String lightIsActive ="false";         // Variabel som sätts till true när ljusstyrning är aktivt.
String lightRawValueString="";         // Lagrar det inlästa ljusvärdet till string.
long lightLastCheckTime = 0;           //  Variabel för senast avlästa temperatur i tid.  
const int lightUpdateTime = 1000;      // Variabel som talar om väntetiden mellan varje avläsning av ljus.
long lightTimerDown, lightTimerUp, lightStartTimerDown =0 , lightStartTimerUp=0;    // Variabler som används när gardinen ska upp eller ner vid ljusstyrning.
const long lightLimit = 650;           // Variabel som håller den gränsen mellan soligt och inte soligt.
const int lightMargin = 10000;         // Variabel som håller tiden på hur länge det kan vara sol eller inte sol innan gardin hissas upp/ner.



// ********** Variabler för servostyrning **********
Servo servo;
const int servoUp = 82;                // Variabel som innehåller servons hastighet upp.
const int servoDown = 105;             // Variabel som innehåller servons hastighet ner.
const int servoStop = 95;              // Variabel som innehåller servons stop - läge.
boolean servoIsActive = false;



// ********** Variabler för rotationsavkodare **********
volatile int rotLastEncoded =0;        
byte savedrotValue = EEPROM.read(2);   // Håller det inlästa rotationsvärdet. Avläses från EEPROM vid uppstart.
int rotValue= (savedrotValue *5) ;     // Variabeln innehåller det akutella rotationsvärdet.
const int rotMin = 0;                  // Variabeln talar om gardinens minsta position som den kan ha.
const int rotMax = 100;                // Variabeln talar om gardinens högsta position när utrullad.  



// ********** Variabler övriga **********
char directionFlag;                    // Variabeln håller koll på riktningen gardinen har. 
int tempCounter =0;                    // Variabel som håller koll på antal loopar programmet gör.



// ********** Funktionen kommunicerar med BT-modulen **********
void sendToBT( char* msg, boolean newLine = true ) {
  if( newLine ) {
    bluetooth.println( msg ); 
  } 
  else {
    bluetooth.print( msg );
  }
}



// ********** Funktionen initierar BT-modulen **********
void initBT() {
  // Seriekommunikation med 115200 bps upprättas mot BT-modulen.
  bluetooth.begin( 115200 );

  // CMD öppnas i BT-modulen och seriekommunikationen ändras till 9600 bps.
  sendToBT( "$$$", false ); 
  delay( 10 );  
  sendToBT( "U,9600,N" );  
  delay( 10 );
  // Seriekommunikation med 9600 bps upprättas mot BT-modulen.
  bluetooth.begin( 9600 );
  delay( 10 );
}



// ********** Funktionen beräknar rullgardinens position **********
void updateEncoder(){

  // Läser avkodarens bitar.
  int MSB = digitalRead( rotationPinA ); 
  int LSB = digitalRead( rotationPinB ); 
  int encoded = (MSB << 1) | LSB; 
  int sum  = (rotLastEncoded << 2) | encoded; 

  // Kontrollerar om rullgardinen hissas upp eller ner.
  // Variabeln sum initieras med olika värden beroende på rotationens riktning.
  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) rotValue --;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) rotValue ++;

  // Lagrar det aktuella rotationsvärdet i savedrotValue.
  savedrotValue = (rotValue/5); 

  // Ifall rotationsvärdet är mellan 255 - 250 , sätt det till 0.
  if(savedrotValue <=255 && savedrotValue >=252){
    savedrotValue =0; 
  }
  
  // Spara värdet i EEPROM.
  EEPROM.write(2, savedrotValue);
  //EEPROM.write(2, (0));                                         //För att nollställa EEPROM, ladda upp 2 gånger!.
  delay(5);
  rotLastEncoded = encoded;
}



// ********** Funktionen avläser temperaturen **********
void readTemp() {  

  // Skapar variabler för att dela upp temperaturen i en sträng.
  int first;
  int second;

  // Anropar sensorn att man vill ha temperatur. 
  sensors.requestTemperatures();
  // Hämta och lagrar temperaturen i variabeln tempCelsius.
  tempCelsius = sensors.getTempCByIndex(0);
  first = tempCelsius;
  second = (tempCelsius - first)*10.0;

  // Gör en sträng av det avlästa värdet för sändning till kontrollenhet.
  tempCelsiusString ="";
  tempCelsiusString += first;
  tempCelsiusString+=".";  
  tempCelsiusString +=second;

  // Automatisk temperaturnedrullning av rullgardin är aktiverad och maxtemp uppnådd rullas gardin ner.
  if(tempCelsius >= tempMax && tempMax!=-1 && savedrotValue<100) {
    servoRollDown(); 
  }
  // Automatisk temperaturupprullning av rullgardin är aktiverad och mintemp uppnådd rullas gardin upp.
  else if  (tempCelsius <= tempMin  && tempMin!=-1 && savedrotValue >0) {
    servoRollUp(); 
  }
}



// ********** Funktionen avläser om solen står på eller inte samt kontrollerar om gardin ska styras av ljuset **********
void readLight() {

  lightRawValue = analogRead( lightPin );
  delay(20);
  if(lightRawValue <lightLimit){
    lightRawValueString ="NO"; 
  }
  else{
    lightRawValueString ="YES"; 
  }
  // Konrollerar om solen står på och solskydd är aktiverat.
  if( lightRawValue > lightLimit && lightIsActive.equals("true") ) {
    // Kontrollerar om timern för nerrullning är noll.
    if( lightStartTimerDown == 0 ) {
      lightStartTimerDown = millis();
    } 
    else {
      lightTimerDown = millis();
    }
  } 
  if( lightRawValue < lightLimit || lightIsActive.equals("false") ) {
    lightStartTimerDown = 0;
    lightTimerDown = 0;
  } 

  // Konrollerar om solen inte står på och solskydd är aktiverat.
  if( lightRawValue < lightLimit && lightIsActive.equals("true") ) {
    // Kontrollerar om timern för nerrullning är noll.
    if( lightStartTimerUp ==0 ) {
      lightStartTimerUp = millis();
    } 
    else {
      lightTimerUp = millis();
    }
  }  
  // Kontrollerar om ljusstyrningen ska vara aktiv.
  if( lightRawValue > lightLimit || lightIsActive.equals("false") )  {
    lightStartTimerUp = 0;
    lightTimerUp = 0;
  }

  // Kontrollerar om solen stått på eller av i 10 sekunder.
  if( (lightTimerDown - lightStartTimerDown) > lightMargin ) {
    servoRollDown();
    lightStartTimerDown = 0;
    lightTimerDown = 0;

  } 
  // Kontrollerar om solen stått av i 10 sekunder. Rulla upp gardin.
  if( (lightTimerUp - lightStartTimerUp) > lightMargin ) {
    servoRollUp();
    lightStartTimerUp = 0;
    lightTimerUp = 0;
    
  }
}



// ********** Funktion som stannar servon **********
void servoRollUp(){
  servo.attach( servoPin );
  servo.write( servoUp );
  directionFlag = '4';
  servoIsActive = true;
}



// ********** Funktion som rullar ner servon **********
void servoRollDown(){
  servo.attach( servoPin );
  servo.write( servoDown );
  directionFlag = '3';
  servoIsActive = true;
}



// ********** Funktion som rullar upp servon **********
void stopServo(){
  servo.detach( );
  servo.write( servoStop );
  directionFlag ='0';
  servoIsActive = false;
}



// ********** Funktion som kontrollerar det skickade värdet från kontrollenheten **********
void controllInput(char btReceivedCommand ){
  
  // Styrenheten reagerar baserat på mottaget kommando.
  if( btReceivedCommand == '2') {                                             // Rullgardin stoppas.  
    stopServo();
    servoIsActive = false;
  } 
  else if( btReceivedCommand == '3') {                                        // Rullgardin rullar ner.
    servoRollDown();
  } 
  else if(btReceivedCommand == '4') {                                         // Rullgardin rullas upp.
    servoRollUp();
  } 
  else if( btReceivedCommand == '5' ) {                                       // Temperaturen, ljus & position skickas.
    String msg ="1;"; 
    msg +=tempCelsiusString;
    msg +=";";
    msg +=lightRawValueString;
    msg+=";";
    msg+=savedrotValue;
    bluetooth.print( msg );
    delay( 10 );
  } 

  else if( btReceivedCommand == '6' ) {                                       // Rotationsavkodarens värde skickas.
    String msg ="2;";
    msg+= savedrotValue;
    bluetooth.print( msg );
    delay( 5 );
  }
  else if(btReceivedCommand == '7') {                                         // Ta bort aktuell anslutning med master.
    sendToBT("$$$", false);                                                  
    delay(30);
    sendToBT("k,1");
    delay(30);
    sendToBT("---");
    delay(30);
  }
  else if(btReceivedCommand == '8') {                                         // Lagrar max - & min - temperaturen från klienten.
    String res="";                                                            // Aktiverar / Avaktiverar ljusstyrning.
    for(byte i=0; i<3;i++){ 
      delay(200);

      // Läser av den maximala temperaturen för temperaturstyrning.
      if(i == 0 ){
        if(bluetooth.available()){ 
          tempMax = bluetooth.parseFloat();
        }
      }
      // Läser av den minimala temperaturen för temperaturstyrning.
      else if(i == 1){
        if(bluetooth.available()){ 
          tempMin = bluetooth.parseFloat();
        }  
      }
      // Kontrollerar om ljusstyrning ska vara på eller av.
      else if (i == 2){
        while (bluetooth.available()){
          char c = bluetooth.read();
          res += c;
        }
        lightIsActive = res;
      }
    }
  }
}



// ********** Funktionen initierar styrenheten **********
void setup() {

  // rensar bluetooth - buffert.
  while(bluetooth.available()){
    bluetooth.read();
  }

  // Definierar in- och utgångar.
  delay(10);
  pinMode( lightPin, INPUT );
  pinMode( rotationPinA, INPUT );
  pinMode( rotationPinB, INPUT );

  // Aktiverar pull-up resistorer.
  digitalWrite( rotationPinA, HIGH );
  digitalWrite( rotationPinB, HIGH );

  // Seriekommunikation startas med 9600 bpm.
  Serial.begin( 9600 );
  delay( 1000 );

  // Initierar BT-modul.
  initBT();
  //Påbörja sökning av temperatur-sensorer.
  sensors.begin();
  // Initierar temperaturen.
  readTemp();
  // Initierar ljus.
  readLight();
}



// ********** Funktionen kör styrenheten **********
void loop() {

  // Uppdaterar avkodarens värde.
  updateEncoder();
  delay(20);
  // Kontrollerar om rotationsavkodarens värde når max - eller minvärdet och stannar då gardinen.
  if((savedrotValue >= rotMax && directionFlag =='3') || ( savedrotValue <= rotMin && directionFlag =='4')){ 
    stopServo();
  }

  // Kontrollerar om styrenheten mottagit något kommando.
  if(bluetooth.available()) {
    // Läser inkommande bluetooth - meddelande.
    btReceivedCommand = bluetooth.read();
    // Anropar metod som kontrollerar inkommande meddelande.
    controllInput(btReceivedCommand);
  }
 
  // Kontrollerar temperaturen.
    if(!servoIsActive && (millis() - tempLastCheckTime) > tempUpdateTime ) {
        if(tempCounter <200){
          tempCounter++;
        }else{
      tempLastCheckTime = millis();
      readTemp();
      tempCounter =0;
      }
    }
    
  // kontrollerar ljusstyrkan.
  if( (millis() - lightLastCheckTime) > lightUpdateTime ) {
    lightLastCheckTime = millis();
    readLight();
  }
}

