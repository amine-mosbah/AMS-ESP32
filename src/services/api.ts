export interface Attendee {
  id: number;
  name: string;
  rfidTag: string;
  timeIn: string;
  status: string;
  role: string;
}

export interface StatsData {
  totalAttendees: number;
  presentToday: number;
  quorumReached: boolean;
  quorumPercentage: number;
}

export interface CheckInRequest {
  rfidTag: string;
}

export interface CheckInResponse {
  success: boolean;
  message: string;
  attendance: Attendee | null;
}

const API_URL = "0dcf768ae0b747cf8b6d18fda0062323.s1.eu.hivemq.cloud:8883";

export const apiService = {
  // Get all attendees for the current assembly
  async getAttendees(): Promise<Attendee[]> {
    try {
      const response = await fetch(`${API_URL}/attendees`);
      if (!response.ok) {
        throw new Error("Failed to fetch attendees");
      }
      return await response.json();
    } catch (error) {
      console.error("Error fetching attendees:", error);
      return [];
    }
  },

  // Get stats for the current assembly
  async getStats(): Promise<StatsData> {
    try {
      const response = await fetch(`${API_URL}/stats`);
      if (!response.ok) {
        throw new Error("Failed to fetch stats");
      }
      return await response.json();
    } catch (error) {
      console.error("Error fetching stats:", error);
      return {
        totalAttendees: 0,
        presentToday: 0,
        quorumReached: false,
        quorumPercentage: 0,
      };
    }
  },

  // Register check-in with RFID tag
  async checkIn(rfidTag: string): Promise<CheckInResponse> {
    try {
      const response = await fetch(`${API_URL}/check-in`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ rfidTag }),
      });

      if (!response.ok) {
        throw new Error("Check-in failed");
      }

      return await response.json();
    } catch (error) {
      console.error("Error during check-in:", error);
      return {
        success: false,
        message: "An error occurred during check-in",
        attendance: null,
      };
    }
  },
};
