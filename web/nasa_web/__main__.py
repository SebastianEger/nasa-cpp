import os

import uvicorn


def main() -> None:
    uvicorn.run(
        "nasa_web.app:app",
        host=os.environ.get("HTTP_HOST", "0.0.0.0"),
        port=int(os.environ.get("HTTP_PORT", "8080")),
        log_level=os.environ.get("LOG_LEVEL", "info"),
    )


if __name__ == "__main__":
    main()
