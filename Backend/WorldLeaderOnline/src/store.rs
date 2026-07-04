use std::collections::HashMap;

use chrono::{Duration, Utc};
use serde_json::Value;
use uuid::Uuid;

use crate::{
    error::{ApiError, ApiResult},
    models::*,
};

#[derive(Default)]
pub struct MemoryStore {
    pub users: HashMap<Uuid, User>,
    pub sessions: HashMap<String, Session>,
    pub profiles: HashMap<Uuid, Profile>,
    pub campaigns: HashMap<Uuid, Campaign>,
    pub tickets: HashMap<Uuid, MatchTicket>,
    pub lobbies: HashMap<Uuid, Lobby>,
    pub battle_results: HashMap<Uuid, BattleResult>,
    pub diplomacy_events: HashMap<Uuid, DiplomacyEvent>,
    pub mods: HashMap<Uuid, ModManifest>,
    pub telemetry_events: HashMap<Uuid, TelemetryEvent>,
    pub replays: HashMap<Uuid, Replay>,
}

impl MemoryStore {
    pub fn upsert_document(&mut self, kind: &str, body: Value) -> ApiResult<()> {
        match kind {
            "users" => {
                let value: User = serde_json::from_value(body).map_err(storage_err)?;
                self.users.insert(value.id, value);
            }
            "profiles" => {
                let value: Profile = serde_json::from_value(body).map_err(storage_err)?;
                self.profiles.insert(value.user_id, value);
            }
            "campaigns" => {
                let value: Campaign = serde_json::from_value(body).map_err(storage_err)?;
                self.campaigns.insert(value.id, value);
            }
            "tickets" => {
                let value: MatchTicket = serde_json::from_value(body).map_err(storage_err)?;
                self.tickets.insert(value.id, value);
            }
            "lobbies" => {
                let value: Lobby = serde_json::from_value(body).map_err(storage_err)?;
                self.lobbies.insert(value.id, value);
            }
            "battle_results" => {
                let value: BattleResult = serde_json::from_value(body).map_err(storage_err)?;
                self.battle_results.insert(value.id, value);
            }
            "diplomacy_events" => {
                let value: DiplomacyEvent = serde_json::from_value(body).map_err(storage_err)?;
                self.diplomacy_events.insert(value.id, value);
            }
            "mods" => {
                let value: ModManifest = serde_json::from_value(body).map_err(storage_err)?;
                self.mods.insert(value.id, value);
            }
            "replays" => {
                let value: Replay = serde_json::from_value(body).map_err(storage_err)?;
                self.replays.insert(value.id, value);
            }
            _ => {}
        }
        Ok(())
    }

    pub fn create_guest_user(&mut self, display_name: Option<String>) -> AuthResponse {
        let now = Utc::now();
        let user_id = Uuid::new_v4();
        let name = clean_string(display_name, "Commander", 40);
        let user = User {
            id: user_id,
            provider: "guest".to_string(),
            provider_subject: user_id.to_string(),
            display_name: name.clone(),
            created_at: now,
        };
        let profile = Profile {
            user_id,
            handle: name,
            preferred_nation: None,
            rating: 1000,
            wins: 0,
            losses: 0,
            updated_at: now,
        };
        let token = format!("wl_dev_{}", Uuid::new_v4().simple());
        let session = Session {
            token: token.clone(),
            user_id,
            expires_at: now + Duration::days(30),
        };

        self.users.insert(user.id, user.clone());
        self.profiles.insert(profile.user_id, profile.clone());
        self.sessions.insert(token.clone(), session);

        AuthResponse {
            token,
            user,
            profile,
        }
    }

    pub fn authenticate(&self, token: &str) -> ApiResult<User> {
        let session = self.sessions.get(token).ok_or(ApiError::Unauthorized)?;
        if session.expires_at < Utc::now() {
            return Err(ApiError::Unauthorized);
        }
        self.users
            .get(&session.user_id)
            .cloned()
            .ok_or(ApiError::Unauthorized)
    }

