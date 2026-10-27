# MCP server for the chicken coop door.
#
# Reads coop_home's Arduino Cloud variables (see coop_home/thingProperties.h)
# and exposes them as tools an agent can call.
#
# Environment:
#   ARDUINO_CLIENT_ID, ARDUINO_CLIENT_SECRET  an API key from Arduino Cloud
#   COOP_THING_ID                             the coop_home Thing's ID

# MCP clients launch this from their own working directory, so point Bundler
# at the Gemfile next to this file.
ENV["BUNDLE_GEMFILE"] ||= File.expand_path("Gemfile", __dir__)
require "bundler/setup"

require "json"
require "net/http"
require "time"
require "uri"

require "mcp"

module ArduinoCloud
  API = "https://api2.arduino.cc/iot"
  TIMEOUT_SECONDS = 10

  @token = nil
  @expires_at = Time.at(0)

  module_function

  def access_token
    return @token if @token && Time.now < @expires_at - 60

    response = Net::HTTP.post_form(URI("#{API}/v1/clients/token"), {
      grant_type: "client_credentials",
      client_id: ENV.fetch("ARDUINO_CLIENT_ID"),
      client_secret: ENV.fetch("ARDUINO_CLIENT_SECRET"),
      audience: API,
    })
    response.value # raises unless 2xx
    body = JSON.parse(response.body)
    @expires_at = Time.now + body["expires_in"]
    @token = body["access_token"]
  end

  def get(path)
    send_request(Net::HTTP::Get.new(URI("#{API}/v2/things/#{ENV.fetch("COOP_THING_ID")}#{path}")))
  end

  def post(path, body)
    request = Net::HTTP::Post.new(URI("#{API}#{path}"), "Content-Type" => "application/json")
    request.body = JSON.generate(body)
    send_request(request)
  end

  def send_request(request)
    request["Authorization"] = "Bearer #{access_token}"
    uri = request.uri
    response = Net::HTTP.start(uri.host, uri.port, use_ssl: true,
                               open_timeout: TIMEOUT_SECONDS, read_timeout: TIMEOUT_SECONDS) do |http|
      http.request(request)
    end
    unless response.is_a?(Net::HTTPSuccess)
      # Arduino puts the useful part of an error in `detail`.
      detail = (JSON.parse(response.body)["detail"] rescue nil) || response.message
      raise "#{response.code} #{detail}"
    end
    JSON.parse(response.body)
  end

  def properties
    get("/properties").to_h { |p| [p["variable_name"], p] }
  end

  # The timeseries endpoint only accepts numeric variables, so read the
  # boolean doorOpen through the raw series query instead.
  def raw_series(property_id, from:, to:)
    post("/v2/series/batch_query_raw", {
      requests: [{ q: "property.#{property_id}", from: from.iso8601, to: to.iso8601, sort: "DESC", series_limit: 1000 }],
      resp_version: 1,
    }).fetch("responses").first
  end
end

# Returns the result both as JSON text and as structured content.
def respond(result)
  MCP::Tool::Response.new([{ type: "text", text: JSON.generate(result) }], structured_content: result)
end

class GetCoopDoorStatus < MCP::Tool
  tool_name "get_coop_door_status"
  description "Get whether the chicken coop door is open or closed right now. " \
    "Returns `door` as 'open', 'closed', or 'unknown'. Trust it only when `stale` is false: " \
    "stale means the coop sensor hasn't checked in for over 16 minutes (dead battery, out of range), " \
    "so the last known state may be wrong. If the home unit stops reporting to Arduino Cloud, `stale` " \
    "is also set (based on `home_unit_last_reported_at`). Never guess the door state when it is " \
    "'unknown' or stale."
  input_schema(properties: {})

  def self.call(server_context: nil)
    begin
      props = ArduinoCloud.properties
    rescue => e
      return respond(door: "unknown", stale: true, error: "Can't read Arduino Cloud: #{e.message}")
    end

    door, stale, minutes = props.values_at("doorOpen", "remoteStale", "minutesSinceHeard")
    if door.nil? || door["last_value"].nil?
      return respond(door: "unknown", stale: true, error: "doorOpen has never been reported")
    end

    # minutesSinceHeard updates every minute while coop_home is online, so its
    # timestamp tells us whether the home unit itself is still alive.
    home_reported_at = minutes&.dig("value_updated_at")
    home_offline = home_reported_at.nil? || Time.now - Time.iso8601(home_reported_at) > 5 * 60

    respond(
      door: door["last_value"] ? "open" : "closed",
      stale: !!stale&.dig("last_value") || home_offline,
      minutes_since_coop_checked_in: minutes&.dig("last_value"),
      door_changed_at: door["value_updated_at"],
      home_unit_last_reported_at: home_reported_at,
    )
  end
end

class GetCoopDoorHistory < MCP::Tool
  tool_name "get_coop_door_history"
  description "List door open/close events from the last `hours` hours, newest first. " \
    "Use this for questions like 'did the door close last night?' or 'when was the door last opened?'. " \
    "Times are UTC."
  input_schema(properties: { hours: { type: "number", default: 24 } })

  def self.call(hours: 24, server_context: nil)
    begin
      door = ArduinoCloud.properties.fetch("doorOpen")
      now = Time.now.utc
      series = ArduinoCloud.raw_series(door["id"], from: now - hours * 3600, to: now)
    rescue => e
      return respond(events: [], error: "Can't read Arduino Cloud: #{e.message}")
    end

    events = series.fetch("times", []).zip(series.fetch("values", [])).map do |at, open|
      { door: open ? "open" : "closed", at: at }
    end
    respond(events: events)
  end
end

def coop_server
  MCP::Server.new(name: "chicken-coop", tools: [GetCoopDoorStatus, GetCoopDoorHistory])
end

# Run directly for stdio (local MCP clients); config.ru serves it over HTTP.
if $PROGRAM_NAME == __FILE__
  MCP::Server::Transports::StdioTransport.new(coop_server).open
end
