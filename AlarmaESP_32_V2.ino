// Librerias para tarjeta SD
#include "FS.h"
#include "SD.h"
#include <SPI.h>

// Librerias para WiFi y obtener fecha/hora de servidor NTP
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "time.h"

//Librerías de conexión a red
#include "data.h"

// Librerías para Telegram
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>


// Replace with your network credentials
//const char* ssid = "Pon tu Red";
//const char* password = "Pon la Clave";

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you

const char* ServerName = "Sistema Esp32"; // Address to the server with http://esp32cam.local/

String local_hwaddr; // WiFi local hardware Address
String local_swaddr; // WiFi local software Address

// Initialize Telegram BOT
//String chatId = "Usuario Telegram"; // User ID
//String BOTtoken = "Token del BOT";

int sts_ENT = false;
int sts_HAB = false;
int sts_DES = false;

WiFiClientSecure clientTCP;

UniversalTelegramBot bot(BOTtoken, clientTCP);

#define P_ENT 34 // P_ENTRADA PIN: GPIO 5
#define P_HAB 35 // P_hABITACION PIN: GPIO 18
#define P_DES 36 // P_DESPACHO PIN: GPIO 19

#define ledConnect 32  //GPIO 4 
#define ledStatus 33 //GPIO 22
#define ledAlarma 21  //GPIO 15
//#define ledCam 12 //GPIO 12

#define Default_LED 2

// Define CS pin for the SD card module
#define SD_CS 5

//Variables Alarma
bool msgEntradaSent = 0;
bool msgHabitacionSent = 0;
bool msgDespachoSent = 0;

bool Sist_Activo = false;

bool EnaP_ENT = 0;
bool EnaP_HAB = 0;
bool EnaP_DES = 0;




//Obtener fecha y hora
struct tm timeinfo;
/*
  struct tm
  {
  int    tm_sec;   //   Seconds [0,60].
  int    tm_min;   //   Minutes [0,59].
  int    tm_hour;  //   Hour [0,23].
  int    tm_mday;  //   Day of month [1,31].
  int    tm_mon;   //   Month of year [0,11].
  int    tm_year;  //   Years since 1900.
  int    tm_wday;  //   Day of week [0,6] (Sunday =0).
  int    tm_yday;  //   Day of year [0,365].
  int    tm_isdst; //   Daylight Savings flag.
  }
*/

//const char* ntpServer = "pool.ntp.org";
const char* ntpServer = "time.google.com";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 7200;

// Variables to save date and time
int second;
int minute;
int hour;
int day;
int month;
int year;

long current;

char Fecha[12];
char Hora[12];


int botRequestDelay = 1000; // mean time between scan messages
long lastTimeBotRan; // last time messages’ scan has been done

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long interval = 30000;


void handleNewMessages(int numNewMessages); // Prototipo Función para manejo mensajes

// Función`para obtener estado y devolverlo como cadena
String getReadings() {
  long rssi = WiFi.RSSI() + 100;

  String message = ObtieneFechahora();

  message += "\n\rSeñal WiFi: " + String(rssi) + "\n";
  if (rssi > 50) {
    message += (F(" (>50 -- Buena)"));
  } else {
    message += (F(" (Podría ser mejor)" ));
  }
  message += "\n\n Sistema ";
  if (Sist_Activo == true) message +=  "Activado\n"; else message += "Desactivado\n";

  message += "*-> Sensor 1 ";
  if (EnaP_ENT == true) message +=  "Activo\n"; else message += "Deshabilitado\n";

  message += "*-> Sensor 2 ";
  if (EnaP_HAB == true) message +=  "Activo\n"; else message += "Deshabilitado\n";

  message += "*-> Sensor 3 ";
  if (EnaP_DES == true) message +=  "Activo\n"; else message += "Deshabilitado\n";

  return message;
}
// Fin Función`para obtener estado y devolverlo como cadena


String ObtieneFechahora() {

  String msg;
  char Fecha[12];
  char Hora[12];

  if (!getLocalTime(&timeinfo)) {
    msg = "Error al obtener Fecha/hora";
    return msg;
  }

  //printLocalTime();
  second = timeinfo.tm_sec;
  minute = timeinfo.tm_min;
  hour = timeinfo.tm_hour;
  day = timeinfo.tm_mday;
  month = timeinfo.tm_mon + 1;
  year = timeinfo.tm_year + 1900;


  //Serial.print("Fecha/hora: ");
  sprintf(Fecha, "%02d/%02d/%04d", day, month, year);
  sprintf(Hora, "%02d/%02d/%02d", hour, minute, second);

  msg = String(Fecha) + "  " + String(Hora);
  return msg;

}



