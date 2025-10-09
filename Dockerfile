ARG BUILD_SHARED=ON
ARG BUILD_STATIC=OFF

# [BASE]
FROM alpine:3.21 AS base

# [DEV]
FROM base AS dev
ARG BUILD_SHARED
ARG BUILD_STATIC
RUN apk add --no-cache cmake git ninja gcc g++

# PAHO
RUN git clone https://github.com/eclipse/paho.mqtt.cpp /paho
WORKDIR /paho
RUN git checkout v1.5.2
RUN git submodule init
RUN git submodule update

RUN cmake -Bbuild -GNinja -H. -DPAHO_WITH_MQTT_C=ON \
    -DPAHO_BUILD_SHARED=${BUILD_SHARED} -DPAHO_BUILD_STATIC=${BUILD_STATIC} -DPAHO_WITH_SSL=OFF
RUN cmake --build build/ --target install

# SPDLOG
RUN git clone https://github.com/gabime/spdlog /spdlog
WORKDIR /spdlog
RUN git checkout v1.15.2
RUN cmake -Bbuild -GNinja -DBUILD_SHARED_LIBS=${BUILD_SHARED}
RUN cmake --build build/ --target install

# JSON
RUN git clone https://github.com/nlohmann/json /json
WORKDIR /json
RUN git checkout v3.12.0
RUN cmake -Bbuild -GNinja -DJSON_BuildTests=OFF -DBUILD_SHARED_LIBS=${BUILD_SHARED}
RUN cmake --build build/ --target install

WORKDIR /
CMD ["bash"]

# [BUILD]
FROM dev AS build
ARG BUILD_SHARED

# NASA-CPP
COPY . /nasa-cpp
WORKDIR /nasa-cpp
RUN cmake -Bbuild -GNinja -DCMAKE_INSTALL_PREFIX=/install -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=${BUILD_SHARED} \
    -DNASACPP_BUILD_SAMPLES=OFF -DNASACPP_BUILD_TESTS=OFF
RUN cmake --build build/ --target install

# [STATIC]
FROM base AS image-static
COPY --from=build /install /usr/local
COPY --from=build /etc/nasa-cpp /etc/nasa-cpp

WORKDIR /
CMD ["tcp_hass_bridge"]

# [SHARED]
FROM dev AS image-shared
COPY --from=build /install /usr/local
COPY --from=build /etc/nasa-cpp /etc/nasa-cpp

WORKDIR /
CMD ["tcp_hass_bridge"]
