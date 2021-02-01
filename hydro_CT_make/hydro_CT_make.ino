#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DHT.h>

//Definitionen DHT 22 Temperatursensor
#define DHTPIN 13  
#define DHTTYPE DHT22    
DHT dht(DHTPIN, DHTTYPE); 

#define DEBUG true // Serielle Ausgabe bei true aktivieren

//Netzwerk und Zeitserverinfos
#define SSID "ssid"
#define PASSWORD "pw"
#define NTP_SERVER "de.pool.ntp.org"
#define TZ_INFO "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00" // Western European Time

WebServer server(80);

// Zeitvariablen
struct tm local;
time_t now;
double time_now; //Zeitpunkt jetzt
double time_p=0; //Zeitpunkt letztes Pumpen
double time_m=0; // Zeitpunkt letztes messen
int lastday=0;

bool pON=false; // Pumpe an
bool pm=false;

int tp=25;   //Gieszeit in sec
int tm=300;  //Intervall messen in sec
int intervall=93*60; //Intervall giesen in sec 
int tint=10; 

float luftfeuchte=10;
float temp=10;
float Vakku=12.00;
float VADC=0;

// Errorvariablen
byte error=0;
byte wifierror=0;
String serror="no Error";

int indx=0;
int rows=0;

const int punkte=150; // Anzahl der Speicherpunkte
String azeit[punkte]; // Ringpuffer Zeitstring der Messung
float atemp[punkte]; // Ringpuffer Temperatur
float aVakku[punkte]; // Ringpuffer Akkustand

unsigned int aLF[punkte];
float ap1[punkte];
//unsigned int ap2[punkte];

// Pindefinitionen
byte pumpe=19;
byte adcAkku=32;
byte adcSensor=33;
byte led=2;  // Board LED
byte ledg=27;
byte ledy=14;
byte ledr=12;
byte t1=36;
byte t2=39;
byte t3=34;
byte t4=25;
byte t5=26;
bool t1n=false; // Tasterzustände now und last
bool t1l=false;
bool t2n=false; 
bool t2l=false; 
bool t3n=false; 
bool t3l=false; 
bool t4n=false; 
bool t4l=false; 
bool t5n=false; 
bool t5l=false;
 

void setup() {
  // Pindefinitionen
  pinMode(pumpe, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(ledg, OUTPUT);
  pinMode(ledy, OUTPUT);
  pinMode(ledr, OUTPUT);
  pinMode(t1, INPUT);
  pinMode(t2, INPUT);
  pinMode(t3, INPUT);
  pinMode(t4, INPUT);
  pinMode(t5, INPUT);
  pinMode(adcSensor, INPUT_PULLUP);
  pinMode(adcAkku, INPUT_PULLUP);

  //Temperatursensor starten
  pinMode(DHTPIN, INPUT);
  dht.begin();
  
  digitalWrite(led, HIGH);
  digitalWrite(ledg, HIGH);
  digitalWrite(ledy, HIGH);
  digitalWrite(ledr, HIGH);
  
  if (DEBUG)
     {
     Serial.begin(115200); // bei Debug seriellen Monitor aktivieren
     }
  
  // Initialisierung Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    debug(".");
  }
  debug(" ");
  debug("IP-Adresse: " + WiFi.localIP().toString());

  digitalWrite(ledr, LOW);

  // NTP Zeit aktualisieren
  if (MDNS.begin("esp32")) {
    debug("MDNS-Responder gestartet.");
  }
  debug("Hole NTP Zeit");
  configTzTime(TZ_INFO, NTP_SERVER); 
  getLocalTime(&local, 10000);   // Systemzeit mit NTP synchronisieren

      
  digitalWrite(ledy, LOW);

  // Webserver initialisieren
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  debug("HTTP-Server gestartet.");
  digitalWrite(ledg, LOW);
  digitalWrite(led, LOW);
}


