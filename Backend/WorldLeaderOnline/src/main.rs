use anyhow::Context;
use tokio::net::TcpListener;
use tracing::info;
use worldleader_online::{AppState, Config, build_router, init_tracing};

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    init_tracing();
    let config = Config::from_env();
    let state = AppState::bootstrap(config.clone()).await?;
    let app = build_router(state);
    let listener = TcpListener::bind(config.bind_addr)
        .await
        .with_context(|| format!("binding {}", config.bind_addr))?;

    info!(
        service = env!("CARGO_PKG_NAME"),
        version = env!("CARGO_PKG_VERSION"),
        bind = %config.bind_addr,
        "WorldLeader online backend listening"
    );

    axum::serve(listener, app)
        .with_graceful_shutdown(shutdown_signal())
        .await
        .context("serving axum app")
}

async fn shutdown_signal() {
    let ctrl_c = async {
        tokio::signal::ctrl_c()
            .await
            .expect("failed to install Ctrl+C handler");
    };

    #[cfg(unix)]
    let terminate = async {
        tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())
            .expect("failed to install SIGTERM handler")
            .recv()
            .await;
    };

    #[cfg(not(unix))]
    let terminate = std::future::pending::<()>();

    tokio::select! {
        _ = ctrl_c => {},
        _ = terminate => {},
    }
}
