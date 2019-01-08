#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

int requestIndex = 0;
 
const char* ssid = "Yeet";
const char* password = "MIA2018x";

const String requestUrl = "http://miatest.vanoort.eu";
const String loginToken = "Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJub25lIn0.eyJpc3MiOiJJc2FoIEF1dGhlbnRpY2F0aW9uIFNlcnZpY2UiLCJhdWQiOiJJc2FoIFdlYiBTZXJ2aWNlcyIsIm5iZiI6MTQ0ODAyNTY3MSwiZXhwIjoxNjcyNTIyMjAwMCwiSXNhaFVzZXJOYW1lIjoiTWFjaGluZU9wZXJhdGlvbnMiLCJNYWNoaW5lQXV0aGVudGljYXRpb24iOiJ0cnVlIn0.C9zrjyBstUNb3hK1e3kiAVYM1hwer09mbm5VO/b6fdstHoW3G1tAMrZA9n8ElEkqYqBkJCzs46QKnp1kawyNdZbyahsfYEBZ7czRoLCVr74ozwI6yC8Do1LwRO820voTBa+bQlHEh0nGlj++k+5odX/MIVf1eX9VkgBqAjOUbITRVwIT5ia260t5ZC0UI9gy1xr7k5wievliLSkrAIVmX++7nQOUzZgKOlHSu3/vmuBA1PCPOtE6l4brnrBc04ohzBAKt3CfZG4gXxeeVOwHPd1YEzSaePzGKlo725Zwk4/u44mDiee2fGfPAzjBLcL2radEJ5OAHjZuIuGIYambJZ+oDGmGUy55Fy/RImd6secT3DJhx5TIgBbEjPiDje32Q9BYAvogplHvzlzpHI9tOCQXG44APjxN9PjW1fFy3+7cVixgMM9zGwRHkeAjNSQluO+GXOygcjIr11GW4moZVY5ak2IOuM127FBfPJLnmKT8Lb0GgLnBpNXeV4rtKkemSYTRheeF8GcDHgWdR29S+RJT3MhvdEPpR3JDox6EgiuttbUd3fAFZD8u98iqkjn/zZxKsQY2io4bFfDfdmZXM9cTMKt8IlADdsM5jOm0txgvjd11cFfSlwp4A1tpRrXNMGw03ypMZ2DY9Lk+msSZEnIzDkJJ3illbDXBHZ5iGDI=";
const int sfcPort = 3087;
const String sfcUsername = "ISAH";
const String sfcPassword = "M!@isah2018";
const int iotPort = 3099;

String authToken = "";
long tokenSetMillis = 0;

bool initialized = false;

void setup () {
 
  Serial.begin(9600);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting..");
  }
 
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
    Serial.println("Auth token is no longer valid, retrieving new token.");
    String newToken = getAuthToken();
    setAuthToken(newToken);
  }

  //getOperationsRaw();
  //sendIotMessage("TEST", "Test bericht", "50", "Degrees");
  delay(5000);
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

String getOperationsRaw(){
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
      return payload;
      
    }

    http.end();
  }
  
  return "";
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
