/*
 ---------------------------------------------------------------------------------------------------
Auteurs : CASSET LOUIS / GLOIRE BAYOUNDOULA
Responsable : Pascal BALLET
Programme Arduino
Stage L3 IFA
void setup()
{
  Serial.begin(9600);
}

void loop()
{
  Serial.print("Bonjour !");
  delay(1000);
}
 C'est préférable de lancer ce programme avant les 10 minutes suivants une heure car il effectue 6 mesures à intervalle de 10 minutes,
 afin de calculer le taux de Co2 moyen par heure dans la pièce.

 (Lancé à 9h00 => Mesures à 9h00, 9h10, 9h20, 9h30, 9h40, 9h50)

 (Si lancé à 9h13 => Mesures à 9h13, 9h23, 9h33, 9h43, 9h53, 10h03,
  or cette dernière devrait entrer dans la moyenne de l'heure suivante.)
 ---------------------------------------------------------------------------------------------------
 */


#include <NTPClient.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <WiFiUdp.h>


//Infos Firebase
#define API_KEY "AIzaSyASUdodD9B57Pq5mgmArHja6ZGeRfcMvBs"
#define USER_EMAIL "esp32-firebase@gmail.com"
#define USER_PASSWORD "Esp32Firebase"
#define DATABASE_URL "https://esp32project-c06f1-default-rtdb.europe-west1.firebasedatabase.app"

//const char* ssid = "Redmi Note 11";
//const char* password = "aaaazzzz";
// Wifi infos
const char* ssid = "DEPINFOWIFI2";
const char* password = "depinfowifi2021!";


// Entrée d'information du capteur
int sensorIn =35;
// Entrées des LED
int ledR =13;
int ledO =14;
int ledV =12;

int i = 0;
int count = 0;
char buffer[200];
char minute1[100];
char minute2[100];
char minute3[100];
char minute4[100];
bool calibrage = false;
char* num;
float val;
float concentration;

// Definition des objets Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Variable pour sauvegrader le USER UID
String uid;

// Variables de temps

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 2 * 3600;
const int daylightOffset_sec = 0;
struct tm timeinfo;


//Fonction setup
void setup()
{

// Pour le moniteur.
Serial.begin(115200);
Serial.println("SCD30 Example");
pinMode(ledR, OUTPUT); // Mettre le pin ledR en sortie.
pinMode(ledO, OUTPUT); // Mettre le pin ledO en sortie.
pinMode(ledV, OUTPUT); // Mettre le pin ledV en sortie.


//Connexion Wifi
WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("\nConnecting");

while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(100);
    }

Serial.println("\nConnexion au wifi réussie !");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());



//Connexion Firebase
config.api_key = API_KEY;
config.database_url = DATABASE_URL;
auth.user.email = USER_EMAIL;
auth.user.password = USER_PASSWORD;

/*----------------------------------------*/
Firebase.reconnectWiFi(true);
/*----------------------------------------*/


signupOK = true;
/*----------------------------------------*/
/* Assigne la fonction callback  pour stocker la variable de generation de tâches tokenStatusCallback */
config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

Firebase.begin(&config, &auth);
/*----------------------------------------*/
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Affichage user UID
  Serial.println("Connexion à la FireBase réussie !");
  Serial.print("User UID: ");
  Serial.println(uid);
  Serial.println("Récupération de la date ");

}





