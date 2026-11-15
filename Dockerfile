FROM gcc:11.3 AS build

RUN sed -i '/security/d' /etc/apt/sources.list && \
    apt update && \
    apt install -y \
      catch2 \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:22.04 AS run

RUN groupadd -r www && useradd -r -g www www

COPY --from=build --chown=www:www /app/build/bin/game_server /app/
COPY --chown=www:www ./data /app/data
COPY --chown=www:www ./static /app/static

USER www

ENTRYPOINT ["/app/game_server", "-c", "/app/data/config.json", "-w", "/app/static"]
