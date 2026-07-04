use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use uuid::Uuid;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct User {
    pub id: Uuid,
    pub provider: String,
    pub provider_subject: String,
    pub display_name: String,
    pub created_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Session {
    pub token: String,
    pub user_id: Uuid,
    pub expires_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Profile {
    pub user_id: Uuid,
    pub handle: String,
    pub preferred_nation: Option<String>,
    pub rating: i32,
    pub wins: i32,
    pub losses: i32,
    pub updated_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Campaign {
    pub id: Uuid,
    pub owner_user_id: Uuid,
    pub name: String,
    pub mode: String,
    pub status: CampaignStatus,
    pub state_version: i32,
    pub cloud_save: Value,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum CampaignStatus {
    Active,
    Archived,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct MatchTicket {
    pub id: Uuid,
    pub user_id: Uuid,
    pub mode: String,
    pub preferred_nation: Option<String>,
    pub status: MatchTicketStatus,
    pub lobby_id: Option<Uuid>,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum MatchTicketStatus {
    Searching,
    Matched,
    Cancelled,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Lobby {
    pub id: Uuid,
    pub owner_user_id: Uuid,
    pub mode: String,
    pub status: LobbyStatus,
    pub max_players: u8,
    pub players: Vec<LobbyPlayer>,
    pub dedicated_server: Option<DedicatedServerAssignment>,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum LobbyStatus {
    Open,
    Ready,
    Assigned,
    Closed,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LobbyPlayer {
    pub user_id: Uuid,
    pub nation_iso: Option<String>,
    pub ready: bool,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DedicatedServerAssignment {
    pub region: String,
    pub connect_addr: String,
    pub build_id: String,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BattleResult {
    pub id: Uuid,
    pub campaign_id: Option<Uuid>,
    pub lobby_id: Option<Uuid>,
    pub winner_user_id: Option<Uuid>,
    pub loser_user_id: Option<Uuid>,
    pub validation_hash: String,
    pub summary: Value,
    pub accepted: bool,
    pub created_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct RankingEntry {
    pub user_id: Uuid,
    pub handle: String,
    pub rating: i32,
    pub wins: i32,
    pub losses: i32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DiplomacyEvent {
    pub id: Uuid,
    pub campaign_id: Uuid,
    pub actor_nation: String,
    pub target_nation: String,
    pub event_type: String,
    pub payload: Value,
    pub created_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ModManifest {
    pub id: Uuid,
    pub owner_user_id: Uuid,
    pub slug: String,
    pub name: String,
    pub version: String,
    pub checksum: String,
    pub download_url: Option<String>,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct TelemetryEvent {
    pub id: Uuid,
    pub user_id: Option<Uuid>,
    pub event_name: String,
    pub payload: Value,
    pub created_at: DateTime<Utc>,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Replay {
    pub id: Uuid,
    pub owner_user_id: Uuid,
    pub battle_result_id: Option<Uuid>,
    pub storage_url: String,
    pub checksum: String,
    pub metadata: Value,
    pub created_at: DateTime<Utc>,
}

#[derive(Debug, Deserialize)]
pub struct GuestAuthRequest {
    pub display_name: Option<String>,
}

#[derive(Debug, Serialize)]
pub struct AuthResponse {
    pub token: String,
    pub user: User,
    pub profile: Profile,
}

#[derive(Debug, Serialize)]
pub struct MeResponse {
    pub user: User,
    pub profile: Profile,
}

#[derive(Debug, Deserialize)]
pub struct UpdateProfileRequest {
    pub handle: Option<String>,
    pub preferred_nation: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct CreateCampaignRequest {
    pub name: String,
    pub mode: Option<String>,
    pub initial_state: Option<Value>,
}

#[derive(Debug, Deserialize)]
pub struct SaveCampaignRequest {
    pub state_version: i32,
    pub cloud_save: Value,
}

#[derive(Debug, Deserialize)]
pub struct CreateMatchTicketRequest {
    pub mode: String,
    pub preferred_nation: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct CreateLobbyRequest {
    pub mode: String,
    pub max_players: Option<u8>,
    pub nation_iso: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct JoinLobbyRequest {
    pub nation_iso: Option<String>,
    pub ready: Option<bool>,
}

#[derive(Debug, Deserialize)]
pub struct AssignDedicatedServerRequest {
    pub region: String,
    pub connect_addr: String,
    pub build_id: String,
}

#[derive(Debug, Deserialize)]
pub struct SubmitBattleResultRequest {
    pub campaign_id: Option<Uuid>,
    pub lobby_id: Option<Uuid>,
    pub winner_user_id: Option<Uuid>,
    pub loser_user_id: Option<Uuid>,
    pub validation_hash: String,
    pub summary: Value,
}

#[derive(Debug, Deserialize)]
pub struct RecordDiplomacyEventRequest {
    pub campaign_id: Uuid,
    pub actor_nation: String,
    pub target_nation: String,
    pub event_type: String,
    pub payload: Option<Value>,
}

#[derive(Debug, Deserialize)]
pub struct PublishModRequest {
    pub slug: String,
    pub name: String,
    pub version: String,
    pub checksum: String,
    pub download_url: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct TelemetryRequest {
    pub event_name: String,
    pub payload: Option<Value>,
}

#[derive(Debug, Deserialize)]
pub struct CreateReplayRequest {
    pub battle_result_id: Option<Uuid>,
    pub storage_url: String,
    pub checksum: String,
    pub metadata: Option<Value>,
}