void loop(){
//Valeur  récupérée ( en mV divisé par 1000 pour volt )
//Conversion en ppm ( 1V = 1000ppm, 2V = 2000ppm etc... )
//float voltage = analogRead(sensorIn)/1000.0;
//float voltagedif = voltage-0.4;
//float concentration=voltagedif*1000.0;
//Nous pouvons éviter le /1000 puis *1000 en faisant -400 de base au lieu du 0.4 entre.
if (calibrage == false ){
  //delay (180000);
  Serial.println("attendre 3 min ");
  calibrage = true;  
}else {
val=analogRead(sensorIn);
float voltage = val*(3.3/4095.0);
  if(voltage == 0)
  {
    Serial.println("voltage egal a zero");
  }
  else if(voltage < 0.4)
  {
    Serial.println("preheating");
  }
  else {
    float voltageDiference=voltage-0.4;
    float concentration=(voltageDiference*5000.0)/1.6;
    Serial.print("la concentration de CO2 est ");
    Serial.print(concentration);
    Serial.println("ppm");
    // Si la concentration est supérieure à 0, il n'y a pas de problème.
   if (concentration>0){

   // Si la concentration est inférieure à 1000 => LED verte, tout va bien
   if (concentration < 800){
   digitalWrite(ledV,HIGH);
      digitalWrite(ledO,LOW);
      digitalWrite(ledR,LOW);
      }
      // Si la concentration est inférieure à 1000 => LED orange, il faut commencer à aérer
      else if (concentration >= 800 && concentration < 1000){
      digitalWrite(ledO,HIGH);
      digitalWrite(ledV,LOW);
      digitalWrite(ledR,LOW);
      }
      // Si la concentration est inférieure à 1000 => LED rouge, il faut vraiment aérer
      else if (concentration >= 1000){
      digitalWrite(ledR,HIGH);
      digitalWrite(ledO,LOW);
      digitalWrite(ledV,LOW);
      }

     
   
   if(!getLocalTime(&timeinfo)){
    Serial.print(".");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }


  else{

    //If regardant si la Firebase est prete, et si le temps d'attente est suffisant.
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 900000/*Temps d'attente*/ || sendDataPrevMillis == 0)){
        sendDataPrevMillis = millis();
        
        int heure = timeinfo.tm_hour;
        int min = timeinfo.tm_min;
        int jour = timeinfo.tm_mday;
        int mois = 1+timeinfo.tm_mon;
        int annee = 1900+timeinfo.tm_year;
        Serial.println("Dans la boucle ");
        if (count == 24) {
          count = 0;
        }

      if (i==0){
         //Changer le nom de la salle ici si ce n'est pas la salle correspondante.
            if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);
             
                strcat(strcpy(buffer, buffer),"/min1/time ");
                sprintf (minute1,"%d H %d", heure,min);                                                  
                printf("%s\n", minute1);
             if (Firebase.RTDB.setString(&fbdo, buffer, minute1)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
            else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            } 
            if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);   
             printf("%s\n", buffer); 
             strcat(strcpy(buffer, buffer),"/min1/taux de CO2 ");
             if (Firebase.RTDB.setInt(&fbdo, buffer, concentration)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
            else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            } 
           
      }
        //  deuxieme part de l'heure 
         if (i==1){
         //inserer le jour 
                if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);
             
            strcat(strcpy(buffer, buffer),"/min2/time ");
                sprintf (minute2,"%dH%d", heure,min);
               
               
            if (Firebase.RTDB.setString(&fbdo, buffer, minute2)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
        // inserer l'heure 
         else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            }    
              if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);   
              
             strcat(strcpy(buffer, buffer),"/min2/taux de CO2 ");
             if (Firebase.RTDB.setInt(&fbdo, buffer, concentration)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
            else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            } 
            
       
        } 
        // mesure de la minute 4 de l'heure 
        if (i==2){
         //inserer le jour 
                if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);
             
            strcat(strcpy(buffer, buffer),"/min3/time ");
                sprintf (minute3,"%dH%d", heure,min);
               
            if (Firebase.RTDB.setString(&fbdo, buffer, minute3)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
        // inserer l'heure 
         else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            }  
             if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);   
             printf("%s\n", buffer); 
             strcat(strcpy(buffer, buffer),"/min3/taux de CO2 ");  
             
            
             if (Firebase.RTDB.setInt(&fbdo, buffer, concentration)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
            else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            } 
            
       
        } 
         if (i==3){
         //inserer le jour 
               if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);
             
            strcat(strcpy(buffer, buffer),"/min4/time ");
                sprintf (minute4,"%dH:%d", heure,min);
               
            if (Firebase.RTDB.setString(&fbdo, buffer, minute4)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
        // inserer l'heure 
         else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            }    
              if (asprintf(&num, "%d", count) == -1) {
                perror("asprintf");
               } else {
                strcat(strcpy(buffer, "Micro 11A/jour1/heure "), num);
              }
              
                  printf("%s\n", buffer);
                  free(num);   
                 printf("%s\n", buffer); 
                 strcat(strcpy(buffer, buffer),"/min4/taux de CO2 ");
             if (Firebase.RTDB.setInt(&fbdo, buffer, concentration)){
              Serial.println("PASSED");
              Serial.println("PATH: " + fbdo.dataPath());
              Serial.println("TYPE: " + fbdo.dataType());
            }
            else {
              Serial.println("FAILED");
              Serial.println("REASON: " + fbdo.errorReason());
            } 
            count++;
            i = -1; 
        } 
         i++; 
         Serial.println(i);
        }
         

          


                
       

      }
      
  }
   else {
  Serial.print("Erreur concentration nulle ou négative");
}
  }
 
  delay(3000);
  }
}

