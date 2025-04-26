import React from "react";
import "./AttendanceList.css";

interface Attendee {
  id: number;
  name: string;
  rfidTag: string;
  timeIn: string;
  status: string;
  role: string;
}

interface AttendanceListProps {
  attendees: Attendee[];
}

function AttendanceList({ attendees }: AttendanceListProps) {
  return (
    <div className="attendance-list">
      <h2 className="section-title">Attendance List</h2>
      <div className="table-container">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Role</th>
              <th>RFID Tag</th>
              <th>Time In</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {attendees.map((attendee) => (
              <tr key={attendee.id}>
                <td>{attendee.name}</td>
                <td>{attendee.role}</td>
                <td>{attendee.rfidTag}</td>
                <td>{new Date(attendee.timeIn).toLocaleTimeString("fr-FR")}</td>
                <td>
                  <span className="status-badge">{attendee.status}</span>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export default AttendanceList;
