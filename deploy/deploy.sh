#!/bin/sh
# Deploys the working tree (tracked files only) to the server over Tailscale.
# Same steps as .github/workflows/deploy.yml, for when Actions is unavailable.
set -eu

HOST=ahmad@100.110.85.52
cd "$(dirname "$0")/.."

# Files are written as the deploy user so they stay owned like CI-deployed ones.
git ls-files -z | rsync -az --from0 --files-from=- --rsync-path="sudo -u deploy rsync" ./ "$HOST:/opt/0xjah.xyz/"
ssh "$HOST" 'cd /opt/0xjah.xyz/server && sudo -u deploy make build && sudo systemctl restart 0xjah.xyz'

sleep 1
curl -fsS --retry 5 --retry-delay 2 --retry-all-errors https://0xjah.xyz/health && echo " deployed"
