#!/bin/sh

# Usage: sudo ./create-service.sh relative_path

set -e

SERVICE_NAME="climate_service"
REL_EXEC="$1"

if [ -z "$SERVICE_NAME" ] || [ -z "$REL_EXEC" ]; then
    echo "Usage: $0 <relative-path-to-executable>"
    exit 1
fi

# Resolve absolute path of the executable
EXEC_PATH="$(realpath "$REL_EXEC")"

if [ ! -x "$EXEC_PATH" ]; then
    echo "Error: $EXEC_PATH is not executable"
    exit 1
fi

# Directory where the executable lives
EXEC_DIR="$(dirname "$EXEC_PATH")"

SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=${SERVICE_NAME} service
After=network.target

[Service]
Type=simple
ExecStart=${EXEC_PATH}
WorkingDirectory=${EXEC_DIR}
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reexec
systemctl daemon-reload
systemctl enable "${SERVICE_NAME}.service"

echo "Service ${SERVICE_NAME} created."
echo "Exec: ${EXEC_PATH}"
echo "WorkingDirectory: ${EXEC_DIR}"