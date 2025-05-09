# Use an official lightweight C build image
FROM debian:stable-slim

# Install build dependencies
RUN apt-get update && \
    apt-get install -y build-essential libncurses5-dev libncursesw5-dev git && \
    rm -rf /var/lib/apt/lists/*

# Set workdir
WORKDIR /app

# Copy source code
COPY . .

# Build the project
RUN make clean && make

# Expose the default server port
EXPOSE 8888

# Default command (can be overridden)
CMD ["/app/build/my_dispute_server"] 