void setup() {

  Serial.begin(115200);
  delay(500);


  /*
     Inicialización SD
  */

  // Initialize SD card
  SD.begin(SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.txt");
  if (!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.txt", "Fecha/hora, Evento \r\n");
  }
  else {
    Serial.println("File already exists");
  }
  file.close();

  /*
     Fin de inicialización SD
  */

  // Definimos los pines

  pinMode(Default_LED, OUTPUT);

  pinMode(ledConnect, OUTPUT);
  pinMode(ledStatus, OUTPUT);
  pinMode(ledAlarma, OUTPUT);

  pinMode(P_ENT, INPUT); // P_Entrada as INPUT
  pinMode(P_HAB, INPUT); // P_HABITACION as INPUT
  pinMode(P_DES, INPUT); // P_DESPACHO as INPUT



  //pinMode(ledCam, OUTPUT);

  digitalWrite(ledConnect, LOW); // Turn led Off
  digitalWrite(ledStatus, LOW); // Turn led Off
  digitalWrite(ledAlarma, LOW); // Turn led Off

  //digitalWrite(ledCam, LOW);

  delay(100);
  Serial.println("\nESP using Telegram Bot");
  String readings = getReadings();
  Serial.print(readings);

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Conectando a: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int sLed = HIGH;

  // ADDED This Update
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");

    digitalWrite(Default_LED, sLed);
    sLed = !sLed;

    delay(500);
  }

  //digitalWrite(ledCam, LOW);
  digitalWrite(Default_LED, HIGH);

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  digitalWrite(ledConnect, HIGH);
  Serial.println(" >> CONECTADA");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Print the Signal Strength:
  long rssi = WiFi.RSSI() + 100;
  Serial.print("Señal WiFi = " + String(rssi));

  if (rssi > 50) Serial.println(F(" (>50 – Good)")); else Serial.println(F(" (Could be Better)"));
  Serial.printf("Type ‘Start’ in Telegram to start bot\r\n");

  Sist_Activo = true;
  bot.sendMessage(chatId, "Sistema conectado y listo \r\n\n Escribe ‘Start’ o ‘S’ en Telegram para Información...\r\n", "");
  logSDCard("Sistema conectado y listo");

  //Activamos los sensores

  EnaP_ENT = 1;
  EnaP_HAB = 1;
  EnaP_DES = 1;

}

