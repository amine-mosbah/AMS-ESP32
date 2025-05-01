// src/App.tsx
import React, { useState, useEffect } from "react";
import { Clock } from "lucide-react";
import AttendanceList from "./components/AttendanceList/AttendanceList";
import Stats from "./components/Stats/Stats";
import RecentActivity from "./components/RecentActivity/RecentActivity";
import "./App.css";

// Import MQTT client
import mqtt from "mqtt";

interface Attendee {
  id: number;
  name: string;
  rfidTag: string;
  timeIn: string;
  status: string;
  role: string;
}

interface StatsData {
  totalAttendees: number;
  presentToday: number;
  quorumReached: boolean;
  quorumPercentage: number;
}

// Define known RFID cards and their associated members
const KNOWN_RFID_CARDS: Record<string, { name: string; role: string }> = {
  BBBA3040: { name: "President Name", role: "President" },
  "98923040": { name: "Vice President Name", role: "Vice President" },
  "0F9A631E": { name: "Member One", role: "Member" },
  EF26401D: { name: "Member Two", role: "Member" },
};

function App() {
  const [attendees, setAttendees] = useState<Attendee[]>([]);
  const [stats, setStats] = useState<StatsData>({
    totalAttendees: Object.keys(KNOWN_RFID_CARDS).length,
    presentToday: 0,
    quorumReached: false,
    quorumPercentage: 0,
  });
  const [connectionStatus, setConnectionStatus] =
    useState<string>("Disconnected");

  // MQTT connection and message handling
  useEffect(() => {
    const client = mqtt.connect(
      "wss://0dcf768ae0b747cf8b6d18fda0062323.s1.eu.hivemq.cloud:8884/mqtt",
      {
        username: "esp32rfid",
        password: "Hama12345",
        clientId:
          "assembly-attendance-web-" + Math.random().toString(16).substr(2, 8),
        clean: true,
        reconnectPeriod: 5000, // Auto-reconnect every 5 seconds
      }
    );

    client.on("connect", () => {
      setConnectionStatus("Connected");
      console.log("Connected to HiveMQ");
      client.subscribe("rfid/card", (err) => {
        if (!err) {
          console.log("Subscribed to rfid/card");
        }
      });
    });

    client.on("error", (err) => {
      setConnectionStatus("Connection Error");
      console.error("Connection error:", err);
    });

    client.on("close", () => {
      setConnectionStatus("Disconnected");
      console.log("Connection closed");
    });

    client.on("message", (topic, message) => {
      if (topic === "rfid/card") {
        try {
          const data = JSON.parse(message.toString());
          const cardUID = data.cardUID;

          // Check if this card is already scanned today
          const alreadyScanned = attendees.some((a) => a.rfidTag === cardUID);

          if (!alreadyScanned) {
            const memberInfo = KNOWN_RFID_CARDS[cardUID];

            if (memberInfo) {
              const newAttendee: Attendee = {
                id: Date.now(), // Using timestamp as temporary ID
                name: memberInfo.name,
                rfidTag: cardUID,
                timeIn: new Date().toISOString(),
                status: "present",
                role: memberInfo.role,
              };

              setAttendees((prev) => [...prev, newAttendee]);

              // Update stats
              const newPresentCount = attendees.length + 1;
              const totalMembers = Object.keys(KNOWN_RFID_CARDS).length;
              const quorumPercentage = (newPresentCount / totalMembers) * 100;

              setStats({
                totalAttendees: totalMembers,
                presentToday: newPresentCount,
                quorumReached: newPresentCount >= Math.ceil(totalMembers * 0.5), // 50% quorum
                quorumPercentage: quorumPercentage,
              });
            } else {
              console.log(`Unknown RFID card scanned: ${cardUID}`);
            }
          }
        } catch (err) {
          console.error("Error processing RFID message:", err);
        }
      }
    });

    return () => {
      client.end();
    };
  }, [attendees]);

  return (
    <div className="app">
      <header className="header">
        <div className="container header-content">
          <h1 className="title">Assembly Attendance System</h1>
          <div className="header-right">
            <div className="current-time">
              <Clock size={24} />
              <div>
                {new Date().toLocaleString("fr-FR", {
                  weekday: "long",
                  year: "numeric",
                  month: "long",
                  day: "numeric",
                  hour: "2-digit",
                  minute: "2-digit",
                })}
              </div>
            </div>
            <div
              className={`connection-status ${connectionStatus.toLowerCase()}`}
            >
              MQTT: {connectionStatus}
            </div>
          </div>
        </div>
      </header>

      <main className="container main-content">
        <div className="grid">
          {/* Stats Cards */}
          <Stats stats={stats} />

          {/* Recent Activity */}
          <div className="recent-activity-container">
            <RecentActivity attendees={attendees.slice(0, 3)} />
          </div>

          {/* Attendance List */}
          <div className="attendance-list-container">
            <AttendanceList attendees={attendees} />
          </div>
        </div>
      </main>

      <footer className="footer">
        <div className="container footer-content">
          Réseaux d'Entreprises Project - Réseaux Hétérogènes © 2025
        </div>
      </footer>
    </div>
  );
}

export default App;
