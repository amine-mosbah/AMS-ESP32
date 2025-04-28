// src/App.tsx
import React, { useState, useEffect } from "react";
import { Clock } from "lucide-react";
import AttendanceList from "./components/AttendanceList/AttendanceList";
import Stats from "./components/Stats/Stats";
import RecentActivity from "./components/RecentActivity/RecentActivity";
import mqttClient from "./mqtt/mqttClient";
import "./App.css";

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

// Mock database of members - in a real app this would come from a database
const membersDatabase = [
  {
    id: 1,
    name: "Sophie Martin",
    rfidTag: "ABC123",
    role: "Member",
  },
  {
    id: 2,
    name: "Thomas Dubois",
    rfidTag: "DEF456",
    role: "Board Member",
  },
  {
    id: 3,
    name: "Julie Leroy",
    rfidTag: "GHI789",
    role: "Secretary",
  },
  {
    id: 4,
    name: "Nicolas Petit",
    rfidTag: "JKL012",
    role: "Member",
  },
  {
    id: 5,
    name: "Emma Bernard",
    rfidTag: "MNO345",
    role: "Treasurer",
  },
  // Add more members as needed
];

// Define MQTT topics
const MQTT_CHECKIN_TOPIC = "assembly/attendance/checkin";
const MQTT_RESPONSE_TOPIC = "assembly/attendance/response";

function App() {
  const [attendees, setAttendees] = useState<Attendee[]>([]);
  const [stats, setStats] = useState<StatsData>({
    totalAttendees: membersDatabase.length,
    presentToday: 0,
    quorumReached: false,
    quorumPercentage: 0,
  });
  const [mqttConnected, setMqttConnected] = useState(false);

  // Calculate quorum percentage
  const updateStats = (currentAttendees: Attendee[]) => {
    const presentCount = currentAttendees.length;
    const totalCount = membersDatabase.length;
    const percentage = (presentCount / totalCount) * 100;
    const quorumReached = percentage >= 50; // Assume quorum is 50%

    setStats({
      totalAttendees: totalCount,
      presentToday: presentCount,
      quorumReached: quorumReached,
      quorumPercentage: percentage,
    });
  };

  // Process RFID from MQTT message
  const processRfidCheckIn = (rfidTag: string) => {
    console.log(`Processing RFID check-in: ${rfidTag}`);

    // Find member in database
    const member = membersDatabase.find((m) => m.rfidTag === rfidTag);

    if (!member) {
      console.log(`Unknown RFID tag: ${rfidTag}`);
      // Send response back to the device
      sendMqttResponse(false, "Unknown RFID");
      return;
    }

    // Check if already checked in
    const existingAttendee = attendees.find((a) => a.rfidTag === rfidTag);
    if (existingAttendee) {
      console.log(`Member already checked in: ${member.name}`);
      sendMqttResponse(false, "Already checked in");
      return;
    }

    // Add to attendees
    const newAttendee: Attendee = {
      id: member.id,
      name: member.name,
      rfidTag: member.rfidTag,
      timeIn: new Date().toISOString(),
      status: "present",
      role: member.role,
    };

    const updatedAttendees = [...attendees, newAttendee];
    setAttendees(updatedAttendees);
    updateStats(updatedAttendees);

    // Send success response
    sendMqttResponse(true, `Welcome ${member.name}`);
  };

  // Send response back to the device
  const sendMqttResponse = (success: boolean, message: string) => {
    const response = {
      success: success,
      message: message,
      timestamp: new Date().toISOString(),
    };

    mqttClient.publish(MQTT_RESPONSE_TOPIC, JSON.stringify(response));
  };

  // Handle incoming MQTT messages
  const handleMqttMessage = (message: string) => {
    try {
      const data = JSON.parse(message);
      if (data.rfidTag) {
        processRfidCheckIn(data.rfidTag);
      }
    } catch (error) {
      console.error("Error processing MQTT message:", error);
    }
  };

  // Connect to MQTT broker on component mount
  useEffect(() => {
    // Connect to MQTT broker
    mqttClient.connect();

    // Set up connection status callback
    mqttClient.onConnectionChange(setMqttConnected);

    // Subscribe to check-in topic
    mqttClient.subscribe(MQTT_CHECKIN_TOPIC, handleMqttMessage);

    // Cleanup on unmount
    return () => {
      mqttClient.disconnect();
    };
  }, []);

  // This effect handles updating stats when attendees change
  useEffect(() => {
    updateStats(attendees);
  }, [attendees]);

  return (
    <div className="app">
      <header className="header">
        <div className="container header-content">
          <h1 className="title">Assembly Attendance System</h1>
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
        </div>
      </header>
      <main className="container main-content">
        {!mqttConnected && (
          <div className="mqtt-status-warning">
            Connecting to MQTT broker...
          </div>
        )}
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
