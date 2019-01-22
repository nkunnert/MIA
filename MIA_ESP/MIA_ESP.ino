#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "MIAWIFI";
const char* password = "12345698";

const String requestUrl = "http://miatest.vanoort.eu";
const String loginToken = "Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.eyJpc3MiOiJJc2FoIEF1dGhlbnRpY2F0aW9uIFNlcnZpY2UiLCJhdWQiOiJJc2FoIFdlYiBTZXJ2aWNlcyIsIm5iZiI6MTQ0ODAyNTY3MSwiZXhwIjoxNjcyNTIyMjAwMCwiSXNhaFVzZXJOYW1lIjoiTWFjaGluZU9wZXJhdGlvbnMiLCJNYWNoaW5lQXV0aGVudGljYXRpb24iOiJ0cnVlIn0.C9zrjyBstUNb3hK1e3kiAVYM1hwer09mbm5VO/b6fdstHoW3G1tAMrZA9n8ElEkqYqBkJCzs46QKnp1kawyNdZbyahsfYEBZ7czRoLCVr74ozwI6yC8Do1LwRO820voTBa+bQlHEh0nGlj++k+5odX/MIVf1eX9VkgBqAjOUbITRVwIT5ia260t5ZC0UI9gy1xr7k5wievliLSkrAIVmX++7nQOUzZgKOlHSu3/vmuBA1PCPOtE6l4brnrBc04ohzBAKt3CfZG4gXxeeVOwHPd1YEzSaePzGKlo725Zwk4/u44mDiee2fGfPAzjBLcL2radEJ5OAHjZuIuGIYambJZ+oDGmGUy55Fy/RImd6secT3DJhx5TIgBbEjPiDje32Q9BYAvogplHvzlzpHI9tOCQXG44APjxN9PjW1fFy3+7cVixgMM9zGwRHkeAjNSQluO+GXOygcjIr11GW4moZVY5ak2IOuM127FBfPJLnmKT8Lb0GgLnBpNXeV4rtKkemSYTRheeF8GcDHgWdR29S+RJT3MhvdEPpR3JDox6EgiuttbUd3fAFZD8u98iqkjn/zZxKsQY2io4bFfDfdmZXM9cTMKt8IlADdsM5jOm0txgvjd11cFfSlwp4A1tpRrXNMGw03ypMZ2DY9Lk+msSZEnIzDkJJ3illbDXBHZ5iGDI=";

const String sfcUsername = "ISAH";
const String sfcPassword = "M!@isah2018";

String authToken = "";
long tokenSetMillis = 0;

bool initialized = false;

String s = "";
String lastOperation = "";

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("");
  Serial.println("ESPDEBUG: Initializing WiFi...");
  
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("ESPDEBUG: Connecting...");
  }

  Serial.println("ESPDEBUG: Connecting succesfull...");
}

void loop() {
  if(!initialized){
    String potentialToken = getAuthToken();
    
    if(!potentialToken.equalsIgnoreCase("")){
      setAuthToken(potentialToken);
      initialized = true;
    }
    else {
      delay(1000);
      return;
    }
  }

  if(!tokenStillValid()){
    //Serial.println("ESPDEBUG: Auth token is no longer valid, retrieving new token.");
    String newToken = getAuthToken();
    setAuthToken(newToken);
  }

//  Serial.println(getOperationRaw());
//  delay(1000);
//  Serial.println(startOperation("PO-18-0004", "1"));
//  delay(1000);
//  sendIotMessage("Alert", "Test bericht van ESP", "10", "Graden Celcius");
//  delay(1000);

  while(Serial.available()){
    char incomingChar = Serial.read();
    if(incomingChar == '\n'){
      String msg = "ESP: " + getReturningMessage();
      Serial.println(msg);
      s = "";
    }
    else {
      s += incomingChar;
    }
  }

    
}

String getReturningMessage(){
  if(s.startsWith("A:")){
    if(s.indexOf("New_Operation") > 0){
      String operation = getOperationRaw();
      if(operation != ""){
        String machCode = xmlTakeParam(operation, "MachCode");
        machCode.replace(" ", "");
        machCode.replace("-", "");
        
        String id = xmlTakeParam(operation, "ProdHeaderOrdNr");
        String color = machCode.substring(0, 1);
        String drawing = machCode.substring(1, 2);
        return "Operation:ID:" + id + "_1_COLOR:" + color + "_2_DRAWING:" + drawing + "_3_";
      }
      //return "Operation:ID:PO-18-9999_1_COLOR:G_2_DRAWING:X_3_";
    }
    
    //Example: IoT:TYPE:Info_1_MESSAGE:Motor_Temp_2_VALUE:50_3_UNIT:Degrees_4_
    if(s.indexOf("IoT:") > 0){
      String type = getIotType(s);
      String message = getIotMessage(s);
      String value = getIotValue(s);
      String unit = getIotUnit(s);
      
      return sendIotMessage(type, message, value, unit);
    }
    
    else {
      return "ESP: Command (" + s + ") ignored.";
    }
  }
//  else {
//    return "";
//  }
}

bool tokenStillValid(){
  long currentMinutes = (millis() / 1000) / 60;
  long tokenSetMinutes = (tokenSetMillis / 1000) / 60;
  return currentMinutes - tokenSetMinutes < 15;
}

String setAuthToken(String newToken){
  authToken = "Bearer " + newToken;
  tokenSetMillis = millis();
  //Serial.println("New auth token has been set!");
}