void loop() {
  // Tasterabfrage
  t1n=digitalRead(t1);
  t2n=digitalRead(t2);
  t3n=digitalRead(t3);
  t4n=digitalRead(t4);
  t5n=digitalRead(t5);

  server.handleClient(); // Liegt eine Webserveranfrage vor?

  // Zeitvariablen aktualisieren
  time(&now);
  localtime_r(&now, &local);
  time_now=mktime(&local);

  delay(50);

  if (t1n==true and t1l==false) // Taster gedrückt, hier kann eigener Code ergänzt werden
  {
   debug("t1");
  }
  if (t2n==true and t2l==false) 
  {
  debug("t2");
  }
  if (t3n==true and t3l==false) 
  {
   debug("t3");
   time_p=0; // giesvorgang starten
  }
  if (t4n==true and t4l==false) 
  {
   debug("t4");
  }
  if (t5n==true and t5l==false) 
  {
   debug("t5");
  }

  t1l=t1n; // aktuelle Tasterzustände speichern
  t2l=t2n;
  t3l=t3n;
  t4l=t4n;
  t5l=t5n;

  if (local.tm_mday != lastday) {   //alle 24h Systemzeit aktualisieren
    
    configTzTime(TZ_INFO, NTP_SERVER); 
    getLocalTime(&local, 10000);  // ESP32 Systemzeit mit NTP Synchronisieren
    lastday=local.tm_mday;
  }

  
  if (difftime(time_now, time_b)>10){ // Blinken alle 10 s
  time_b=time_now;
    if (error ==0) {
       digitalWrite(ledg, HIGH);
       if (wifierror==1) {  digitalWrite(ledy, HIGH);}  
       delay(100);
       digitalWrite(ledg, LOW);
       digitalWrite(ledy, LOW);
    }
    if (error ==1) {
       digitalWrite(ledr, HIGH);
       if (wifierror==1) {  digitalWrite(ledy, HIGH);}  
       delay(100);
       digitalWrite(ledr, LOW);
       digitalWrite(ledy, LOW);
    }
    
  }

  if (difftime(time_now, time_m)>tm){ // messen alle tm sekunden
   
   messen(); 
   wlanstatus();
    
  }

  if (difftime(time_now, time_p)>intervall){  //Giesen jededes Intervall
        time_p=time_now;
      
          if (!pON){
              intervall=92*60; //default intervall
              if (error==0){
              pON=true;
              intervall=tp;   //tp = Gieszeit
              digitalWrite(led, HIGH);
              digitalWrite(pumpe, HIGH);    
              }
          }
         
      
          else if (pON) {   
          pON=false;

          digitalWrite(led, LOW);
          digitalWrite(pumpe, LOW);
          
            if (temp<=10) {intervall=360;}
            else if (temp>10 && temp<=15) {intervall=180*60;}
            else if (temp>15 && temp<=20) {intervall=120*60;}
            else if (temp>20 && temp<=25) {intervall=90*60;}
            else if (temp>25 && temp<=30) {intervall=60*60;}      
            else if (temp>30) {intervall=30*60;}
            else {intervall=91*60;}; //Fehler in Temp abfangen

            azeit[indx]=zeitstr2();

            if (isnan(temp) || temp>60 ) {temp=0;} //Fehler in Temp abfangen
            if (isnan(luftfeuchte) || luftfeuchte > 100) {luftfeuchte=0;}

            // Ringpuffer beschreiben
            atemp[indx]=temp;
            aVakku[indx]=Vakku;
            aLF[indx]=luftfeuchte;
            indx=indx+1;
            if (rows<punkte) {rows=rows+1;} // Anzahl Datenpunkte für Ausgabe
            if (indx==punkte) {indx=0;}  
          }

    
       }
  
}

