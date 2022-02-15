#include <DHT.h>

#include <OneWire.h>
#include <DallasTemperature.h>


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include "ArduinoJson.h"

const char *ssid = "SSID_NAME";
const char *password = "SSID_PASSWORD";

const char *jsonContent = "application/json";

const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/css/bootstrap.min.css" rel="stylesheet" integrity="sha384-EVSTQN3/azprG1Anm3QDgpJLIm9Nao0Yz1ztcQTwFspd3yD65VohhpuuCOmLASjC" crossorigin="anonymous" />
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/js/bootstrap.bundle.min.js" integrity="sha384-MrcW6ZMFYlzcLA8Nl+NtUVF0sA7MsXsP1UyJoMp4YLEuNSfAP+JcXn/tWtIaxVXM" crossorigin="anonymous"></script>

    <script type="text/javascript">
      window.onload = function () {
        var dpsDHT = [];
        var dpsDS18B20 = [];
        var dpsUmid = [];
        var dpsLDR = [];
        var decorredTime = 0;

        var chartLDR = new CanvasJS.Chart("chartContainer", {
          theme:"light2",
          exportEnabled: true,
          title: {
            text: "Sensor Fotoresistor (LDR)",
          },
          data: [
            {
              type: "spline",
              markerSize: 0,
              dataPoints: dpsLDR,
            },
          ],
        });
        chartLDR.render();

        var chartUmid = new CanvasJS.Chart("chartContainerUmid", {
          exportEnabled: true,
          animationEnabled: true,
          title: {
            text: "Leitura de Umidade",
            horizontalAlign: "left",
          },
          data: [
            {
              type: "doughnut",
              startAngle: 60,
              //innerRadius: 60,
              indexLabelFontSize: 17,
              indexLabel: "{label} #percent%",
              toolTipContent: "<b>{label}:</b> {y} (#percent%)",
              dataPoints: dpsUmid,
            },
          ],
        });
        chartUmid.render();

        var chartTemperatures = new CanvasJS.Chart("chartContainerTemp", {
          exportEnabled: true,
          theme: "light2",
          animationEnabled: true,
          title: {
            text: "Medição de Temperatura dos sensores",
          },
          axisY: {
            title: "Temperatura",
            suffix: "*C",
          },
          toolTip: {
            shared: "true",
          },
          legend: {
            cursor: "pointer",
            itemclick: toggleDataSeries,
          },
          data: [
            {
              type: "spline",
              visible: true,
              markerSize: 5,
              showInLegend: true,
              yValueFormatString: "##.00 *C",
              name: "Sensor (DHT11)",
              dataPoints: dpsDHT,
            },
            {
              type: "spline",
              visible: true,
              markerSize: 5,
              showInLegend: true,
              yValueFormatString: "##.00 *C",
              name: "Sensor (DS18B20)",
              dataPoints: dpsDS18B20,
            },
          ],
        });

        chartTemperatures.render();

        function toggleDataSeries(e) {
          if (typeof e.dataSeries.visible === "undefined" || e.dataSeries.visible) {
            e.dataSeries.visible = false;
          } else {
            e.dataSeries.visible = true;
          }
          chartTemperatures.render();
        }

        var dateTime = new Date();
        var updateInterval = 1000;
        var dataLength = 50;
        var updateChart = async function (count) {
          let response = await fetch("/api");

          if (response.ok) {
            let json = await response.json();
            let dht = json["DHT"];
            let ldr = json["LDR"];
            let ds18b20 = json["DS18B20"];

            dateTime = new Date();

            dpsDHT.push({
              x: dateTime,
              y: dht.Temp,
            });
            dpsDS18B20.push({
              x: dateTime,
              y: ds18b20.Temp,
            });
            dpsLDR.push({
              x: dateTime,
              y: ldr.ldr,
            });

            if (dpsDS18B20.length > dataLength) dpsDS18B20.shift();
            if (dpsDHT.length > dataLength) dpsDHT.shift();
            if (dpsLDR.length > dataLength) dpsLDR.shift();
            dpsUmid.shift();
            dpsUmid.shift();
            dpsUmid.push({
              label: "DHT11",
              y: dht.Umid,
            });
            dpsUmid.push({
              label: "",
              y: 100 - dht.Umid,
            });

            chartUmid.render();
            chartTemperatures.render();
            chartLDR.render();
            console.log(json);
          } else {
            console.log("HTTP-Error: " + response.status);
          }
        };
        updateChart(dataLength);
        setInterval(function () {
          updateChart();
        }, updateInterval);
      };
    </script>
    <script type="text/javascript" src="https://canvasjs.com/assets/script/canvasjs.min.js"></script>
  </head>
  <body class="m-2 p-2">
    <div class="container card p-2">
      <div class="row">
        <div class="col-7">
          <div class="container card p-2">
            <div id="chartContainerTemp" style="height: 300px; width: 100%;"></div>
          </div>
        </div>
        <div class="col p-2">
          <div id="chartContainerUmid" style="height: 300px; width: 100%;"></div>
        </div>
      </div>
    </div>
    <div class="container card p-2">
      <div id="chartContainer" style="height: 250px; width: 100%;"></div>
    </div>
  </body>
</html>
)rawliteral";


ESP8266WebServer server(80);

const int pinLDR = A0;
const int pinDHT = 12; // D6 ou GPIO12
const int pinDS18B20 = 14; // D5 ou GPIO14

int ldrVal = 0;
float tempAr = 0.0; 
float umidAr = 0.0; 
float tempds18b20 = 0.0;

DHT dht11(pinDHT, DHT11);

OneWire oneWire(pinDS18B20);
DallasTemperature ds18b20(&oneWire);

void dhtGetValues(){
  tempAr = dht11.readTemperature(); 
  umidAr = dht11.readHumidity(); 
  if (!isnan(umidAr) && !isnan(tempAr)) {
    Serial.print("Temperatura: "); 
    Serial.print(tempAr);
    Serial.print(" *C || Umidade: "); 
    Serial.print(umidAr);
    Serial.print(" %");
    Serial.println();
  }
  else {
      tempAr = 0.0; 
      umidAr = 0.0; 
  }
  delay(50);
}

void ldrGetValues(){
  ldrVal = analogRead(pinLDR);
  Serial.print("LDR: "); 
  Serial.print(ldrVal);
  Serial.println();
  delay(50);
}

void ds18b20GetValues(){
  ds18b20.requestTemperatures();
  tempds18b20 = ds18b20.getTempCByIndex(0);
  Serial.print("Temperatura: ");
  Serial.print(tempds18b20);
  Serial.print("*C.");
  Serial.println();
  delay(50);
}

void handleIndex() {
  server.send_P(200, "text/html", html);
}

void handleApi() {

  String response;
  
  response = "{";
  response += "\"LDR\": {\"ldr\": " + String(ldrVal) + "},";
  response += "\"DHT\": {\"Temp\": " + String(tempAr) + ", \"Umid\": " + String(umidAr) + "},";
  response += "\"DS18B20\": {\"Temp\": " + String(tempds18b20) + "}";
  response += "}";

  server.send(200, jsonContent, response);
}



void setup() {
  Serial.begin(9600);

  dht11.begin();
  ds18b20.begin();
  pinMode(pinLDR, INPUT);

  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleIndex);

  server.on("/api", handleApi);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  dhtGetValues();
  ldrGetValues();
  ds18b20GetValues();
  server.handleClient();
}
