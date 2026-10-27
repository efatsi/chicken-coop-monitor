# HTTP entry point, for hosting on Fly.io: bundle exec puma config.ru
require_relative "coop_mcp"

transport = MCP::Server::Transports::StreamableHTTPTransport.new(
  coop_server,
  stateless: true,
  enable_json_response: true,
  serve_subscriptions_listen: false,
  # Fly sets FLY_APP_NAME, and the app is served at <app>.fly.dev.
  allowed_hosts: ENV["FLY_APP_NAME"] && ["#{ENV["FLY_APP_NAME"]}.fly.dev"],
)

map("/mcp") { run transport }
