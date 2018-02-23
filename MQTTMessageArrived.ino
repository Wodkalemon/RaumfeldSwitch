void printMessage(MQTT::Message &message) {
  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println(msg);
  delete msg;
}

void messageArrivedUpdateDelta(MQTT::MessageData& md) {
  MQTT::Message &message = md.message;
  printMessage(md.message);
  
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);

  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(msg);

  if (!root.success()) {
    Serial.println("Error: Failed to dezerialize JSON!");
  }
  
  JsonObject& state = root["state"];
  saveDesired(state);
  sendUpdateDesiredMessage();

  printVariables();
    
  delete msg;
}

void messageArrivedGetAccepted(MQTT::MessageData& md) {
  MQTT::Message &message = md.message;
  printMessage(md.message);
  
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);

  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(msg);

  if (!root.success()) {
    Serial.println("Error: Failed to dezerialize JSON!");
  }
    
  JsonObject& state_reported = root["state"]["reported"];
  inputLevel = state_reported["inputLevel"];
  threshold = state_reported["threshold"];
  alwaysOn = state_reported["alwaysOn"];
  delayOff = state_reported["delayOff"];
  putVariables();

  if (containsNestedKey(root, "desired")) {
    JsonObject& state_desired = root["state"]["desired"];
    saveDesired(state_desired);
    sendUpdateDesiredMessage();
  }

  
  printVariables();
  
  delete msg;
  messageGetArrived = true;
}

void saveDesired(JsonObject& json) {
  if (json.containsKey("inputLevel")) {
    inputLevel = json["inputLevel"];
  }
  if (json.containsKey("threshold")) {
    threshold = json["threshold"];
  }
  if (json.containsKey("alwaysOn")) {
    alwaysOn = json["alwaysOn"];
  }
  if (json.containsKey("delayOff")) {
    delayOff = json["delayOff"];
  }
  putVariables();
}
