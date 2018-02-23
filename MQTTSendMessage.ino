
void sendUpdateMessage () {
    //send a message
    MQTT::Message message;
    char buf[100];
    
    StaticJsonBuffer<600> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& state = root.createNestedObject("state");
    JsonObject& state_reported = state.createNestedObject("reported");
    state_reported["inputLevel"] = inputLevel;
    state_reported["threshold"] = threshold;
    state_reported["alwaysOn"] = alwaysOn;
    state_reported["delayOff"] = delayOff;
    root.printTo(buf);
    
    //strcpy(buf, "{\"state\":{\"reported\":{\"inputLevel\": 50, \"threshold\": 40, \"alwaysOn\": true}}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(awsTopicUpdate, message); 
}


void sendUpdateDesiredMessage () {
    //send a message
    MQTT::Message message;
    char buf[200];
    
    StaticJsonBuffer<700> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& state = root.createNestedObject("state");
    JsonObject& state_reported = state.createNestedObject("reported");
    state_reported["inputLevel"] = inputLevel;
    state_reported["threshold"] = threshold;
    state_reported["alwaysOn"] = alwaysOn;
    state_reported["delayOff"] = delayOff;
   
    state["desired"] = (char*)0;
    root.printTo(buf);
    
    //strcpy(buf, "{\"state\":{\"reported\":{\"inputLevel\": 50, \"threshold\": 40, \"alwaysOn\": true}}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    int rc = client->publish(awsTopicUpdate, message); 
    Serial.println(buf);
    Serial.println("sendUpdateDesiredMessage() rc: ");
    Serial.println(rc);

}


void sendGetMessage (){
    Serial.println("SendGetMessage()");
    MQTT::Message message;
    char buf[100];
    strcpy(buf, "{\"state\":{\"reported\":{\"inputLevel\": 99, \"threshold\": 99, \"alwaysOn\": false}}}");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    Serial.println("SendGetMessage()2");
    int rc = client->publish(awsTopicGet, message); 
    Serial.println("SendGetMessage() rc: ");
    Serial.println(rc);
}
