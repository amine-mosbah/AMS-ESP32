// src/hooks/useMqtt.ts
import { useEffect, useRef, useState } from "react";
import mqtt from "mqtt";

export const useMqtt = (onMessage: (message: string) => void) => {
  const [mqttConnected, setMqttConnected] = useState(false);
  const clientRef = useRef<mqtt.MqttClient | null>(null);

  useEffect(() => {
    const options = {
      host: "REDACTED",
      port: 8883,
      protocol: "mqtts",
      username: "esp32rfid",
      password: "REDACTED",
    };

    const client = mqtt.connect(options);
    clientRef.current = client;

    client.on("connect", () => {
      console.log("✅ Connected to HiveMQ");
      setMqttConnected(true);
      client.subscribe("rfid/card");
    });

    client.on("message", (topic, message) => {
      if (topic === "rfid/card") {
        console.log("📥 Received:", message.toString());
        onMessage(message.toString());
      }
    });

    client.on("error", (err) => {
      console.error("❌ MQTT Error:", err);
    });

    client.on("close", () => {
      console.log("🔌 MQTT Disconnected");
      setMqttConnected(false);
    });

    return () => {
      client.end();
    };
  }, [onMessage]);

  return mqttConnected;
};
