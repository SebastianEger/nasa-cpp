# nasa-web: standalone dashboard + history recorder for tcp_hass_bridge
# Build context is the repository root (the demo stage needs msg/).
FROM python:3.12-slim AS base

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

WORKDIR /app
COPY web/requirements.txt .
RUN pip install -r requirements.txt

COPY web/nasa_web ./nasa_web

# [DEMO] image with the simulator and the message lists, used by the "demo" compose profile
FROM base AS demo
COPY web/tools ./tools
COPY msg /msg
ENTRYPOINT ["python", "tools/simulate.py", "--msg-dir", "/msg/AE08BXYDGG-MIM03EN"]

# [RUNTIME]
FROM base AS runtime
RUN useradd --system --uid 1000 --home /app nasa && mkdir -p /data && chown nasa /data
USER nasa

ENV HTTP_HOST=0.0.0.0 \
    HTTP_PORT=8080 \
    DB_PATH=/data/nasa_web.sqlite3

VOLUME ["/data"]
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD python -c "import os,urllib.request; urllib.request.urlopen(f'http://127.0.0.1:{os.environ[\"HTTP_PORT\"]}/api/status', timeout=4)"

CMD ["python", "-m", "nasa_web"]
