FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    cmake \
    libboost-all-dev \
    libasio-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN if [ ! -f crow_all.h ]; then \
    wget -q https://github.com/CrowCpp/Crow/releases/latest/download/crow_all.h; \
    fi

RUN g++ -std=c++20 -O2 -pthread \
    main.cpp \
    MySocket/MySocket.cpp \
    PktDef/PktDef.cpp \
    -I/app \
    -I/app/MySocket \
    -I/app/PktDef \
    -o coil_server

EXPOSE 3540

CMD ["./coil_server"]