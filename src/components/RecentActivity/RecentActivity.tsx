import { Clock } from "lucide-react";
import "./RecentActivity.css";
import { Attendee } from "../../types/Attendee";

interface RecentActivityProps {
  attendees: Attendee[];
}

function RecentActivity({ attendees }: RecentActivityProps) {
  return (
    <div className="recent-activity">
      <h2 className="section-title">Recent Check-ins</h2>
      <div className="activity-list">
        {attendees.map((attendee) => (
          <div key={attendee.id} className="activity-item">
            <div className="activity-header">
              <div className="activity-name">{attendee.name}</div>
              <div className="activity-time">
                {new Date(attendee.timeIn).toLocaleTimeString("fr-FR")}
              </div>
            </div>
            <div className="activity-status">
              <Clock size={14} className="clock-icon" />
              just checked in
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

export default RecentActivity;
