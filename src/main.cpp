#include <Arduino.h>

#include <FastLED.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <WifiUdp.h>
#include "HueApi.h"
#include <MQTT.h>

// DEFINES
#define LED_PIN     2
#define NUM_LEDS    14
#define BRIGHTNESS  64
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define onPin 4 // on and brightness up
#define offPin 5 // off and brightness down




// END DEFINES


String HUE_Name = "Hue ESP32";   //max 32 characters!!!
int HUE_LightsCount = 1;                     //number of emulated hue-lights
int HUE_PixelPerLight = NUM_LEDS;                  //number of leds forming one emulated hue-light
int HUE_TransitionLeds = 1;                 //number of 'space'-leds inbetween the emulated hue-lights; pixelCount must be divisible by this value
String HUE_UserName = "hUE";     //hue-username (needs to be configured in the diyHue-bridge
int HUE_FirstHueLightNr = 20;                //first free number for the first hue-light (look in diyHue config.json)
int HUE_ColorCorrectionRGB[3] = {100, 100, 100};  // light multiplier in percentage /R, G, B/
uint8_t scene;
uint8_t startup;
bool inTransition;
bool useDhcp = true;
bool hwSwitch = false;

// MQTTClient MQTT(1024);
CRGBArray<NUM_LEDS> leds;
WebServer websrv(80);
IPAddress address ( 192,  168,   0,  95);     // choose an unique IP Adress
IPAddress gateway ( 192,  168,   0,   1);     // Router IP
IPAddress submask(255, 255, 255,   0);
byte mac[6]; // to hold  the wifi mac address
WiFiClient net;
HueApi objHue = HueApi(leds, mac, HUE_Name, HUE_LightsCount, HUE_PixelPerLight, HUE_TransitionLeds, HUE_UserName, HUE_FirstHueLightNr);
void infoLight(CRGB color) {

  // Flash the strip in the selected color. White = booted, green = WLAN connected, red = WLAN could not connect
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
    FastLED.show();
    // leds.fadeToBlackBy(10);
  }
  // leds = CRGB(CRGB::Black);
  FastLED.show();
}


void Log(String msg) {
  Serial.println(msg);

  // if (MQTT.connected()) {
  //   MQTT.publish("iot/ledcontroller/log", msg);
  // }
}

// void ConnectMQTT(){

//   Log("Connecting to MQTT-Broker: ");

//   MQTT.begin("192.168.0.65", 1883, net);
//   MQTT.setWill("iot/ledcontroller/log", "Off");
  
//   while (!MQTT.connect("ledcontroller", "", "")) {
//     Serial.print(".");
//     delay(500);
//   }

// //  MQTT.onMessage(messageReceived);

//   MQTT.subscribe("iot/ledcontroller/log");
// /*  MQTT.subscribe(Conf.getString("ctrl.cmdtopic"));
//   MQTT.subscribe(Conf.getString("ctrl.pcttopic"));
//   MQTT.subscribe(Conf.getString("ctrl.rgbtopic"));
//   MQTT.subscribe(Conf.getString("ctrl.scenetopic"));
//   MQTT.subscribe(Conf.getString("ctrl.cfgtopic"));
// */
//   Log("MQTT connected.\r\n");
// }


String WebLog(String message) {
  
  message += "URI: " + websrv.uri();
  message += "\r\n Method: " + (websrv.method() == HTTP_GET) ? "GET" : "POST";
  message += "\r\n Arguments: " + websrv.args(); + "\r\n";
  for (uint8_t i = 0; i < websrv.args(); i++) {
    message += " " + websrv.argName(i) + ": " + websrv.arg(i) + " \r\n";
  }
  
  Log(message);
  return message;
}

void websrvDetect() {
  Log("In Detect!!");
  String output = objHue.Detect();
  websrv.send(200, "text/plain", output);
  Log(output);
}

void websrvStateGet() {
  String output = objHue.StateGet(websrv.arg("light"));
  websrv.send(200, "text/plain", output);
}

void websrvStatePut() {
  Log("In State put!!" + websrv.arg("plain"));

  String output = objHue.StatePut(websrv.arg("plain"));
  if (output.substring(0, 4) == "FAIL") {
    websrv.send(404, "text/plain", "FAIL. " + websrv.arg("plain"));
  }
  websrv.send(200, "text/plain", output);
}

