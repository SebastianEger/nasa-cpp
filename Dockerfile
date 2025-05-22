# [BASE]
FROM alpine:3.21 AS base
RUN apk add --no-cache cmake git gcc g++ libressl-dev ninja

# [DEV-BUILD]
FROM base AS dev-build

# PAHO
RUN git clone https://github.com/eclipse/paho.mqtt.cpp /paho
WORKDIR /paho
RUN git checkout v1.5.2
RUN git submodule init
RUN git submodule update

RUN cmake -Bbuild -GNinja -H. -DCMAKE_INSTALL_PREFIX=/install -DPAHO_WITH_MQTT_C=ON
RUN cmake --build build/ --target install

# SPDLOG
RUN git clone https://github.com/gabime/spdlog /spdlog
WORKDIR /spdlog
RUN git checkout v1.15.2
RUN cmake -Bbuild -GNinja -DCMAKE_INSTALL_PREFIX=/install
RUN cmake --build build/ --target install

# JSON
RUN git clone https://github.com/nlohmann/json /json
WORKDIR /json
RUN git checkout v3.12.0
RUN cmake -Bbuild -GNinja -DCMAKE_INSTALL_PREFIX=/install -DJSON_BuildTests=OFF
RUN cmake --build build/ --target install

# [DEV]
FROM base AS dev
COPY --from=dev-build /install /usr/local

WORKDIR /
CMD ["bash"]

# [NASA-CPP-BUILD]
FROM dev AS nasa-cpp-build

# NASA-CPP
COPY . /nasa-cpp
WORKDIR /nasa-cpp
RUN cmake -Bbuild -GNinja -DCMAKE_INSTALL_PREFIX=/install -DCMAKE_BUILD_TYPE=Release -DBUILD_SAMPLES=OFF -DBUILD_TESTS=OFF
RUN cmake --build build/ --target install

# [FINAL]
FROM nasa-cpp-build AS final
COPY --from=nasa-cpp-build /install /usr/local
COPY --from=nasa-cpp-build /etc/nasa-cpp /etc/nasa-cpp

WORKDIR /
CMD ["tcp_hass_bridge"]
