@echo off
REM Build the Docker image
docker build -t adaptive-autosar .

REM Run the container with workspace folder mounted to /workspace
docker run -it --rm -v "%cd%\workspace:/workspace" --ipc=host adaptive-autosar