void websrvReset() {
  websrv.send(200, "text/html", "reset");
  delay(1000);
  esp_restart();
}

void websrvConfig() {

  DynamicJsonDocument root(1024);
  String output;
  HueApi::state tmpState;

  root["name"] = HUE_Name;
  root["scene"] = scene;
  root["startup"] = startup;
  root["hw"] = hwSwitch;
  root["on"] = onPin;
  root["off"] = offPin;
  root["hwswitch"] = (int)hwSwitch;
  root["lightscount"] = HUE_LightsCount;
  root["pixelcount"] = HUE_PixelPerLight;
  root["transitionleds"] = HUE_TransitionLeds;
  root["rpct"] = RGB_R;
  root["gpct"] = RGB_G;
  root["bpct"] = RGB_B;
  root["disdhcp"] = (int)!useDhcp;
  root["addr"] = (String)address[0] + "." + (String)address[1] + "." + (String)address[2] + "." + (String)address[3];
  root["gw"] = (String)gateway[0] + "." + (String)gateway[1] + "." + (String)gateway[2] + "." + (String)gateway[3];
  root["sm"] = (String)submask[0] + "." + (String)submask[1] + "." + (String)submask[2] + "." + (String)submask[3];

  serializeJson(root, output);
  websrv.send(200, "text/plain", output);
}


