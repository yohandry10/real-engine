mod config;
mod error;
mod handlers;
mod models;
mod state;
mod store;

pub use config::Config;
pub use state::AppState;

use axum::{
    Router,
    http::{HeaderValue, Method},
    routing::{get, post, put},
};
use tower_http::{cors::CorsLayer, trace::TraceLayer};

pub fn init_tracing() {
    let filter = tracing_subscriber::EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| "worldleader_online=info,tower_http=info".into());
    tracing_subscriber::fmt().with_env_filter(filter).init();
}

pub fn build_router(state: AppState) -> Router {
    let cors = CorsLayer::new()
        .allow_origin(
            state
                .config
                .cors_origin
                .parse::<HeaderValue>()
                .unwrap_or_else(|_| HeaderValue::from_static("*")),
        )
        .allow_methods([Method::GET, Method::POST, Method::PUT, Method::DELETE])
        .allow_headers(tower_http::cors::Any);

    Router::new()
        .route("/healthz", get(handlers::healthz))
        .route("/readyz", get(handlers::readyz))
        .route("/v1/auth/guest", post(handlers::auth_guest))
        .route("/v1/me", get(handlers::me))
        .route("/v1/profiles/me", put(handlers::update_profile))
        .route(
            "/v1/campaigns",
            get(handlers::list_campaigns).post(handlers::create_campaign),
        )
        .route("/v1/campaigns/{campaign_id}", get(handlers::get_campaign))
        .route(
            "/v1/campaigns/{campaign_id}/save",
            post(handlers::save_campaign),
        )
        .route(
            "/v1/matchmaking/tickets",
            post(handlers::create_match_ticket),
        )
        .route(
            "/v1/matchmaking/tickets/{ticket_id}",
            get(handlers::get_match_ticket),
        )
        .route("/v1/lobbies", post(handlers::create_lobby))
        .route("/v1/lobbies/{lobby_id}", get(handlers::get_lobby))
        .route("/v1/lobbies/{lobby_id}/join", post(handlers::join_lobby))
        .route(
            "/v1/lobbies/{lobby_id}/dedicated-server",
            put(handlers::assign_dedicated_server),
        )
        .route("/v1/battle-results", post(handlers::submit_battle_result))
        .route("/v1/rankings", get(handlers::rankings))
        .route(
            "/v1/diplomacy/events",
            post(handlers::record_diplomacy_event),
        )
        .route(
            "/v1/campaigns/{campaign_id}/diplomacy",
            get(handlers::list_diplomacy_events),
        )
        .route(
            "/v1/mods",
            get(handlers::list_mods).post(handlers::publish_mod),
        )
        .route("/v1/telemetry", post(handlers::ingest_telemetry))
        .route("/v1/replays", post(handlers::create_replay))
        .route("/v1/replays/{replay_id}", get(handlers::get_replay))
        .layer(cors)
        .layer(TraceLayer::new_for_http())
        .with_state(state)
}
