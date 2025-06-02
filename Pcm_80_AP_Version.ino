#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "main.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

//per modalità AP
const char* ap_ssid = "LexiconPCM8x-9x_OLED";
const char* ap_password = "";  // opzionale


WebServer server(80);
// Pagina HTML con barra di avanzamento
const char* loginIndex = R"rawliteral(
<!DOCTYPE html>
<meta charset="utf-8">
<html>
  <head>
    <title>Lexicon Pcm8x/9x Oled</title>
    <style>
      body { font-family: sans-serif; text-align: center; margin-top: 50px; }
      progress { width: 80%; height: 20px; }
    </style>
  </head>
  <body>
    <h1>Lexicon PCM8x/9x OLED Upgrade</h1>
    <form method="POST" action="/update" enctype="multipart/form-data" id="upload_form">
      <input type="file" name="update">
      <input type="submit" value="Carica">
    </form>
    <br>
    <progress id="progressBar" value="0" max="100"></progress>
    <p id="status"></p>
    <script>
      const form = document.getElementById('upload_form');
      const progress = document.getElementById('progressBar');
      const status = document.getElementById('status');

      form.addEventListener('submit', function (e) {
        e.preventDefault();
        const file = form.querySelector('input[type="file"]').files[0];
        const xhr = new XMLHttpRequest();
        xhr.upload.addEventListener("progress", function(evt) {
          if (evt.lengthComputable) {
            const percent = (evt.loaded / evt.total) * 100;
            progress.value = percent;
            status.innerText = `Caricamento: ${percent.toFixed(1)}%`;
          }
        }, false);
        xhr.open('POST', '/update', true);
        xhr.onload = function () {
          if (xhr.status === 200) {
            status.innerText = "Aggiornamento completato. Riavvio display in corso...";
          } else {
            status.innerText = "Errore durante l'update.";
          }
        };
        const formData = new FormData();
        formData.append("update", file);
        xhr.send(formData);
      });
    </script>
  </body>
</html>
)rawliteral";

U8G2_SSD1322_NHD_256X64_F_4W_SW_SPI u8g2(
  U8G2_R2,
  /* clock=*/22,
  /* data=*/21,
  /* cs=*/23,
  /* dc=*/19,
  /* reset=*/18);

//simbolo assenza Wifi
void drawNoWifi(int x, int y) {
  // Archi del segnale
  u8g2.drawCircle(x + 6, y, 6, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  u8g2.drawCircle(x + 6, y, 4, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  u8g2.drawCircle(x + 6, y, 2, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  u8g2.drawDisc(x + 6, y, 1); //punto
  u8g2.drawLine(x + 0, y - 8, x + 12, y + 4);  // diagonale (tipo sbarrato)
  u8g2.sendBuffer();
}

//simbolo Wifi
void drawWiFiIcon(int x, int y) {
  /*disegna tre archi*/
  u8g2.drawCircle(x, y, 6, U8G2_DRAW_UPPER_RIGHT);  
  u8g2.drawCircle(x, y, 4, U8G2_DRAW_UPPER_RIGHT);  
  u8g2.drawCircle(x, y, 2, U8G2_DRAW_UPPER_RIGHT);  
  u8g2.drawDisc(x, y, 1);  //*punto
  u8g2.sendBuffer();
}


void setup() {
  u8g2.setFont(u8g2_font_courR12_tf);

  u8g2.setFont(u8g2_font_8x13_tf);

  for (int i = 0; i < 8; i++) {
    pinMode(databus[i], INPUT);
  }
  for (int i = 0; i < 40; i++) {
    character[i] = 32;  // clear char buffer
  }



  pinMode(DISPDWR, INPUT);   //  genera Interrupt quando riceve un nuovo dato DISPDWR (dispositivo pin DataWrite)
  pinMode(DISPBSY, OUTPUT);  //  genera un segnale busy verso il dispositivo
  attachInterrupt(digitalPinToInterrupt(DISPDWR), NewDataInterrupt, FALLING);

  u8g2.begin();
  u8g2.setContrast(255);

  u8g2.clearBuffer();  // Pulisce il buffer

  u8g2.drawStr(0, 24, "Lexicon Display On...");  // Testo alla posizione (0,24)
  u8g2.sendBuffer();

  //per configurazione AP
  bool ap = WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();

  if (ap) {
    drawWiFiIcon(0, 8);  // posizione angolo in alto a destra
  } else {
    drawNoWifi(0, 8);
  }

  // Web UI base
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", loginIndex);
  });


  // Gestione upload OTA
  server.on(
    "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "Aggiornamento fallito" : "Aggiornamento riuscito. Riavvio...");
      delay(1000);
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        u8g2.clearDisplay();
        u8g2.drawStr(0, 24, "Upload in progress...");
        u8g2.drawStr(0, 40, "Please, do not turn off. ");
        u8g2.sendBuffer();


        if (!Update.begin()) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          u8g2.clearDisplay();
          u8g2.drawStr(0, 24, "Restart...");
          u8g2.sendBuffer();
        } else
          Update.printError(Serial);
      }
    });

  server.begin();
}

void loop() {
  server.handleClient();
  delay(10);
  u8g2.sendBuffer();
}


//void ParseData(void* parameter) {
void ParseData() {

  switch (data) {

    case 0x10:  // Display position
      mode = data;
      break;

    case 0x0f:  // all display
      /*
      u8g2.clearBuffer();
      u8g2.drawStr(9, 24, "PCM80 Display OLED");
      u8g2.drawStr(9, 46, "Mattia Bonfanti");
      */
      break;

    case 0x1f:  // reset
      u8g2.clearBuffer();
      position = 0;
      break;

    default:  // write char
      if (position < 20) {
        char tmp[3];
        sprintf(tmp, "%c", data);
        u8g2.setDrawColor(0);
        u8g2.drawBox(0 + 12 * position, 12, 12, 16);
        u8g2.setDrawColor(1);
        if (data == 159) {
          u8g2.drawBox(3 + 12 * position, 16, 4, 4);
        } else {
          u8g2.drawStr(3 + 12 * position, 24, tmp);
        }

      } else {
        char temp[3];
        sprintf(temp, "%c", data);
        u8g2.setDrawColor(0);
        u8g2.drawBox(12 * (position - 20), 40, 12, 16);
        u8g2.setDrawColor(1);
        if (data == 159) {
          u8g2.drawBox(3 + 12 * (position - 20), 48, 4, 4);
        } else {
          u8g2.drawStr(3 + 12 * (position - 20), 52, temp);
        }
      }
      position++;
      if (position == 40) {
        position = 0;
      }
      break;
  }
  digitalWrite(DISPBSY, 0);
  //vTaskDelete(NULL);
}

void IRAM_ATTR NewDataInterrupt() {
  digitalWrite(DISPBSY, 1);  // quando riceve l'interrupt, il segnale busy viene messo a livello alto
  uint8_t tmp = 0;
  data = 0;
  for (int i = 0; i < 8; i++) {
    tmp = digitalRead(databus[i]);
    data += (tmp << i);
  }

  switch (mode) {
    case 0x10:  // Display position
      position = data;
      mode = 0;
      digitalWrite(DISPBSY, 0);
      break;
    default:  // Parse data
      //xTaskCreate(ParseData, "Parse_Task", 10000, NULL, 1, NULL);
      ParseData();
      break;
  }
}
