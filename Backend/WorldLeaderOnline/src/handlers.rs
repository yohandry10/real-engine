use axum::{
    Json,
    extract::{Path, Query, State},
    http::HeaderMap,
};
use serde::Deserialize;
use serde_json::json;
use uuid::Uuid;

use crate::{
    AppState,
    error::{ApiError, ApiResult},
    models::*,
};

pub async fn healthz() -> Json<serde_json::Value> {
    Json(json!({
        "status": "ok",
        "service": "worldleader-online",
        "version": env!("CARGO_PKG_VERSION")
    }))
}

pub async fn readyz(State(state): State<AppState>) -> Json<serde_json::Value> {
    let postgres = state.postgres_ready().await;
    let redis = state.redis_ready().await;
    Json(json!({
        "status": if postgres && redis { "ready" } else { "degraded" },
        "postgres": postgres,
        "redis": redis
    }))
}

pub async fn auth_guest(
    State(state): State<AppState>,
    Json(request): Json<GuestAuthRequest>,
) -> ApiResult<Json<AuthResponse>> {
    let response = {
        let mut store = state.store.write().await;
        store.create_guest_user(request.display_name)
    };
    state
        .persist_document("users", response.user.id.to_string(), &response.user)
        .await;
    state
        .persist_document(
            "profiles",
            response.profile.user_id.to_string(),
            &response.profile,
        )
        .await;
    Ok(Json(response))
}

pub async fn me(State(state): State<AppState>, headers: HeaderMap) -> ApiResult<Json<MeResponse>> {
    let user = require_user(&state, &headers).await?;
    let profile = {
        let store = state.store.read().await;
        store.profile(user.id)?
    };
    Ok(Json(MeResponse { user, profile }))
}

pub async fn update_profile(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<UpdateProfileRequest>,
) -> ApiResult<Json<Profile>> {
    let user = require_user(&state, &headers).await?;
    let profile = {
        let mut store = state.store.write().await;
        store.update_profile(user.id, request)?
    };
    state
        .persist_document("profiles", profile.user_id.to_string(), &profile)
        .await;
    Ok(Json(profile))
}

pub async fn create_campaign(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<CreateCampaignRequest>,
) -> ApiResult<Json<Campaign>> {
    let user = require_user(&state, &headers).await?;
    let campaign = {
        let mut store = state.store.write().await;
        store.create_campaign(user.id, request)?
    };
    state
        .persist_document("campaigns", campaign.id.to_string(), &campaign)
        .await;
    Ok(Json(campaign))
}

pub async fn list_campaigns(
    State(state): State<AppState>,
    headers: HeaderMap,
) -> ApiResult<Json<Vec<Campaign>>> {
    let user = require_user(&state, &headers).await?;
    let campaigns = {
        let store = state.store.read().await;
        store.list_campaigns(user.id)
    };
    Ok(Json(campaigns))
}

pub async fn get_campaign(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(campaign_id): Path<Uuid>,
) -> ApiResult<Json<Campaign>> {
    let user = require_user(&state, &headers).await?;
    let campaign = {
        let store = state.store.read().await;
        store.get_campaign(user.id, campaign_id)?
    };
    Ok(Json(campaign))
}

pub async fn save_campaign(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(campaign_id): Path<Uuid>,
    Json(request): Json<SaveCampaignRequest>,
) -> ApiResult<Json<Campaign>> {
    let user = require_user(&state, &headers).await?;
    let campaign = {
        let mut store = state.store.write().await;
        store.save_campaign(user.id, campaign_id, request)?
    };
    state
        .persist_document("campaigns", campaign.id.to_string(), &campaign)
        .await;
    Ok(Json(campaign))
}

pub async fn create_match_ticket(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<CreateMatchTicketRequest>,
) -> ApiResult<Json<MatchTicket>> {
    let user = require_user(&state, &headers).await?;
    let ticket = {
        let mut store = state.store.write().await;
        store.create_match_ticket(user.id, request)?
    };
    state
        .persist_document("tickets", ticket.id.to_string(), &ticket)
        .await;
    state
        .cache_ephemeral(format!("matchmaking:ticket:{}", ticket.id), &ticket, 1800)
        .await;
    Ok(Json(ticket))
}

pub async fn get_match_ticket(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(ticket_id): Path<Uuid>,
) -> ApiResult<Json<MatchTicket>> {
    let user = require_user(&state, &headers).await?;
    let ticket = {
        let store = state.store.read().await;
        store.get_match_ticket(user.id, ticket_id)?
    };
    Ok(Json(ticket))
}

pub async fn create_lobby(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<CreateLobbyRequest>,
) -> ApiResult<Json<Lobby>> {
    let user = require_user(&state, &headers).await?;
    let lobby = {
        let mut store = state.store.write().await;
        store.create_lobby(user.id, request)?
    };
    state
        .persist_document("lobbies", lobby.id.to_string(), &lobby)
        .await;
    state
        .cache_ephemeral(format!("lobby:{}", lobby.id), &lobby, 3600)
        .await;
    Ok(Json(lobby))
}

pub async fn get_lobby(
    State(state): State<AppState>,
    Path(lobby_id): Path<Uuid>,
) -> ApiResult<Json<Lobby>> {
    let lobby = {
        let store = state.store.read().await;
        store.get_lobby(lobby_id)?
    };
    Ok(Json(lobby))
}