    pub fn profile(&self, user_id: Uuid) -> ApiResult<Profile> {
        self.profiles
            .get(&user_id)
            .cloned()
            .ok_or(ApiError::NotFound("profile"))
    }

    pub fn update_profile(
        &mut self,
        user_id: Uuid,
        request: UpdateProfileRequest,
    ) -> ApiResult<Profile> {
        let profile = self
            .profiles
            .get_mut(&user_id)
            .ok_or(ApiError::NotFound("profile"))?;
        if let Some(handle) = request.handle {
            profile.handle = clean_required(handle, "handle", 3, 40)?;
        }
        profile.preferred_nation = request
            .preferred_nation
            .map(|iso| iso.trim().to_uppercase());
        profile.updated_at = Utc::now();
        Ok(profile.clone())
    }

    pub fn create_campaign(
        &mut self,
        user_id: Uuid,
        request: CreateCampaignRequest,
    ) -> ApiResult<Campaign> {
        let now = Utc::now();
        let campaign = Campaign {
            id: Uuid::new_v4(),
            owner_user_id: user_id,
            name: clean_required(request.name, "campaign name", 3, 80)?,
            mode: request
                .mode
                .unwrap_or_else(|| "online_campaign".to_string()),
            status: CampaignStatus::Active,
            state_version: 1,
            cloud_save: request
                .initial_state
                .unwrap_or_else(|| serde_json::json!({})),
            created_at: now,
            updated_at: now,
        };
        self.campaigns.insert(campaign.id, campaign.clone());
        Ok(campaign)
    }

    pub fn list_campaigns(&self, user_id: Uuid) -> Vec<Campaign> {
        let mut values: Vec<_> = self
            .campaigns
            .values()
            .filter(|campaign| campaign.owner_user_id == user_id)
            .cloned()
            .collect();
        values.sort_by_key(|campaign| campaign.created_at);
        values
    }

    pub fn get_campaign(&self, user_id: Uuid, campaign_id: Uuid) -> ApiResult<Campaign> {
        let campaign = self
            .campaigns
            .get(&campaign_id)
            .cloned()
            .ok_or(ApiError::NotFound("campaign"))?;
        if campaign.owner_user_id != user_id {
            return Err(ApiError::Unauthorized);
        }
        Ok(campaign)
    }

    pub fn save_campaign(
        &mut self,
        user_id: Uuid,
        campaign_id: Uuid,
        request: SaveCampaignRequest,
    ) -> ApiResult<Campaign> {
        let campaign = self
            .campaigns
            .get_mut(&campaign_id)
            .ok_or(ApiError::NotFound("campaign"))?;
        if campaign.owner_user_id != user_id {
            return Err(ApiError::Unauthorized);
        }
        if request.state_version < campaign.state_version {
            return Err(ApiError::Conflict(format!(
                "stale state_version {}, current {}",
                request.state_version, campaign.state_version
            )));
        }
        campaign.state_version = request.state_version;
        campaign.cloud_save = request.cloud_save;
        campaign.updated_at = Utc::now();
        Ok(campaign.clone())
    }

    pub fn create_lobby(&mut self, user_id: Uuid, request: CreateLobbyRequest) -> ApiResult<Lobby> {
        let now = Utc::now();
        let max_players = request.max_players.unwrap_or(2).clamp(2, 8);
        let lobby = Lobby {
            id: Uuid::new_v4(),
            owner_user_id: user_id,
            mode: clean_required(request.mode, "mode", 2, 40)?,
            status: LobbyStatus::Open,
            max_players,
            players: vec![LobbyPlayer {
                user_id,
                nation_iso: request.nation_iso.map(|iso| iso.trim().to_uppercase()),
                ready: false,
            }],
            dedicated_server: None,
            created_at: now,
            updated_at: now,
        };
        self.lobbies.insert(lobby.id, lobby.clone());
        Ok(lobby)
    }