void websrvRoot() {

  Log("StateRoot: " + websrv.uri());

  if (websrv.arg("section").toInt() == 1) {
    HUE_Name = websrv.arg("name");
    startup = websrv.arg("startup").toInt();
    scene = websrv.arg("scene").toInt();
    HUE_LightsCount = websrv.arg("lightscount").toInt();
    HUE_PixelPerLight = websrv.arg("pixelcount").toInt();
    HUE_TransitionLeds = websrv.arg("transitionleds").toInt();
    HUE_ColorCorrectionRGB[0] = websrv.arg("rpct").toInt();
    HUE_ColorCorrectionRGB[1] = websrv.arg("gpct").toInt();
    HUE_ColorCorrectionRGB[2] = websrv.arg("bpct").toInt();
    hwSwitch = websrv.hasArg("hwswitch") ? websrv.arg("hwswitch").toInt() : 0;

    // saveConfig();
  } else if (websrv.arg("section").toInt() == 2) {
    useDhcp = (!websrv.hasArg("disdhcp")) ? 1 : websrv.arg("disdhcp").toInt();
    if (websrv.hasArg("disdhcp")) {
      address.fromString(websrv.arg("addr"));
      gateway.fromString(websrv.arg("gw"));
      submask.fromString(websrv.arg("sm"));
    }
    // saveConfig();
  }

  String htmlContent = "<!DOCTYPE html> <html> <head> <meta charset=\"UTF-8\"> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> <title>Hue Light</title> <link href=\"https://fonts.googleapis.com/icon?family=Material+Icons\" rel=\"stylesheet\"> <link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/css/materialize.min.css\"> <link rel=\"stylesheet\" href=\"https://cerny.in/nouislider.css\"/> </head> <body> <div class=\"wrapper\"> <nav class=\"nav-extended row deep-purple\"> <div class=\"nav-wrapper col s12\"> <a href=\"#\" class=\"brand-logo\">DiyHue</a> <ul id=\"nav-mobile\" class=\"right hide-on-med-and-down\" style=\"position: relative;z-index: 10;\"> <li><a target=\"_blank\" href=\"https://github.com/diyhue\"><i class=\"material-icons left\">language</i>GitHub</a></li> <li><a target=\"_blank\" href=\"https://diyhue.readthedocs.io/en/latest/\"><i class=\"material-icons left\">description</i>Documentation</a></li> <li><a target=\"_blank\" href=\"https://diyhue.slack.com/\" ><i class=\"material-icons left\">question_answer</i>Slack channel</a></li> </ul> </div> <div class=\"nav-content\"> <ul class=\"tabs tabs-transparent\"> <li class=\"tab\"><a class=\"active\" href=\"#test1\">Home</a></li> <li class=\"tab\"><a href=\"#test2\">Preferences</a></li> <li class=\"tab\"><a href=\"#test3\">Network settings</a></li> </ul> </div> </nav> <ul class=\"sidenav\" id=\"mobile-demo\"> <li><a target=\"_blank\" href=\"https://github.com/diyhue\">GitHub</a></li> <li><a target=\"_blank\" href=\"https://diyhue.readthedocs.io/en/latest/\">Documentation</a></li> <li><a target=\"_blank\" href=\"https://diyhue.slack.com/\" >Slack channel</a></li> </ul> <div class=\"container\"> <div class=\"section\"> <div id=\"test1\" class=\"col s12\"> <form> <input type=\"hidden\" name=\"section\" value=\"1\"> <div class=\"row\"> <div class=\"col s10\"> <label for=\"power\">Power</label> <div id=\"power\" class=\"switch section\"> <label> Off <input type=\"checkbox\" name=\"pow\" id=\"pow\" value=\"1\"> <span class=\"lever\"></span> On </label> </div> </div> </div> <div class=\"row\"> <div class=\"col s12 m10\"> <label for=\"bri\">Brightness</label> <input type=\"text\" id=\"bri\" class=\"js-range-slider\" name=\"bri\" value=\"\"/> </div> </div> <div class=\"row\"> <div class=\"col s12\"> <label for=\"hue\">Color</label> <div> <canvas id=\"hue\" width=\"320px\" height=\"320px\" style=\"border:1px solid #d3d3d3;\"></canvas> </div> </div> </div> <div class=\"row\"> <div class=\"col s12\"> <label for=\"ct\">Color Temp</label> <div> <canvas id=\"ct\" width=\"320px\" height=\"50px\" style=\"border:1px solid #d3d3d3;\"></canvas> </div> </div> </div> </form> </div> <div id=\"test2\" class=\"col s12\"> <form method=\"POST\" action=\"/\"> <input type=\"hidden\" name=\"section\" value=\"1\"> <div class=\"row\"> <div class=\"col s12\"> <label for=\"name\">Light Name</label> <input type=\"text\" id=\"name\" name=\"name\"> </div> </div> <div class=\"row\"> <div class=\"col s12 m6\"> <label for=\"startup\">Default Power:</label> <select name=\"startup\" id=\"startup\"> <option value=\"0\">Last State</option> <option value=\"1\">On</option> <option value=\"2\">Off</option> </select> </div> </div> <div class=\"row\"> <div class=\"col s12 m6\"> <label for=\"scene\">Default Scene:</label> <select name=\"scene\" id=\"scene\"> <option value=\"0\">Relax</option> <option value=\"1\">Read</option> <option value=\"2\">Concentrate</option> <option value=\"3\">Energize</option> <option value=\"4\">Bright</option> <option value=\"5\">Dimmed</option> <option value=\"6\">Nightlight</option> <option value=\"7\">Savanna sunset</option> <option value=\"8\">Tropical twilight</option> <option value=\"9\">Arctic aurora</option> <option value=\"10\">Spring blossom</option> </select> </div> </div> <div class=\"row\"> <div class=\"col s4 m3\"> <label for=\"pixelcount\" class=\"col-form-label\">Pixel count</label> <input type=\"number\" id=\"pixelcount\" name=\"pixelcount\"> </div> </div> <div class=\"row\"> <div class=\"col s4 m3\"> <label for=\"lightscount\" class=\"col-form-label\">Lights count</label> <input type=\"number\" id=\"lightscount\" name=\"lightscount\"> </div> </div> <div class=\"row\"> <div class=\"col s4 m3\"> <label for=\"transitionleds\">Transition leds:</label> <select name=\"transitionleds\" id=\"transitionleds\"> <option value=\"0\">0</option> <option value=\"2\">2</option> <option value=\"4\">4</option> <option value=\"6\">6</option> <option value=\"8\">8</option> <option value=\"10\">10</option> </select> </div> </div> <div class=\"row\"> <div class=\"col s4 m3\"> <label for=\"rpct\" class=\"form-label\">Red multiplier</label> <input type=\"number\" id=\"rpct\" class=\"js-range-slider\" data-skin=\"round\" name=\"rpct\" value=\"\"/> </div> <div class=\"col s4 m3\"> <label for=\"gpct\" class=\"form-label\">Green multiplier</label> <input type=\"number\" id=\"gpct\" class=\"js-range-slider\" data-skin=\"round\" name=\"gpct\" value=\"\"/> </div> <div class=\"col s4 m3\"> <label for=\"bpct\" class=\"form-label\">Blue multiplier</label> <input type=\"number\" id=\"bpct\" class=\"js-range-slider\" data-skin=\"round\" name=\"bpct\" value=\"\"/> </div> </div> <div class=\"row\"><label class=\"control-label col s10\">HW buttons:</label> <div class=\"col s10\"> <div class=\"switch section\"> <label> Disable <input type=\"checkbox\" name=\"hwswitch\" id=\"hwswitch\" value=\"1\"> <span class=\"lever\"></span> Enable </label> </div> </div> </div> <div class=\"switchable\"> <div class=\"row\"> <div class=\"col s4 m3\"> <label for=\"on\">On Pin</label> <input type=\"number\" id=\"on\" name=\"on\"> </div> <div class=\"col s4 m3\"> <label for=\"off\">Off Pin</label> <input type=\"number\" id=\"off\" name=\"off\"> </div> </div> </div> <div class=\"row\"> <div class=\"col s10\"> <button type=\"submit\" class=\"waves-effect waves-light btn teal\">Save</button> <!--<button type=\"submit\" name=\"reboot\" class=\"waves-effect waves-light btn grey lighten-1\">Reboot</button>--> </div> </div> </form> </div> <div id=\"test3\" class=\"col s12\"> <form method=\"POST\" action=\"/\"> <input type=\"hidden\" name=\"section\" value=\"2\"> <div class=\"row\"> <div class=\"col s12\"> <label class=\"control-label\">Manual IP assignment:</label> <div class=\"switch section\"> <label> Disable <input type=\"checkbox\" name=\"disdhcp\" id=\"disdhcp\" value=\"0\"> <span class=\"lever\"></span> Enable </label> </div> </div> </div> <div class=\"switchable\"> <div class=\"row\"> <div class=\"col s12 m3\"> <label for=\"addr\">Ip</label> <input type=\"text\" id=\"addr\" name=\"addr\"> </div> <div class=\"col s12 m3\"> <label for=\"sm\">Submask</label> <input type=\"text\" id=\"sm\" name=\"sm\"> </div> <div class=\"col s12 m3\"> <label for=\"gw\">Gateway</label> <input type=\"text\" id=\"gw\" name=\"gw\"> </div> </div> </div> <div class=\"row\"> <div class=\"col s10\"> <button type=\"submit\" class=\"waves-effect waves-light btn teal\">Save</button> <!--<button type=\"submit\" name=\"reboot\" class=\"waves-effect waves-light btn grey lighten-1\">Reboot</button>--> <!--<button type=\"submit\" name=\"reboot\" class=\"waves-effect waves-light btn grey lighten-1\">Reboot</button>--> </div> </div> </form> </div> </div> </div> </div> <script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/3.4.1/jquery.min.js\"></script> <script src=\"https://cdnjs.cloudflare.com/ajax/libs/materialize/1.0.0/js/materialize.min.js\"></script> <script src=\"https://cerny.in/nouislider.js\"></script> <script src=\"https://cerny.in/diyhue.js\"></script> </body> </html>";

  websrv.send(200, "text/html", htmlContent);
  if (websrv.args()) {
    delay(1000);    // needs to wait until response is received by browser. If ESP restarts too soon, browser will think there was an error.
    esp_restart();
  }
}

