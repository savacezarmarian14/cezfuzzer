FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y libstdc++6 procps && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
CMD ["/app/binary"]