    pub fn join_lobby(
        &mut self,
        user_id: Uuid,
        lobby_id: Uuid,
        request: JoinLobbyRequest,
    ) -> ApiResult<Lobby> {
        let lobby = self
            .lobbies
            .get_mut(&lobby_id)
            .ok_or(ApiError::NotFound("lobby"))?;
        if lobby.status != LobbyStatus::Open && lobby.status != LobbyStatus::Ready {
            return Err(ApiError::Conflict("lobby is not joinable".to_string()));
        }
        if let Some(player) = lobby.players.iter_mut().find(|p| p.user_id == user_id) {
            player.nation_iso = request.nation_iso.map(|iso| iso.trim().to_uppercase());
            player.ready = request.ready.unwrap_or(player.ready);
        } else {
            if lobby.players.len() >= lobby.max_players as usize {
                return Err(ApiError::Conflict("lobby is full".to_string()));
            }
            lobby.players.push(LobbyPlayer {
                user_id,
                nation_iso: request.nation_iso.map(|iso| iso.trim().to_uppercase()),
                ready: request.ready.unwrap_or(false),
            });
        }
        lobby.status = if lobby.players.len() >= lobby.max_players as usize
            && lobby.players.iter().all(|p| p.ready)
        {
            LobbyStatus::Ready
        } else {
            LobbyStatus::Open
        };
        lobby.updated_at = Utc::now();
        Ok(lobby.clone())
    }

    pub fn get_lobby(&self, lobby_id: Uuid) -> ApiResult<Lobby> {
        self.lobbies
            .get(&lobby_id)
            .cloned()
            .ok_or(ApiError::NotFound("lobby"))
    }

    pub fn assign_dedicated_server(
        &mut self,
        user_id: Uuid,
        lobby_id: Uuid,
        request: AssignDedicatedServerRequest,
    ) -> ApiResult<Lobby> {
        let lobby = self
            .lobbies
            .get_mut(&lobby_id)
            .ok_or(ApiError::NotFound("lobby"))?;
        if lobby.owner_user_id != user_id {
            return Err(ApiError::Unauthorized);
        }
        lobby.dedicated_server = Some(DedicatedServerAssignment {
            region: clean_required(request.region, "region", 2, 40)?,
            connect_addr: clean_required(request.connect_addr, "connect_addr", 3, 256)?,
            build_id: clean_required(request.build_id, "build_id", 3, 80)?,
        });
        lobby.status = LobbyStatus::Assigned;
        lobby.updated_at = Utc::now();
        Ok(lobby.clone())
    }

    pub fn create_match_ticket(
        &mut self,
        user_id: Uuid,
        request: CreateMatchTicketRequest,
    ) -> ApiResult<MatchTicket> {
        let now = Utc::now();
        let mut ticket = MatchTicket {
            id: Uuid::new_v4(),
            user_id,
            mode: clean_required(request.mode, "mode", 2, 40)?,
            preferred_nation: request
                .preferred_nation
                .map(|iso| iso.trim().to_uppercase()),
            status: MatchTicketStatus::Searching,
            lobby_id: None,
            created_at: now,
            updated_at: now,
        };

        if let Some((other_id, _)) = self
            .tickets
            .iter()
            .find(|(_, other)| {
                other.user_id != user_id
                    && other.mode == ticket.mode
                    && other.status == MatchTicketStatus::Searching
            })
            .map(|(id, other)| (*id, other.clone()))
        {
            let lobby = Lobby {
                id: Uuid::new_v4(),
                owner_user_id: user_id,
                mode: ticket.mode.clone(),
                status: LobbyStatus::Open,
                max_players: 2,
                players: vec![
                    LobbyPlayer {
                        user_id,
                        nation_iso: ticket.preferred_nation.clone(),
                        ready: false,
                    },
                    LobbyPlayer {
                        user_id: self.tickets[&other_id].user_id,
                        nation_iso: self.tickets[&other_id].preferred_nation.clone(),
                        ready: false,
                    },
                ],
                dedicated_server: None,
                created_at: now,
                updated_at: now,
            };
            ticket.status = MatchTicketStatus::Matched;
            ticket.lobby_id = Some(lobby.id);
            ticket.updated_at = now;
            if let Some(other) = self.tickets.get_mut(&other_id) {
                other.status = MatchTicketStatus::Matched;
                other.lobby_id = Some(lobby.id);
                other.updated_at = now;
            }
            self.lobbies.insert(lobby.id, lobby);
        }

        self.tickets.insert(ticket.id, ticket.clone());
        Ok(ticket)
    }

