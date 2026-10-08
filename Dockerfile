# Stage 1 - compile. The gcc image exceeds 1.5 GB
# and none of it reaches the final image.
FROM gcc:14-bookworm AS build
WORKDIR /src
RUN apt-get update \
 && apt-get install -y --no-install-recommends cmake git \
 && rm -rf /var/lib/apt/lists/*
COPY CMakeLists.txt .
COPY src/ ./src/
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

# Stage 2 - runtime. A bare Debian plus the statically linked binary.
FROM debian:bookworm-slim
LABEL org.opencontainers.image.title="MAP proiect" \
      org.opencontainers.image.authors="Nume Prenume <email@student.upt.ro>" \
      org.opencontainers.image.source="https://github.com/utilizator/repo"

ARG COMMIT=dev
ARG BUILT_AT=unknown
ENV APP_COMMIT=$COMMIT APP_BUILT_AT=$BUILT_AT

RUN useradd --uid 10001 --create-home app
COPY --from=build /src/build/server /usr/local/bin/server
USER app
EXPOSE 8080
CMD ["/usr/local/bin/server"]
