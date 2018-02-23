void subscribeUpdateDelta () {
   //subscript to a topic
    int rc = client->subscribe(awsTopicUpdateDelta, MQTT::QOS0, messageArrivedUpdateDelta);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed");
}

void subscribeGetAccepted(){
    int rc = client->subscribe(awsTopicGetAccepted, MQTT::QOS0, messageArrivedGetAccepted);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed to awsTopicUpdate");
}

void unsubscribeGetAccepted(){
    int rc = client->unsubscribe(awsTopicGetAccepted);
    if (rc != 0) {
      Serial.print("rc from MQTT unsubscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT unsubscribed from awsTopicGetAccepted");
}