    pub fn get_match_ticket(&self, user_id: Uuid, ticket_id: Uuid) -> ApiResult<MatchTicket> {
        let ticket = self
            .tickets
            .get(&ticket_id)
            .cloned()
            .ok_or(ApiError::NotFound("match ticket"))?;
        if ticket.user_id != user_id {
            return Err(ApiError::Unauthorized);
        }
        Ok(ticket)
    }

    pub fn submit_battle_result(
        &mut self,
        request: SubmitBattleResultRequest,
    ) -> ApiResult<BattleResult> {
        let hash = clean_required(request.validation_hash, "validation_hash", 12, 256)?;
        if request.winner_user_id == request.loser_user_id && request.winner_user_id.is_some() {
            return Err(ApiError::BadRequest(
                "winner_user_id and loser_user_id cannot match".to_string(),
            ));
        }
        if let Some(winner) = request.winner_user_id
            && !self.profiles.contains_key(&winner)
        {
            return Err(ApiError::NotFound("winner profile"));
        }
        if let Some(loser) = request.loser_user_id
            && !self.profiles.contains_key(&loser)
        {
            return Err(ApiError::NotFound("loser profile"));
        }

        let result = BattleResult {
            id: Uuid::new_v4(),
            campaign_id: request.campaign_id,
            lobby_id: request.lobby_id,
            winner_user_id: request.winner_user_id,
            loser_user_id: request.loser_user_id,
            validation_hash: hash,
            summary: request.summary,
            accepted: true,
            created_at: Utc::now(),
        };
        if let Some(winner) = result.winner_user_id
            && let Some(profile) = self.profiles.get_mut(&winner)
        {
            profile.wins += 1;
            profile.rating += 16;
            profile.updated_at = Utc::now();
        }
        if let Some(loser) = result.loser_user_id
            && let Some(profile) = self.profiles.get_mut(&loser)
        {
            profile.losses += 1;
            profile.rating = (profile.rating - 12).max(100);
            profile.updated_at = Utc::now();
        }
        self.battle_results.insert(result.id, result.clone());
        Ok(result)
    }

    pub fn rankings(&self, limit: usize) -> Vec<RankingEntry> {
        let mut values: Vec<_> = self
            .profiles
            .values()
            .map(|profile| RankingEntry {
                user_id: profile.user_id,
                handle: profile.handle.clone(),
                rating: profile.rating,
                wins: profile.wins,
                losses: profile.losses,
            })
            .collect();
        values.sort_by(|a, b| {
            b.rating
                .cmp(&a.rating)
                .then_with(|| a.handle.cmp(&b.handle))
        });
        values.truncate(limit.clamp(1, 100));
        values
    }

    pub fn record_diplomacy_event(
        &mut self,
        request: RecordDiplomacyEventRequest,
    ) -> ApiResult<DiplomacyEvent> {
        if !self.campaigns.contains_key(&request.campaign_id) {
            return Err(ApiError::NotFound("campaign"));
        }
        let event = DiplomacyEvent {
            id: Uuid::new_v4(),
            campaign_id: request.campaign_id,
            actor_nation: clean_required(request.actor_nation, "actor_nation", 2, 8)?
                .to_uppercase(),
            target_nation: clean_required(request.target_nation, "target_nation", 2, 8)?
                .to_uppercase(),
            event_type: clean_required(request.event_type, "event_type", 3, 64)?,
            payload: request.payload.unwrap_or_else(|| serde_json::json!({})),
            created_at: Utc::now(),
        };
        self.diplomacy_events.insert(event.id, event.clone());
        Ok(event)
    }