void websrvNotFound() {
  Log("Not found " +  websrv.uri());
  websrv.send(404, "text/plain", WebLog("File Not Found\n\n"));
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(1000);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );

  WiFiManager wifiManager;
  wifiManager.autoConnect("New Hue Light");
  infoLight(CRGB::White);
  while (WiFi.status() != WL_CONNECTED) {
    infoLight(CRGB::Red);
    delay(500);
  }
  if (!useDhcp) {
    wifiManager.setSTAStaticIPConfig(address, gateway, submask);
  }
  WiFi.macAddress(mac);
  // ConnectMQTT();
    websrv.on("/state", HTTP_PUT, websrvStatePut);
    websrv.on("/state", HTTP_GET, websrvStateGet);
    websrv.on("/detect", HTTP_GET, websrvDetect);
    websrv.on("/discover", HTTP_GET, websrvDetect);

    websrv.on("/config", websrvConfig);
    websrv.on("/", websrvRoot);
    websrv.on("/reset", websrvReset);
    websrv.onNotFound(websrvNotFound);
    websrv.begin();

}

void loop() {

  FastLED.show();
  websrv.handleClient();
  objHue.lightEngine();
  // EVERY_N_MILLISECONDS(200) {
  //   MQTT.loop();
  // }
  // put your main code here, to run repeatedly:
}