void handleRoot() {

  messen(); 

String td="</td><td>";

String message="<html><head><script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script><script type=\"text/javascript\">google.charts.load('current', {'packages':['corechart']});";
message += "google.charts.setOnLoadCallback(drawChart);function drawChart() {var data = new google.visualization.DataTable();data.addColumn('date', 'Zeit');data.addColumn('number', 'Temp');";
message += "data.addColumn('number', 'Volt');data.addColumn('number', 'Luftf');data.addRows([";
 
 int j= indx-1;
 if (j<0) {j=punkte-1;}

 for (int i = 1; i <= rows; i++) {
            message += "[new Date("  +String(azeit[j]) +"),"+ String(atemp[j])  +","+  String(aVakku[j])  +","+  String(aLF[j]) + "],"; 
            j=j-1;
            if (j<0) {j=punkte-1;}     
        }

       
message += "]);var date_formatter = new google.visualization.DateFormat({ pattern: \"dd.MM HH:mm\"});date_formatter.format(data, 0);";
message += "var options = {curveType: 'function',legend: { position: 'bottom' },hAxis: {format: 'HH:mm'},pointSize: 0, chartArea:{left:20,top:10}};var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));";
message += "chart.draw(data, options);}</script><style>table {border-collapse: collapse;} table, td, th {border: 1px solid black;text-align: left;}</style></head>";



  message += "<body style=\"background-color:white;\"><font face=\"Calibri\" size=\"1\"><h1>Hydrokultur</h1><BR><table><tr><th>Luftf.</th><th>Temp</th><th>VADC</th><th>V Solar</th><th>Int</th><th>tp</th></tr>";
  
  message += "<tr><td>" + String(luftfeuchte)+td+String(temp)+td+String(VADC)+td+String(Vakku)+td;
  message += String(intervall/60)+td+String(tp) + "</td></tr>";
  message += "<tr><th>Zeit</th><th>min run</th><th>Error</th><th>Wlan Code</th></tr><tr><td>";
  
  message += zeitstr() +td+String(difftime(time_now, time_p)/60)+td+serror+td+String(WiFi.status());
  message += "</td></tr></table><div id=\"curve_chart\" style=\"width: 520px; height: 400px\"></div>"; 
  message += "</body></html>"; 
  
server.send(200,"text/html", message);
}

void handleNotFound(){
  String message="Pfad ";
  message += server.uri();
  message += " wurde nicht gefunden.\n";
  server.send(404, "text/plain", message);
}

void debug(String Msg)
{
 if (DEBUG)
 {
 Serial.println(Msg);
 }
}

String zeitstr()
{
      String zeit="";  

      if (local.tm_hour<10) {zeit=zeit + "0" + String(local.tm_hour)+ ":";}
      else {zeit=zeit + String(local.tm_hour)+ ":";}
      if (local.tm_min<10) {zeit=zeit + "0" + String(local.tm_min)+ ":";}
      else {zeit=zeit + String(local.tm_min)+ ":";}
      if (local.tm_sec<10) {zeit=zeit + "0" + String(local.tm_sec);}
      else {zeit=zeit + String(local.tm_sec);}

  return zeit;
}

String zeitstr2() 
{
      String zeit="";  
      //[new Date(1789,3,30,12,1,1),37.8,80.8,41.8],
      zeit+= String(local.tm_year +1900) +","+ String(local.tm_mon) +","+ String(local.tm_mday) +","+ String(local.tm_hour) +","+ String(local.tm_min) +","+ String(local.tm_sec);
     

  return zeit;
}

void messen()
{
    time_m=time_now;
    luftfeuchte=dht.readHumidity();
    temp=dht.readTemperature();
    VADC=analogRead(adcAkku)*3.3/4095;
    VADC=analogRead(adcAkku)*3.3/4095; //erste Messung fehlerhaft
    Vakku=1.11 + 5.56*VADC;
     if (Vakku < 10) //powersaving on low akku 
      {
      error=1;
      serror="low Power ";
      }     
      else
      {
        error=0;
        serror="no error";
      }


      
  
}

void wlanstatus(){

// netzwerkstatus überprüfen
      if (WiFi.status() != WL_CONNECTED) {
        wifierror=1;
        tm=30; // alle 30 s versuchen neu zu verbinden
        WiFi.begin(SSID, PASSWORD); //try reconnect            
      }
      if(WiFi.status() == WL_CONNECTED)
      {
        wifierror=0;
        tm=300;
      }

}