void loop() {


  // if WiFi is down, try reconnecting
  if (WiFi.status() != WL_CONNECTED) {
    int sLed = HIGH;
    digitalWrite(Default_LED, LOW);
    if ((WiFi.status() != WL_CONNECTED) and (currentMillis - previousMillis >= interval)) {
      Serial.print(millis());
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();


      digitalWrite(Default_LED, sLed);
      sLed = !sLed;

      previousMillis = currentMillis;
      logSDCard("Reconectando...");
    }
    else {
      bot.sendMessage(chatId, "Se perdió la conexión WiFi.\n Pero ha sido restablecida" , "");
      //ESP.restart();
      Sist_Activo = true;
      logSDCard("Sistema reconectado");
      digitalWrite(Default_LED, HIGH);
    }
  }

  // Leer sensores

  digitalWrite(ledStatus, Sist_Activo); // Turn led

  if (EnaP_ENT == 1) {
    sts_ENT = digitalRead(P_ENT);
  } else {
    sts_ENT = 1;
  }


  if (EnaP_HAB == 1) {
    sts_HAB = digitalRead(P_HAB);
  } else {
    sts_HAB = 1;
  }

  if (EnaP_DES == 1) {
    sts_DES = digitalRead(P_DES);
  } else {
    sts_DES = 1;
  }



  if (Sist_Activo) {

    //Leer estado de Entrada para activar el led de alarma
    if (sts_ENT == 0)  //si la Entrada se abre activar el led de alarma
    {
      digitalWrite(ledAlarma, HIGH);
      //digitalWrite(PinCamaraOut, HIGH);
      //Configurar notificacion a telegram
      if (msgEntradaSent == 0)
      {
        logSDCard("Puerta ENTRADA ABIERTA");
        bot.sendMessage(chatId, "ALERTA, Puerta ENTRADA ABIERTA", "");
        msgEntradaSent = 1;
      }
    }
    else
    { // Si la Entrada se cierra apagar el led de alarma

      if (msgEntradaSent == 1)
      {
        bot.sendMessage(chatId, "Puerta ENTRADA cerrada", "");
        logSDCard("Puerta ENTRADA cerrada");
        msgEntradaSent = 0;
        //digitalWrite(PinCamaraOut, LOW);
      }
      if (sts_HAB == 0 or sts_DES == 0) {
        digitalWrite(ledAlarma, HIGH);
      }
      else {
        digitalWrite(ledAlarma, LOW);
        //digitalWrite(PinCamaraOut, LOW);
      }
    }


    //Leer estado de Habitación para activar el led de alarma
    if (sts_HAB == 0) //si la Habitación se abre activar el led de alarma
    {
      digitalWrite(ledAlarma, HIGH);
      //digitalWrite(PinCamaraOut, HIGH);
      //Configurar notificacion a telegram
      if (msgHabitacionSent == 0)
      {
        logSDCard("Puerta HABITACIÓN ABIERTA");
        bot.sendMessage(chatId, "ALERTA, Puerta HABITACIÓN ABIERTA", "");
        msgHabitacionSent = 1;
      }
    }
    else
    { // Si la Habitación se cierra apagar el led de alarma
      if (msgHabitacionSent == 1)
      {
        logSDCard("Puerta HABITACIÓN cerrada");
        bot.sendMessage(chatId, "Puerta HABITACIÓN cerrada", "");
        msgHabitacionSent = 0;
        //digitalWrite(PinCamaraOut, LOW);
      }
      if (sts_ENT == 0 or sts_DES == 0) {
        digitalWrite(ledAlarma, HIGH);
      }
      else {
        digitalWrite(ledAlarma, LOW);
        //digitalWrite(PinCamaraOut, LOW);
      }
    }

    //Leer estado de Despacho para activar el led de alarma
    if (sts_DES == 0) //si el Despacho se abre activar el led de alarma
    {
      digitalWrite(ledAlarma, HIGH);
      //digitalWrite(PinCamaraOut, HIGH);
      //Configurar notificacion a telegram
      if (msgDespachoSent == 0)
      {
        logSDCard("Puerta DESPACHO ABIERTA");
        bot.sendMessage(chatId, "ALERTA, Puerta DESPACHO ABIERTA", "");
        msgDespachoSent = 1;
      }
    }
    else
    { // Si el Despacho se cierra apagar el led de alarma
      if (msgDespachoSent == 1)
      {
        logSDCard("Puerta DESPACHO cerrada");
        bot.sendMessage(chatId, "Puerta DESPACHO cerrada", "");
        msgDespachoSent = 0;
        //digitalWrite(PinCamaraOut, LOW);
      }
      if (sts_ENT == 0 or sts_HAB == 0) {
        digitalWrite(ledAlarma, HIGH);
      }
      else {
        digitalWrite(ledAlarma, LOW);
        //digitalWrite(PinCamaraOut, LOW);
      }
    }


  } else {
    sts_ENT = false;
    sts_HAB = false;
    sts_DES = false;

    msgEntradaSent = 0;
    msgHabitacionSent = 0;
    msgDespachoSent = 0;
  }


  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.print("Message received : ");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    String chat_user = String(bot.messages[i].from_name);
    if (chat_id != chatId) {
      bot.sendMessage(chat_id, "Usuario no autorizado \n", "");
      bot.sendMessage(chatId, "Intento de uso por " + chat_user + ", no autorizado. \r\n", "");

      String logMsg = "Intento de uso por " + chat_user + ", no autorizado.";
      logSDCard(logMsg.c_str());
      continue;
    }

    // Print message received
    String fromName = bot.messages[i].from_name;
    String text = bot.messages[i].text;

    Serial.println(numNewMessages + " From " + fromName + " >" + text + " request");

    //Activar
    if (text == "On" or text == "1") {
      String msg = "Sistema ";
      Sist_Activo = true; //!Sist_Activo;
      Serial.print("Sist_Activo = ");

      if (Sist_Activo == true) Serial.println("ON"); else Serial.println("OFF");
      if (Sist_Activo == true) msg += "Activado"; else msg += "Desactivado";

      logSDCard(msg.c_str());

      //msg += "\r\n";
      digitalWrite(ledStatus, Sist_Activo);
      bot.sendMessage(chatId, msg, "");
    }

    //Desactivar
    if (text == "Off" or text == "0") {
      String msg = "Sistema ";
      Sist_Activo = false; //!Sist_Activo;
      Serial.print("Sist_Activo = ");
      if (Sist_Activo == true) Serial.println("ON"); else Serial.println("OFF");
      if (Sist_Activo == true) msg += "Activado"; else msg += "Desactivado";

      digitalWrite(ledStatus, Sist_Activo);
      digitalWrite(ledAlarma, LOW);

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }

    // Sensores
    if (text == "S1 on" or text == "S1 1" or text == "Ent on" or text == "Ent 1" or text == "S on" or text == "S 1") {
      String msg = "Sensor 1 Activo";
      EnaP_ENT = 1;
      pinMode(P_ENT, INPUT);

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }

    if (text == "S1 off" or text == "S1 0" or text == "Ent off" or text == "Ent 0" or text == "S off" or text == "S 0") {
      String msg = "Sensor 1 Deshabilitado";
      EnaP_ENT = 0;
      sts_ENT = false;
      msgEntradaSent = 0;

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }

    if (text == "S2 on" or text == "S2 1" or text == "Hab on" or text == "Hab 1" or text == "S on" or text == "S 1") {
      String msg = "Sensor 2 Activo";
      EnaP_HAB = 1;
      pinMode(P_HAB, INPUT);

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }

    if (text == "S2 off" or text == "S2 0" or text == "Hab off" or text == "Hab 0" or text == "S off" or text == "S 0") {
      String msg = "Sensor 2 Deshabilitado";
      EnaP_HAB = 0;
      sts_HAB = false;
      msgHabitacionSent = 0;

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }

    if (text == "S3 on" or text == "S3 1" or text == "Des on" or text == "Des 1" or text == "S on" or text == "S 1") {
      String msg = "Sensor 3 Activo";
      EnaP_DES = 1;
      pinMode(P_DES, INPUT);

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }
    if (text == "S3 off" or text == "S3 0" or text == "Des off" or text == "Des 0" or text == "S off" or text == "S 0") {
      String msg = "Sensor 3 Deshabilitado";
      EnaP_DES = 0;
      sts_DES = false;
      msgDespachoSent = 0;

      logSDCard(msg.c_str());

      bot.sendMessage(chatId, msg, "");
    }

    // Info
    if (text == "Info" or text == "info" or text == "I" or text == "i" or text == "5") {
      String readings = getReadings();
      logSDCard("Solicitada INFO");
      bot.sendMessage(chatId, readings, "");
    }

    // Inicio
    if (text == "/Start" or text == "/start" or text == "Start" or text == "start" or text == "/S" or text == "/s") {
      String welcome = "Bienvenido al Sistema ESP32.\r\n";
      long rssi = WiFi.RSSI() + 100;
      welcome += "Señal WiFi: " + String(rssi) + "\n";
      if (rssi > 50) welcome += (F(" (>50 – Buena)\n")); else welcome  += (F(" (Podría ser mejor)\n" ));
      welcome += "\n";
      welcome += "On (1) : Inicia el Sistema\n";
      welcome += "Off  (0): Detiene el Sistema\n";
      welcome += "Info (I) (5): Información Sistema \n\n";
      if (Sist_Activo == true) welcome += "Estado : Activado\n"; else welcome += "Estado : Desactivado\r\n";

      //message += "*-> Sensor 1 ";
      if (EnaP_ENT == true) welcome += "*-> Sensor 1 Activo\n"; else welcome += "*-> Sensor 1 Deshabilitado\n";

      //message += "*-> Sensor 2 ";
      if (EnaP_HAB == true) welcome += "*-> Sensor 2 Activo\n"; else welcome += "*-> Sensor 2 Deshabilitado\n";

      // message += "*-> Sensor 3 ";
      if (EnaP_DES == true) welcome += "*-> Sensor 3 Activo\n"; else welcome += "*-> Sensor 3 Deshabilitado\n";

      logSDCard("Seleccionado 'Start'");
      bot.sendMessage(chatId, welcome, "Markdown");
    }
  }
}


// Escribir los eventos
void logSDCard(const char * evento) {

  String dataMessage = String(ObtieneFechahora()) + ", " + evento + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());

}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
