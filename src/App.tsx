import { useState, useEffect } from "react";
import { Clock, RotateCcw } from "lucide-react";
import AttendanceList from "./components/AttendanceList/AttendanceList";
import Stats from "./components/Stats/Stats";
import RecentActivity from "./components/RecentActivity/RecentActivity";
import "./App.css";
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

const KNOWN_RFID_CARDS: Record<string, { name: string; role: string }> = {
  BBBA3040: { name: "Mohamed Amine BEN MOSBAH", role: "President" },
  "98923040": { name: "Ahmed Aziz BEN AYED", role: "Project Manager" },
  "0F9A631E": { name: "Sedki BAGGA", role: "Senior Member" },
  EF26401D: { name: "Rayen BEN ABDEJELLIL", role: "Senior Member" },
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

  useEffect(() => {
    const client = mqtt.connect(
      "wss://0dcf768ae0b747cf8b6d18fda0062323.s1.eu.hivemq.cloud:8884/mqtt",
      {
        username: "esp32rfid",
        password: "Hama12345",
        clientId:
          "assembly-attendance-web-" + Math.random().toString(16).substr(2, 8),
        clean: true,
        reconnectPeriod: 5000,
      }
    );

    client.on("connect", () => {
      setConnectionStatus("Connected");
      client.subscribe("rfid/card");
    });

    client.on("error", (err) => {
      setConnectionStatus("Connection Error");
      console.error("Connection error:", err);
    });

    client.on("close", () => {
      setConnectionStatus("Disconnected");
    });

    client.on("message", (topic, message) => {
      if (topic === "rfid/card") {
        try {
          const data = JSON.parse(message.toString());
          const cardUID = data.cardUID;
          const alreadyScanned = attendees.some((a) => a.rfidTag === cardUID);

          if (!alreadyScanned) {
            const memberInfo = KNOWN_RFID_CARDS[cardUID];
            if (memberInfo) {
              const newAttendee: Attendee = {
                id: Date.now(),
                name: memberInfo.name,
                rfidTag: cardUID,
                timeIn: new Date().toISOString(),
                status: "Present",
                role: memberInfo.role,
              };

              setAttendees((prev) => [...prev, newAttendee]);
              updateStats(attendees.length + 1);
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

  const updateStats = (presentCount: number) => {
    const totalMembers = Object.keys(KNOWN_RFID_CARDS).length;
    setStats({
      totalAttendees: totalMembers,
      presentToday: presentCount,
      quorumReached: presentCount >= Math.ceil(totalMembers * 0.5),
      quorumPercentage: (presentCount / totalMembers) * 100,
    });
  };

  const handleResetAttendance = () => {
    if (window.confirm("Are you sure you want to reset the attendance?")) {
      setAttendees([]);
      updateStats(0);
    }
  };

  return (
    <div className="app">
      <header className="header">
        <div className="container header-content">
          <h1 className="title">Assembly Attendance System</h1>
          <div className="header-right">
            <div className="current-time">
              <Clock size={20} />
              <span>
                {new Date().toLocaleString("fr-FR", {
                  weekday: "long",
                  year: "numeric",
                  month: "long",
                  day: "numeric",
                  hour: "2-digit",
                  minute: "2-digit",
                })}
              </span>
            </div>
            <div className="connection-container">
              <div
                className={`connection-status ${connectionStatus.toLowerCase()}`}
              >
                MQTT: {connectionStatus}
              </div>
              <button onClick={handleResetAttendance} className="reset-btn">
                <RotateCcw size={18} />
                Reset
              </button>
            </div>
          </div>
        </div>
      </header>

      <main className="container main-content">
        <div className="grid">
          <Stats stats={stats} />
          <div className="recent-activity-container">
            <RecentActivity attendees={attendees.slice(0, 3)} />
          </div>
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