    pub fn diplomacy_events(&self, campaign_id: Uuid) -> Vec<DiplomacyEvent> {
        let mut values: Vec<_> = self
            .diplomacy_events
            .values()
            .filter(|event| event.campaign_id == campaign_id)
            .cloned()
            .collect();
        values.sort_by_key(|event| event.created_at);
        values
    }

    pub fn publish_mod(
        &mut self,
        user_id: Uuid,
        request: PublishModRequest,
    ) -> ApiResult<ModManifest> {
        let now = Utc::now();
        let slug = clean_required(request.slug, "slug", 3, 80)?;
        let manifest = ModManifest {
            id: Uuid::new_v4(),
            owner_user_id: user_id,
            slug,
            name: clean_required(request.name, "name", 3, 80)?,
            version: clean_required(request.version, "version", 1, 32)?,
            checksum: clean_required(request.checksum, "checksum", 8, 256)?,
            download_url: request.download_url,
            created_at: now,
            updated_at: now,
        };
        self.mods.insert(manifest.id, manifest.clone());
        Ok(manifest)
    }

    pub fn mods(&self) -> Vec<ModManifest> {
        let mut values: Vec<_> = self.mods.values().cloned().collect();
        values.sort_by(|a, b| a.slug.cmp(&b.slug));
        values
    }

    pub fn ingest_telemetry(
        &mut self,
        user_id: Option<Uuid>,
        request: TelemetryRequest,
    ) -> ApiResult<TelemetryEvent> {
        let event = TelemetryEvent {
            id: Uuid::new_v4(),
            user_id,
            event_name: clean_required(request.event_name, "event_name", 3, 80)?,
            payload: request.payload.unwrap_or_else(|| serde_json::json!({})),
            created_at: Utc::now(),
        };
        self.telemetry_events.insert(event.id, event.clone());
        Ok(event)
    }

    pub fn create_replay(
        &mut self,
        user_id: Uuid,
        request: CreateReplayRequest,
    ) -> ApiResult<Replay> {
        if let Some(result_id) = request.battle_result_id
            && !self.battle_results.contains_key(&result_id)
        {
            return Err(ApiError::NotFound("battle result"));
        }
        let replay = Replay {
            id: Uuid::new_v4(),
            owner_user_id: user_id,
            battle_result_id: request.battle_result_id,
            storage_url: clean_required(request.storage_url, "storage_url", 8, 2048)?,
            checksum: clean_required(request.checksum, "checksum", 8, 256)?,
            metadata: request.metadata.unwrap_or_else(|| serde_json::json!({})),
            created_at: Utc::now(),
        };
        self.replays.insert(replay.id, replay.clone());
        Ok(replay)
    }

    pub fn get_replay(&self, replay_id: Uuid) -> ApiResult<Replay> {
        self.replays
            .get(&replay_id)
            .cloned()
            .ok_or(ApiError::NotFound("replay"))
    }
}

fn clean_string(value: Option<String>, fallback: &str, max: usize) -> String {
    let value = value.unwrap_or_else(|| fallback.to_string());
    let trimmed = value.trim();
    if trimmed.is_empty() {
        fallback.to_string()
    } else {
        trimmed.chars().take(max).collect()
    }
}

fn clean_required(value: String, field: &str, min: usize, max: usize) -> ApiResult<String> {
    let trimmed = value.trim();
    if trimmed.len() < min || trimmed.len() > max {
        return Err(ApiError::BadRequest(format!(
            "{field} length must be between {min} and {max}"
        )));
    }
    Ok(trimmed.to_string())
}

fn storage_err(err: serde_json::Error) -> ApiError {
    ApiError::Storage(err.to_string())
}
