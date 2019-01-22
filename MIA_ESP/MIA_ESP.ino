#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Yeet";
const char* password = "MIA2018x";

const String requestUrl = "http://miatest.vanoort.eu";
const String loginToken = "Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.eyJpc3MiOiJJc2FoIEF1dGhlbnRpY2F0aW9uIFNlcnZpY2UiLCJhdWQiOiJJc2FoIFdlYiBTZXJ2aWNlcyIsIm5iZiI6MTQ0ODAyNTY3MSwiZXhwIjoxNjcyNTIyMjAwMCwiSXNhaFVzZXJOYW1lIjoiTWFjaGluZU9wZXJhdGlvbnMiLCJNYWNoaW5lQXV0aGVudGljYXRpb24iOiJ0cnVlIn0.C9zrjyBstUNb3hK1e3kiAVYM1hwer09mbm5VO/b6fdstHoW3G1tAMrZA9n8ElEkqYqBkJCzs46QKnp1kawyNdZbyahsfYEBZ7czRoLCVr74ozwI6yC8Do1LwRO820voTBa+bQlHEh0nGlj++k+5odX/MIVf1eX9VkgBqAjOUbITRVwIT5ia260t5ZC0UI9gy1xr7k5wievliLSkrAIVmX++7nQOUzZgKOlHSu3/vmuBA1PCPOtE6l4brnrBc04ohzBAKt3CfZG4gXxeeVOwHPd1YEzSaePzGKlo725Zwk4/u44mDiee2fGfPAzjBLcL2radEJ5OAHjZuIuGIYambJZ+oDGmGUy55Fy/RImd6secT3DJhx5TIgBbEjPiDje32Q9BYAvogplHvzlzpHI9tOCQXG44APjxN9PjW1fFy3+7cVixgMM9zGwRHkeAjNSQluO+GXOygcjIr11GW4moZVY5ak2IOuM127FBfPJLnmKT8Lb0GgLnBpNXeV4rtKkemSYTRheeF8GcDHgWdR29S+RJT3MhvdEPpR3JDox6EgiuttbUd3fAFZD8u98iqkjn/zZxKsQY2io4bFfDfdmZXM9cTMKt8IlADdsM5jOm0txgvjd11cFfSlwp4A1tpRrXNMGw03ypMZ2DY9Lk+msSZEnIzDkJJ3illbDXBHZ5iGDI=";

const String sfcUsername = "ISAH";
const String sfcPassword = "M!@isah2018";

String authToken = "";
long tokenSetMillis = 0;

String lastOperation = "";

bool initialized = false;

String inputString = "";
bool stringComplete = false;

void setup () {
 
  Serial.begin(115200);
//  WiFi.begin(ssid, password);
// 
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(1000);
//    Serial.println("Connecting..");
//  }
 
}
 
void loop() {
//  if(!initialized){
//    String potentialToken = getAuthToken();
//    
//    if(!potentialToken.equalsIgnoreCase("")){
//      setAuthToken(potentialToken);
//      initialized = true;
//    }
//    else {
//      delay(1000);
//      return;
//    }
//  }
//
//  if(!tokenStillValid()){
//    Serial.println("Auth token is no longer valid, retrieving new token.");
//    String newToken = getAuthToken();
//    setAuthToken(newToken);
//  }

  while (Serial.available()) {
    String messageBuffer = "";
    char b = Serial.read();
    if(b == '\n' && messageBuffer.startsWith("A:")) {
      String msg = getReturningMessage(messageBuffer);
      Serial.println(msg);
      messageBuffer = "";
    }
    else {
      messageBuffer += b;
    }
  }
}

String getReturningMessage(String message){
  if(message.startsWith("A:")){
    if(message.indexOf("New_Operation") > 0){
      return "ESP: test operation";
    }
    else {
      return "ESP: Command (" + message + ") not supported...";
    }
  }
  else {
    return "ESP: Command (" + message + ") not supported...";
  }
}

//void serialEvent() {
//  if(stringComplete == false){
//    while (Serial.available()) {
//    char inChar = (char)Serial.read();
//    inputString += inChar;
//    
//      if (inChar == '\n') {
//        stringComplete = true;
//        onNewMessageReceived();
//      }
//    }
//  }
//}

