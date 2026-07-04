use std::{env, net::SocketAddr};

#[derive(Clone, Debug)]
pub struct Config {
    pub bind_addr: SocketAddr,
    pub database_url: Option<String>,
    pub redis_url: Option<String>,
    pub cors_origin: String,
    pub run_migrations: bool,
}

impl Config {
    pub fn from_env() -> Self {
        let bind_addr = env::var("WL_ONLINE_BIND")
            .ok()
            .and_then(|value| value.parse::<SocketAddr>().ok())
            .unwrap_or_else(|| SocketAddr::from(([127, 0, 0, 1], 8787)));

        Self {
            bind_addr,
            database_url: env::var("DATABASE_URL").ok().filter(|s| !s.is_empty()),
            redis_url: env::var("REDIS_URL").ok().filter(|s| !s.is_empty()),
            cors_origin: env::var("WL_ONLINE_CORS_ORIGIN").unwrap_or_else(|_| "*".to_string()),
            run_migrations: env::var("WL_ONLINE_RUN_MIGRATIONS")
                .map(|v| matches!(v.as_str(), "1" | "true" | "TRUE" | "yes" | "YES"))
                .unwrap_or(true),
        }
    }
}
