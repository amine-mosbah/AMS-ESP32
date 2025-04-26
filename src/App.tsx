// src/App.tsx
import React, { useState, useEffect } from "react";
import { Clock } from "lucide-react";
import AttendanceList from "./components/AttendanceList/AttendanceList";
import Stats from "./components/Stats/Stats";
import RecentActivity from "./components/RecentActivity/RecentActivity";
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

function App() {
  const [attendees, setAttendees] = useState<Attendee[]>([]);
  const [stats, setStats] = useState<StatsData>({
    totalAttendees: 0,
    presentToday: 0,
    quorumReached: false,
    quorumPercentage: 0,
  });

  // Fetch data from backend
  useEffect(() => {
    // Replace with actual API call
    const fetchData = async () => {
      try {
        // This would be your actual API endpoint
        // const response = await fetch('http://localhost:8080/api/attendees');
        // const data = await response.json();

        // Mock data for now
        const mockData: Attendee[] = [
          {
            id: 1,
            name: "Sophie Martin",
            rfidTag: "ABC123",
            timeIn: "2025-04-25T09:15:22",
            status: "present",
            role: "Member",
          },
          {
            id: 2,
            name: "Thomas Dubois",
            rfidTag: "DEF456",
            timeIn: "2025-04-25T09:20:45",
            status: "present",
            role: "Board Member",
          },
          {
            id: 3,
            name: "Julie Leroy",
            rfidTag: "GHI789",
            timeIn: "2025-04-25T09:25:10",
            status: "present",
            role: "Secretary",
          },
          {
            id: 4,
            name: "Nicolas Petit",
            rfidTag: "JKL012",
            timeIn: "2025-04-25T09:32:55",
            status: "present",
            role: "Member",
          },
          {
            id: 5,
            name: "Emma Bernard",
            rfidTag: "MNO345",
            timeIn: "2025-04-25T09:36:20",
            status: "present",
            role: "Treasurer",
          },
        ];

        setAttendees(mockData);

        // Calculate stats
        setStats({
          totalAttendees: 50, // Mock total registered members
          presentToday: mockData.length,
          quorumReached: mockData.length >= 25, // Mock quorum requirement
          quorumPercentage: (mockData.length / 50) * 100,
        });
      } catch (error) {
        console.error("Error fetching data:", error);
      }
    };

    fetchData();

    // Set up polling to refresh data every 10 seconds
    const interval = setInterval(fetchData, 10000);
    return () => clearInterval(interval);
  }, []);

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