String getAuthToken(){
  Serial.println("ESPDEBUG: getAuthToken");
  if(WiFi.status()== WL_CONNECTED){
    //Serial.println("ESPDEBUG: getAuthToken WiFi is connected");
    String payload = "";
    HTTPClient http;

    http.begin(requestUrl + ":3087/MachineOperationService/users/" + sfcUsername + "/unsecure/token");
    http.addHeader("Authorization", loginToken);
    http.addHeader("Content-Type", "application/xml");
    http.addHeader("cache-control", "no-cache");

    //Serial.print("ESPDEBUG: getAuthToken http.PUT to ");
    //Serial.println(requestUrl);
    int httpCode = http.PUT("<string xmlns=\"http://schemas.microsoft.com/2003/10/Serialization/\">" + sfcPassword + "</string>");

    Serial.print("ESPDEBUG: getAuthToken http.PUT return code (must be>0): ");
    Serial.println(httpCode);
    if (httpCode > 0) { //Check the returning code
 
      //Serial.print("ESPDEBUG: getAuthToken get returncode from ");
      //Serial.println(requestUrl);

      payload = http.getString();
      payload.replace("<string xmlns=\"http://schemas.microsoft.com/2003/10/Serialization/\">", "");
      payload.replace("</string>", "");
      //Serial.print("ESPDEBUG: getAuthToken succesfully got returncode: ");
      //Serial.println(payload);
      
    }

    http.end();
    return payload;
  }

  return "";
}

//String startOperation(String operationId, String lineNr) {
//  if(WiFi.status()== WL_CONNECTED){
//    String payload = "";
//    HTTPClient http;
//
//    http.begin(requestUrl + ":3087/MachineOperationService/productionorders/" + operationId + "/operations/" + lineNr + "/timeregistrations");
//    http.addHeader("Content-Type", "application/json");
//
//    String body = "<int \"xmlns=http://schemas.microsoft.com/2003/10/Serialization/\">5</int>";
//    int httpCode = http.POST(body);
//
//    if (httpCode > 0) { //Check the returning code
// 
//      payload = http.getString();
//      //Serial.println(payload);
//      
//    }
//
//    http.end();
//    return payload;
//  }
//}
//
//String endOperation(String operationId, String lineNr){
//  
//}

String getOperationRaw(){
  if(WiFi.status()== WL_CONNECTED){
    String payload = "";
    String result = "";
    HTTPClient http;

    http.begin(requestUrl + ":3087/MachineOperationService/productionorders/operations");
    http.addHeader("Authorization", authToken);
    http.addHeader("Content-Type", "application/xml");
    http.addHeader("cache-control", "no-cache");

    int httpCode = http.GET();

    if (httpCode > 0) {
      payload = http.getString();
      
      result = xmlTakeParam(payload, "MachineOperation");
      while(!(xmlTakeParam(result, "MachCode").indexOf("-") > 0)){
        String removeThis = "<MachineOperation>" + result + "</MachineOperation>";
        payload.replace(removeThis, "");
        result = xmlTakeParam(payload, "MachineOperation");
      }
      //lastOperation = param;
    }

    http.end();
    return result;
  }
}

String sendIotMessage(String type, String message, String value, String unit){
  if(WiFi.status()== WL_CONNECTED){
    String payload = "";
    HTTPClient http;

    http.begin(requestUrl + ":3099/api/IoTMessage/");
    http.addHeader("Content-Type", "application/json");

    String body = "{\r\n\t\"MessageType\": \"" + type + "\",\r\n\t\"Description\": \"" + message + "\",\r\n\t\"Value\": \"" + value + "\",\r\n\t\"Unit\": \"" + unit + "\",\r\n\t\"MessageDate\": \"07-12-2018 12:00:00\"\r\n}";
    int httpCode = http.PUT(body);

    if (httpCode > 0) { //Check the returning code
 
      payload = http.getString();
      //Serial.println(payload);
      
    }

    http.end();
    return payload;
  }
}

String getIotType(String inStr){
  String needParam = "TYPE:";
  if(inStr.indexOf(needParam)>0){
     int CountChar=needParam.length();
     int indexStart=inStr.indexOf(needParam);
     int indexStop= inStr.indexOf("_1_");  
     String resultParam = inStr.substring(indexStart+CountChar, indexStop);

     return resultParam;
  }
  return "not found";
}

String getIotMessage(String inStr){
  String needParam = "MESSAGE:";
  if(inStr.indexOf(needParam)>0){
     int CountChar=needParam.length();
     int indexStart=inStr.indexOf(needParam);
     int indexStop= inStr.indexOf("_2_");  
     String resultParam = inStr.substring(indexStart+CountChar, indexStop);

     return resultParam;
  }
  return "not found";
}

String getIotValue(String inStr){
  String needParam = "VALUE:";
  if(inStr.indexOf(needParam)>0){
     int CountChar=needParam.length();
     int indexStart=inStr.indexOf(needParam);
     int indexStop= inStr.indexOf("_3_");  
     String resultParam = inStr.substring(indexStart+CountChar, indexStop);

     return resultParam;
  }
  return "not found";
}

String getIotUnit(String inStr){
  String needParam = "UNIT:";
  if(inStr.indexOf(needParam)>0){
     int CountChar=needParam.length();
     int indexStart=inStr.indexOf(needParam);
     int indexStop= inStr.indexOf("_4_");  
     String resultParam = inStr.substring(indexStart+CountChar, indexStop);

     return resultParam;
  }
  return "not found";
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
