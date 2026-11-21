# Server config

Live copies of what runs on the server (Debian 13, Caddy in front, SSH over Tailscale only).
Edit here first, then copy to the server.

| File | Goes to |
|---|---|
| `Caddyfile` | `/etc/caddy/Caddyfile` |
| `0xjah.xyz.service` | `/etc/systemd/system/0xjah.xyz.service` |
| `nftables.conf` | `/etc/nftables.conf` (never `flush ruleset`: it wipes Tailscale's rules) |
| `sshd-hardening.conf` | `/etc/ssh/sshd_config.d/00-hardening.conf` |
| `sysctl-hardening.conf` | `/etc/sysctl.d/90-hardening.conf` |
| `apt-auto-reboot.conf` | `/etc/apt/apt.conf.d/52auto-reboot` |
| `deploy-sudoers` | `/etc/sudoers.d/deploy` |

Secrets live in `/etc/0xjah.xyz.env` (root, 0600), never in this repo.

## Rebuild from scratch

```sh
apt install gcc make libc6-dev libcurl4-openssl-dev libssl-dev libmd4c-dev libmd4c-html0-dev rsync
# Caddy: official repo, see https://caddyserver.com/docs/install#debian-ubuntu-raspbian
useradd --system --home-dir /opt/0xjah.xyz --shell /usr/sbin/nologin xyz
useradd --create-home --shell /bin/bash deploy
install -d -o deploy -g deploy /opt/0xjah.xyz
# copy the files above, put the GitHub deploy public key in ~deploy/.ssh/authorized_keys, then:
systemctl daemon-reload && systemctl enable --now 0xjah.xyz caddy nftables
```

Pushing to `main` deploys through `.github/workflows/deploy.yml`.