pub async fn join_lobby(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(lobby_id): Path<Uuid>,
    Json(request): Json<JoinLobbyRequest>,
) -> ApiResult<Json<Lobby>> {
    let user = require_user(&state, &headers).await?;
    let lobby = {
        let mut store = state.store.write().await;
        store.join_lobby(user.id, lobby_id, request)?
    };
    state
        .persist_document("lobbies", lobby.id.to_string(), &lobby)
        .await;
    state
        .cache_ephemeral(format!("lobby:{}", lobby.id), &lobby, 3600)
        .await;
    Ok(Json(lobby))
}

pub async fn assign_dedicated_server(
    State(state): State<AppState>,
    headers: HeaderMap,
    Path(lobby_id): Path<Uuid>,
    Json(request): Json<AssignDedicatedServerRequest>,
) -> ApiResult<Json<Lobby>> {
    let user = require_user(&state, &headers).await?;
    let lobby = {
        let mut store = state.store.write().await;
        store.assign_dedicated_server(user.id, lobby_id, request)?
    };
    state
        .persist_document("lobbies", lobby.id.to_string(), &lobby)
        .await;
    state
        .cache_ephemeral(format!("lobby:{}", lobby.id), &lobby, 3600)
        .await;
    Ok(Json(lobby))
}

pub async fn submit_battle_result(
    State(state): State<AppState>,
    Json(request): Json<SubmitBattleResultRequest>,
) -> ApiResult<Json<BattleResult>> {
    let result = {
        let mut store = state.store.write().await;
        store.submit_battle_result(request)?
    };
    state
        .persist_document("battle_results", result.id.to_string(), &result)
        .await;
    Ok(Json(result))
}

#[derive(Debug, Deserialize)]
pub struct RankingQuery {
    pub limit: Option<usize>,
}

pub async fn rankings(
    State(state): State<AppState>,
    Query(query): Query<RankingQuery>,
) -> Json<Vec<RankingEntry>> {
    let rankings = {
        let store = state.store.read().await;
        store.rankings(query.limit.unwrap_or(50))
    };
    Json(rankings)
}

pub async fn record_diplomacy_event(
    State(state): State<AppState>,
    Json(request): Json<RecordDiplomacyEventRequest>,
) -> ApiResult<Json<DiplomacyEvent>> {
    let event = {
        let mut store = state.store.write().await;
        store.record_diplomacy_event(request)?
    };
    state
        .persist_document("diplomacy_events", event.id.to_string(), &event)
        .await;
    Ok(Json(event))
}

pub async fn list_diplomacy_events(
    State(state): State<AppState>,
    Path(campaign_id): Path<Uuid>,
) -> Json<Vec<DiplomacyEvent>> {
    let events = {
        let store = state.store.read().await;
        store.diplomacy_events(campaign_id)
    };
    Json(events)
}

pub async fn publish_mod(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<PublishModRequest>,
) -> ApiResult<Json<ModManifest>> {
    let user = require_user(&state, &headers).await?;
    let manifest = {
        let mut store = state.store.write().await;
        store.publish_mod(user.id, request)?
    };
    state
        .persist_document("mods", manifest.id.to_string(), &manifest)
        .await;
    Ok(Json(manifest))
}

pub async fn list_mods(State(state): State<AppState>) -> Json<Vec<ModManifest>> {
    let mods = {
        let store = state.store.read().await;
        store.mods()
    };
    Json(mods)
}

pub async fn ingest_telemetry(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<TelemetryRequest>,
) -> ApiResult<Json<TelemetryEvent>> {
    let user_id = optional_user(&state, &headers).await.map(|user| user.id);
    let event = {
        let mut store = state.store.write().await;
        store.ingest_telemetry(user_id, request)?
    };
    Ok(Json(event))
}

pub async fn create_replay(
    State(state): State<AppState>,
    headers: HeaderMap,
    Json(request): Json<CreateReplayRequest>,
) -> ApiResult<Json<Replay>> {
    let user = require_user(&state, &headers).await?;
    let replay = {
        let mut store = state.store.write().await;
        store.create_replay(user.id, request)?
    };
    state
        .persist_document("replays", replay.id.to_string(), &replay)
        .await;
    Ok(Json(replay))
}

pub async fn get_replay(
    State(state): State<AppState>,
    Path(replay_id): Path<Uuid>,
) -> ApiResult<Json<Replay>> {
    let replay = {
        let store = state.store.read().await;
        store.get_replay(replay_id)?
    };
    Ok(Json(replay))
}

async fn require_user(state: &AppState, headers: &HeaderMap) -> ApiResult<User> {
    let token = bearer_token(headers)?;
    let store = state.store.read().await;
    store.authenticate(token)
}

async fn optional_user(state: &AppState, headers: &HeaderMap) -> Option<User> {
    let token = bearer_token(headers).ok()?;
    let store = state.store.read().await;
    store.authenticate(token).ok()
}

fn bearer_token(headers: &HeaderMap) -> ApiResult<&str> {
    let value = headers
        .get(axum::http::header::AUTHORIZATION)
        .ok_or(ApiError::Unauthorized)?
        .to_str()
        .map_err(|_| ApiError::Unauthorized)?;
    value
        .strip_prefix("Bearer ")
        .filter(|token| !token.trim().is_empty())
        .ok_or(ApiError::Unauthorized)
}
