import { Users, ClipboardCheck, Percent } from "lucide-react";
import "./Stats.css";
import { StatsData } from "../../types/Attendee";

interface StatsProps {
  stats: StatsData;
}

export default function Stats({ stats }: StatsProps) {
  return (
    <>
      <div className="stat-card">
        <div className="stat-content">
          <Users className="stat-icon total-icon" size={24} />
          <div className="stat-details">
            <h3 className="stat-label">Total Registered</h3>
            <p className="stat-value">{stats.totalAttendees}</p>
          </div>
        </div>
      </div>

      <div className="stat-card">
        <div className="stat-content">
          <ClipboardCheck className="stat-icon present-icon" size={24} />
          <div className="stat-details">
            <h3 className="stat-label">Present Today</h3>
            <p className="stat-value">{stats.presentToday}</p>
          </div>
        </div>
      </div>

      <div className="stat-card">
        <div className="stat-content">
          <Percent
            className={
              stats.quorumReached
                ? "stat-icon quorum-reached-icon"
                : "stat-icon quorum-not-reached-icon"
            }
            size={24}
          />
          <div className="stat-details">
            <h3 className="stat-label">Quorum Status</h3>
            <p className="stat-value">
              {stats.quorumPercentage.toFixed(1)}%
              <span
                className={
                  stats.quorumReached
                    ? "quorum-status quorum-reached"
                    : "quorum-status quorum-not-reached"
                }
              >
                {stats.quorumReached ? "(Reached)" : "(Not Reached)"}
              </span>
            </p>
          </div>
        </div>
      </div>
    </>
  );
}
