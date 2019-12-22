/* RTC Libraries */
#include <Wire.h>
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire); //object rtc

/* ESP Libraries */
#include <ESP.h>
int sensor = ESP.getChipId(); //id do Esp dentro do inteiro sensor

/* Thread libraries */
#include <Thread.h>
#include <ThreadController.h>
Thread envia = Thread();
Thread wifi = Thread();
Thread coleta = Thread();
ThreadController control = ThreadController();

/* Wifi manager */
#include <WiFiManager.h>
String host = "www.adlim.com.br";

/* Ultrasonic */
#include <Ultrasonic.h>
#define trigger D5
#define echo D7
Ultrasonic ultrasonic(trigger, echo); //Object declaration

/* Flash Memory ESP */
#include <FS.h>

/* Variáveis */
String  msg, buf; //enviar dados de data e hora para memória interna (msg)->data+hora e (buf)->salvar no arquivo da memória interna
int previousState = 0;
int maximo = 0;
int atual = 0;
int previous = 0;
int timeout = 0;
int cont = 0;

void setup () {
  Serial.begin(115200);

  /* RTC CODE RUN ONCE ARDUINO(ON) //Pega a data compilada e seta o rtc*/
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) {
    if (Rtc.LastError() != 0) {
      // we have a communications error
      // see https://www.arduino.cc/en/Reference/WireEndTransmission for
      // what the number means
      Serial.print("RTC communications error = ");
      Serial.println(Rtc.LastError());
    } else {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      Serial.println("RTC lost confidence in the DateTime!");

      // following line sets the RTC to the date & time this sketch was compiled
      // it will also reset the valid flag internally unless the Rtc device is
      // having an issue

      Rtc.SetDateTime(compiled);
    }
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();

  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }  else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);

  /*SPIFFS CODE RUN ONCE ARDUINO(ON)*/
  //Abre o sistema de arquivos (mount)
  openFS();
  //Cria o arquivo caso o mesmo não exista
  createFile();

  /* WIFI MANAGER CODE RUN ONCE ARDUINO(ON) */
  WiFiManager wifiManager;
  wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setTimeout(20);
  wifiManager.autoConnect("ADLIMNOW", "wifipass");

  /* THREAD CODE RUN ONCE ARDUINO(ON)*/
  wifi.onRun(wifiConnect);
  envia.onRun(enviaDados);
  envia.setInterval(60000);
  coleta.onRun(coletaLoop);
  control.add(&envia);
  control.add(&wifi);
  control.add(&coleta);

  /* ULTRASONIC CODE RUN ONCE ARDUINO(ON)*/
  delay(500);
  maximo = ultrasonic.read();
  delay(1000);
  previousState = ultrasonic.read();
  delay(10);
}

void loop () {
  control.run();
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u-%02u-%02u%02u:%02u:%02u"),
             dt.Year(),
             dt.Month(),
             dt.Day(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  String msg = (datestring + String(sensor));
  writeFile(msg);
}

void wifiConnect(){
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Tentando Conectar");
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.setConfigPortalTimeout(120);
    //wifiManager.setTimeout(20);
    wifiManager.autoConnect("ADLIMNOW", "wifipass");
    timeout++;
    if (timeout >= 3) {
      timeout = 0;
      ESP.restart();
    }
  }
}

/* --------------- Thread Code -------------------------------------------- */
void enviaDados() {
  //  Faz a leitura do arquivo
  File rFile = SPIFFS.open("/log.txt", "r");
  Serial.println("Reading file...");
  while (rFile.available()) {
    WiFiClient client;

    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }

    String line = rFile.readStringUntil('\n');
    line.trim();
    client.print(String("GET ") + "/nodemcu/salvar.php?sensor=" + line + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  }
  SPIFFS.format();
  rFile.close();
}

void coletaLoop() {
 // int temp = 0;

  /* Media */
//  for (int i = 0; i < 10; i++) {
//    temp = ultrasonic.read();
//    delay(10);
//    realTime = realTime + (temp / 10);
//    delay(15);
//  }
  int realTime = 0;
  
  /* Mediana */
  int v[10]; //float
  int i=0;
  int j=0;
  int aux=0; //float
  int tamanho = 0;
 
  for (i = 0; i < 10; i++) {
    v[i] = ultrasonic.read();
    delay(8);
  }

  tamanho = sizeof(v) / sizeof(float);

  for(i=0;i<tamanho-1;i++){
        for(j=i+1;j<tamanho;j++){
            if(v[i] > v[j]){
                aux = v[i];
                v[i] = v[j];
                v[j] = aux;
            }
        }
    }
   realTime = (v[tamanho/2-1]+v[tamanho/2])/2;
   Serial.print("maximo: ");
   Serial.println(maximo);
   Serial.print(" - real: ");
   Serial.println(realTime);
//   Serial.print(" - tempo: ");
//   Serial.println( now() );
  
//  Serial.print("Máximo: ");
//  Serial.print(maximo);
//  Serial.print(" Real: ");
//  Serial.println(realTime);

  if ((realTime >= maximo - 25) && (realTime <= maximo + 20)) {
    previous = atual;
    atual = 0;
  } else if (realTime > maximo + 20) {
    atual = 2;
  } else {
    previous = atual;
    atual = 1;
  }

  if (previous == 1 && atual == 0) {
    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);
  }
}

/*-------- SPIFFS code ----------*/
void formatFS(void) {
  SPIFFS.format();
}

void createFile(void) {
  File wFile;

  //Cria o arquivo se ele não existir
  if (SPIFFS.exists("/log.txt")) {
    Serial.println("Arquivo ja existe!");
  } else {
    Serial.println("Criando o arquivo...");
    wFile = SPIFFS.open("/log.txt", "w+");

    //Verifica a criação do arquivo
    if (!wFile) {
      Serial.println("Erro ao criar arquivo!");
    } else {
      Serial.println("Arquivo criado com sucesso!");
    }
  }
  wFile.close();
}

void deleteFile(void) {
  //Remove o arquivo
  if (SPIFFS.remove("/log.txt")) {
    Serial.println("Erro ao remover arquivo!");
  } else {
    Serial.println("Arquivo removido com sucesso!");
  }
}

void writeFile(String msg) {
  //Abre o arquivo para adição (append)
  //Inclue sempre a escrita na ultima linha do arquivo
  File rFile = SPIFFS.open("/log.txt", "a+");

  if (!rFile) {
    Serial.println("Erro ao abrir arquivo!");
  } else {
    rFile.println(msg);
    Serial.println(msg);
  }
  rFile.close();
  delay(10);
}

void readFile(void) {
  //Faz a leitura do arquivo
  File rFile = SPIFFS.open("/log.txt", "r");
  Serial.println("Reading file...");
  while (rFile.available()) {
    String line = rFile.readStringUntil('\n');
    buf += line;
    buf += "<br />";
  }
  rFile.close();
}

void closeFS(void) {
  SPIFFS.end();
}

void openFS(void) {
  //Abre o sistema de arquivos
  if (!SPIFFS.begin()) {
    Serial.println("Erro ao abrir o sistema de arquivos");
  } else {
    Serial.println("Sistema de arquivos aberto com sucesso!");
  }
}
