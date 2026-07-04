use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use http_body_util::BodyExt;
use serde_json::{Value, json};
use tower::ServiceExt;
use worldleader_online::{AppState, Config, build_router};

fn test_config() -> Config {
    Config {
        bind_addr: "127.0.0.1:0".parse().unwrap(),
        database_url: None,
        redis_url: None,
        cors_origin: "*".to_string(),
        run_migrations: false,
    }
}

async fn json_request(
    app: &mut axum::Router,
    method: &str,
    uri: &str,
    token: Option<&str>,
    body: Value,
) -> (StatusCode, Value) {
    let mut builder = Request::builder()
        .method(method)
        .uri(uri)
        .header("content-type", "application/json");
    if let Some(token) = token {
        builder = builder.header("authorization", format!("Bearer {token}"));
    }
    let response = app
        .oneshot(builder.body(Body::from(body.to_string())).unwrap())
        .await
        .unwrap();
    let status = response.status();
    let bytes = response.into_body().collect().await.unwrap().to_bytes();
    let value = serde_json::from_slice(&bytes).unwrap_or_else(|_| json!({}));
    (status, value)
}

#[tokio::test]
async fn guest_auth_profile_and_campaign_flow() {
    let state = AppState::bootstrap(test_config()).await.unwrap();
    let mut app = build_router(state);

    let (status, auth) = json_request(
        &mut app,
        "POST",
        "/v1/auth/guest",
        None,
        json!({"display_name":"QA Commander"}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    let token = auth["token"].as_str().unwrap();

    let (status, profile) = json_request(
        &mut app,
        "PUT",
        "/v1/profiles/me",
        Some(token),
        json!({"handle":"Gran Colombia","preferred_nation":"CO"}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(profile["preferred_nation"], "CO");

    let (status, campaign) = json_request(
        &mut app,
        "POST",
        "/v1/campaigns",
        Some(token),
        json!({"name":"America Online","initial_state":{"turn":1,"nation":"CO"}}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    let campaign_id = campaign["id"].as_str().unwrap();

    let (status, saved) = json_request(
        &mut app,
        "POST",
        &format!("/v1/campaigns/{campaign_id}/save"),
        Some(token),
        json!({"state_version":2,"cloud_save":{"turn":2,"treasury":65000}}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(saved["state_version"], 2);
}

#[tokio::test]
async fn matchmaking_lobby_and_ranking_flow() {
    let state = AppState::bootstrap(test_config()).await.unwrap();
    let mut app = build_router(state);

    let (_, p1) = json_request(
        &mut app,
        "POST",
        "/v1/auth/guest",
        None,
        json!({"display_name":"Player One"}),
    )
    .await;
    let (_, p2) = json_request(
        &mut app,
        "POST",
        "/v1/auth/guest",
        None,
        json!({"display_name":"Player Two"}),
    )
    .await;
    let t1 = p1["token"].as_str().unwrap();
    let t2 = p2["token"].as_str().unwrap();
    let p1_id = p1["user"]["id"].as_str().unwrap();
    let p2_id = p2["user"]["id"].as_str().unwrap();

    let (status, lobby) = json_request(
        &mut app,
        "POST",
        "/v1/lobbies",
        Some(t1),
        json!({"mode":"pvp_1v1","max_players":2,"nation_iso":"CO"}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    let lobby_id = lobby["id"].as_str().unwrap();
    let (status, assigned) = json_request(
        &mut app,
        "PUT",
        &format!("/v1/lobbies/{lobby_id}/dedicated-server"),
        Some(t1),
        json!({
            "region":"us-east",
            "connect_addr":"203.0.113.10:7777",
            "build_id":"wl-ds-2026-07-03"
        }),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(assigned["status"], "assigned");
    assert_eq!(
        assigned["dedicated_server"]["connect_addr"],
        "203.0.113.10:7777"
    );

    let (status, _ticket1) = json_request(
        &mut app,
        "POST",
        "/v1/matchmaking/tickets",
        Some(t1),
        json!({"mode":"pvp_1v1","preferred_nation":"CO"}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);

    let (status, ticket2) = json_request(
        &mut app,
        "POST",
        "/v1/matchmaking/tickets",
        Some(t2),
        json!({"mode":"pvp_1v1","preferred_nation":"VE"}),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(ticket2["status"], "matched");

    let (status, result) = json_request(
        &mut app,
        "POST",
        "/v1/battle-results",
        None,
        json!({
            "winner_user_id": p1_id,
            "loser_user_id": p2_id,
            "validation_hash": "dedicated-server-signed-result",
            "summary": {"map":"border_skirmish"}
        }),
    )
    .await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(result["accepted"], true);

    let response = app
        .oneshot(
            Request::builder()
                .method("GET")
                .uri("/v1/rankings?limit=2")
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let bytes = response.into_body().collect().await.unwrap().to_bytes();
    let rankings: Value = serde_json::from_slice(&bytes).unwrap();
    assert_eq!(rankings[0]["user_id"], p1_id);
}
