FROM ruby:3.3-slim

WORKDIR /app
COPY Gemfile Gemfile.lock ./
RUN apt-get update && apt-get install -y --no-install-recommends build-essential && rm -rf /var/lib/apt/lists/*
RUN bundle config set without development && bundle install
COPY coop_mcp.rb config.ru ./

EXPOSE 8080
CMD ["bundle", "exec", "puma", "-b", "tcp://0.0.0.0:8080", "config.ru"]