//void onNewMessageReceived(){
//  if(inputString.startsWith("A: New Operation")){
//    Serial.println("ESP: Operation = " + lastOperation);
//    inputString = "";
//    return;
//  }
//  
//  if(inputString.startsWith("A: Operation Done")){
//    //Send API message
//    lastOperation = "";
//    Serial.println("ESP: Operation has been finished!");
//    inputString = "";
//    return;
//  }
//
//  //Example message:
//  //A: IoT: type=Info, message=Machine Temperature, value=50, unit=degrees,
//  if(inputString.startsWith("A: IoT:")){
//    inputString.replace("A: IoT: ", "");
//    
//    String type = getRequiredInfoFromInput("type");
//    String message = getRequiredInfoFromInput("message");
//    String value = getRequiredInfoFromInput("value");
//    String unit = getRequiredInfoFromInput("unit");
//    
//    //sendIotMessage(type, message, value, unit);
//    
//    Serial3.println("ESP: IoT message sent!");
//    inputString = "";
//    return;
//  }
//}

String getRequiredInfoFromInput(String prefix){
  if(inputString.indexOf(prefix)>0){
     int CountChar=prefix.length();
     int indexStart= inputString.indexOf(prefix);
     int indexStop= inputString.indexOf(",");
     String result = inputString.substring(indexStart+CountChar, indexStop);

     result.replace("=", "");
     
     return result;
  }
  return "";
}

bool tokenStillValid(){
  long currentMinutes = (millis() / 1000) / 60;
  long tokenSetMinutes = (tokenSetMillis / 1000) / 60;
  return currentMinutes - tokenSetMinutes < 15;
}

String setAuthToken(String newToken){
  authToken = "Bearer " + newToken;
  tokenSetMillis = millis();
  Serial.println("New auth token has been set!");
}

String getAuthToken(){
  if(WiFi.status()== WL_CONNECTED){
    String payload = "";
    HTTPClient http;

    http.begin(requestUrl + ":3087/MachineOperationService/users/" + sfcUsername + "/unsecure/token");
    http.addHeader("Authorization", loginToken);
    http.addHeader("Content-Type", "application/xml");
    http.addHeader("cache-control", "no-cache");

    int httpCode = http.PUT("<string xmlns=\"http://schemas.microsoft.com/2003/10/Serialization/\">" + sfcPassword + "</string>");

    if (httpCode > 0) { //Check the returning code
 
      payload = http.getString();
      payload.replace("<string xmlns=\"http://schemas.microsoft.com/2003/10/Serialization/\">", "");
      payload.replace("</string>", "");
      Serial.println(payload);
      
    }

    http.end();
    return payload;
  }

  return "";
}

void getOperationRaw(){
  if(WiFi.status()== WL_CONNECTED){
    String payload = "";
    HTTPClient http;

    http.begin(requestUrl + ":3087/MachineOperationService/productionorders/operations");
    http.addHeader("Authorization", authToken);
    http.addHeader("Content-Type", "application/xml");
    http.addHeader("cache-control", "no-cache");

    int httpCode = http.GET();

    if (httpCode > 0) {
      payload = http.getString();
      
      String param = xmlTakeParam(payload, "MachineOperation");
      lastOperation = param;
    }

    http.end();
  }
}

void sendIotMessage(String type, String message, String value, String unit){
  if(WiFi.status()== WL_CONNECTED){
    String payload = "";
    HTTPClient http;

    http.begin(requestUrl + ":3099/api/IoTMessage/");
    http.addHeader("Content-Type", "application/json");

    String body = "{\n\t\"MessageType\": \"" + type + "\",\n\t\"Description\": \"" + message + "\",\n\t\"Value\": \"" + value + "\",\n\t\"" + unit + "\": \"St\",\n\t\"MessageDate\": \"\"\n}";
    int httpCode = http.PUT(body);

    if (httpCode > 0) { //Check the returning code
 
      payload = http.getString();
      Serial.println(payload);
      
    }

    http.end();
  }
}

String xmlTakeParam(String inStr, String needParam)
{
  if(inStr.indexOf("<"+needParam+">")>0){
     int CountChar=needParam.length();
     int indexStart=inStr.indexOf("<"+needParam+">");
     int indexStop= inStr.indexOf("</"+needParam+">");  
     String resultParam = inStr.substring(indexStart+CountChar+2, indexStop);

     return resultParam;
  }
  return "not found";
}
