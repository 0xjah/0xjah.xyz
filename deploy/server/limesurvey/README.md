# LimeSurvey (survey.0xjah.xyz)

LimeSurvey 6 for the Hikerji surveys, on the same server as 0xjah.xyz. Edit here first, then copy to the server.

- Public surveys: https://survey.0xjah.xyz (short links `/organizers` and `/hikers`, see the `Caddyfile`).
- Admin: https://nexus.tail2013b3.ts.net, **Tailscale only**. The public site does not route any admin path.

| File | Goes to |
|---|---|
| `limesurvey-pool.conf` | `/etc/php/8.4/fpm/pool.d/limesurvey.conf` |
| `php-fpm-limits.conf` | `/etc/systemd/system/php8.4-fpm.service.d/limits.conf` |
| `limesurvey-backup` | `/usr/local/sbin/limesurvey-backup` (root, 0755) |
| `limesurvey-backup.service`, `.timer` | `/etc/systemd/system/` |

Code lives in `/srv/limesurvey` (root-owned; only `tmp/`, `upload/` and `application/config/` belong to the
`limesurvey` user that php-fpm runs as). MariaDB listens on localhost only. Secrets (database and first admin
password) are in `/root/limesurvey.secrets` (root, 0600) and in `application/config/config.php` (0600), never here.
The Hikerji themes are deployed from the GP-1 repo: `docs/outreach/theme/deploy.sh`.

## Backups

`limesurvey-backup.timer` dumps the database every night at 02:30 UTC to `/var/backups/limesurvey/`
(root only, 0700), gzipped, and deletes dumps older than 14 days. Survey responses are personal data under
Jordan's Personal Data Protection Law (No. 24 of 2023): don't copy dumps off the server unencrypted.

Run one now: `sudo systemctl start limesurvey-backup` (log: `journalctl -u limesurvey-backup`).

### Restore

```sh
ls -lt /var/backups/limesurvey/                       # pick a dump
sudo systemctl stop php8.4-fpm                        # no writes while restoring
sudo mariadb -e 'DROP DATABASE limesurvey; CREATE DATABASE limesurvey CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci'
gunzip -c /var/backups/limesurvey/limesurvey-YYYY-MM-DDTHHMM.sql.gz | sudo mariadb limesurvey
sudo systemctl start php8.4-fpm
```

The database user and its grants survive the drop, so `config.php` keeps working. Uploaded theme files are
not in the dump; redeploy them with the GP-1 theme script.

## Rebuild from scratch

```sh
apt install php8.4-fpm php8.4-mysql php8.4-mbstring php8.4-gd php8.4-xml php8.4-zip php8.4-intl php8.4-curl php8.4-ldap mariadb-server
useradd --system --home-dir /var/lib/limesurvey --create-home --shell /usr/sbin/nologin limesurvey
usermod -aG limesurvey caddy      # Caddy serves published assets from tmp/ and upload/
# LimeSurvey: GitHub tag archive (it ships vendor/ and node_modules/, both needed) into /srv/limesurvey,
# config.php with the DB user, then: runuser -u limesurvey -- php application/commands/console.php install ...
# copy the files above, then:
systemctl daemon-reload && systemctl enable --now php8.4-fpm limesurvey-backup.timer
```
