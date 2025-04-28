// src/mqtt/mqttClient.ts
import mqtt from "mqtt";

class MQTTClient {
  private client: mqtt.MqttClient | null = null;
  private subscribeCallbacks: Map<string, ((message: string) => void)[]> =
    new Map();
  private connectionCallback: ((connected: boolean) => void) | null = null;

  connect(
    brokerUrl: string = "96e028affe4149618c56d875477fe9d8.s1.eu.hivemq.cloud:8883"
  ) {
    this.client = mqtt.connect(brokerUrl);

    this.client.on("connect", () => {
      console.log("Connected to MQTT broker");
      if (this.connectionCallback) {
        this.connectionCallback(true);
      }
    });

    this.client.on("error", (err) => {
      console.error("MQTT connection error:", err);
      if (this.connectionCallback) {
        this.connectionCallback(false);
      }
    });

    this.client.on("message", (topic, message) => {
      const messageStr = message.toString();
      console.log(`Received message on ${topic}: ${messageStr}`);

      const callbacks = this.subscribeCallbacks.get(topic) || [];
      callbacks.forEach((callback) => callback(messageStr));
    });
  }

  disconnect() {
    if (this.client) {
      this.client.end();
      this.client = null;
    }
  }

  subscribe(topic: string, callback: (message: string) => void) {
    if (!this.client) {
      console.error("MQTT client not connected");
      return;
    }

    this.client.subscribe(topic, (err) => {
      if (err) {
        console.error(`Error subscribing to ${topic}:`, err);
        return;
      }
      console.log(`Subscribed to ${topic}`);
    });

    // Add callback to the topic
    if (!this.subscribeCallbacks.has(topic)) {
      this.subscribeCallbacks.set(topic, []);
    }
    this.subscribeCallbacks.get(topic)?.push(callback);
  }

  publish(topic: string, message: string) {
    if (!this.client) {
      console.error("MQTT client not connected");
      return;
    }

    this.client.publish(topic, message);
    console.log(`Published to ${topic}: ${message}`);
  }

  onConnectionChange(callback: (connected: boolean) => void) {
    this.connectionCallback = callback;
  }
}

// Singleton instance
export const mqttClient = new MQTTClient();
export default mqttClient;
