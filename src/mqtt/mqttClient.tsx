var mqtt = require("mqtt");

var options = {
  host: "0dcf768ae0b747cf8b6d18fda0062323.s1.eu.hivemq.cloud",
  port: 8883,
  protocol: "mqtts",
  username: "esp32rfid",
  password: "Hama12345",
};

// initialize the MQTT client
var client = mqtt.connect(options);

// setup the callbacks
client.on("connect", function () {
  console.log("Connected");
});

client.on("error", function (error: any) {
  console.log(error);
});

client.on("message", function (topic: any, message: { toString: () => any }) {
  // called each time a message is received
  console.log("Received message:", topic, message.toString());
});

// subscribe to topic 'my/test/topic'
client.subscribe("my/test/topic");

// publish message 'Hello' to topic 'my/test/topic'
client.publish("my/test/topic", "Hello");
