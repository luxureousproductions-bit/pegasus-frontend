# Pegasus X ESRB worker

A small server that gives ESRB **content descriptors** ("Blood and Gore",
"Strong Language"...) for games that already have an ESRB letter, from IGDB.
A Cloudflare Worker plus a D1 database, both on the free tier. Pegasus X and
the XBSX theme don't use it yet.

- One endpoint: `POST /v1/esrb`, up to 10 games a request.
- Each game: `igdbId` if known, otherwise `name` + `platform` (an ES-DE system
  name such as `snes`, or an IGDB platform id) + `year`; and always `esrb`, the
  letter the game already has (EC, E, E10+, T, M, AO, RP).
- Answers only from the ESRB rating on IGDB (its current `age_ratings` fields,
  ESRB's organization only), and only when IGDB's ESRB letter is the one sent.
  A name lookup needs exactly one game with that name, platform and year;
  anything else answers `none`. It never guesses.
- Interactive elements ("In-Game Purchases", "Users Interact"...) come in a
  separate `interactive` list, only when IGDB lists them on the game's ESRB
  rating.
- Every answer is kept in D1, "checked, none found" too: a game is asked of
  IGDB once.
- Not a proxy to IGDB: fixed lookups only, only the ESRB descriptions go out.
- Limits: IGDB's 4 requests a second and 8 at once (shared by every copy of
  the worker, in D1), 30 requests a minute per address. Over a limit, or when
  Cloudflare's free tier is used up, a game answers `later` with `retryAfter`
  (seconds).

Files: `worker.js` (all the code, pasted into Cloudflare), `schema.sql` (the
database, pasted into D1 once), `test.json` (the test below), `test/test.mjs`
(a local test with a fake IGDB: `node server/esrb-worker/test/test.mjs`).

The Twitch keys are **never** in this repo: you enter them in Cloudflare as
secrets.

## Deploy (from a phone, in the Cloudflare dashboard)

You need a Cloudflare account (free) and a Twitch app (free).

**1. Twitch keys.** At dev.twitch.tv/console (log in with Twitch, turn on
two-factor if it asks): Applications > Register Your Application. Name:
anything (e.g. "Pegasus X ESRB"). OAuth Redirect URL: `http://localhost`.
Category: Application Integration. Client type: Confidential. Create, then
Manage: copy the **Client ID**, press **New Secret** and copy the **Client
Secret**. Keep them for step 5 only.

**2. The database.** dash.cloudflare.com > Storage & Databases > D1 SQL
Database > Create. Name: `pegasusx-esrb`. Create. Open it > **Console**: paste
all of `schema.sql` and run it. (Tables appear under Tables: games, lookups,
token, igdb_seconds, igdb_open, ip_minutes.)

**3. The worker.** Compute (Workers) > Workers & Pages > Create > Start with
Hello World > name it `pegasusx-esrb` > Deploy. Then **Edit code**: delete
everything in `worker.js` there, paste all of this folder's `worker.js`, and
press **Deploy**.

**4. Bind the database.** The worker > Settings > Bindings > Add > D1
database. Variable name: `DB` (exactly). Database: `pegasusx-esrb`. Deploy.

**5. The keys.** The worker > Settings > Variables and Secrets > Add. Type:
**Secret**. Name `TWITCH_CLIENT_ID`, value: the Client ID. Add another: Secret,
`TWITCH_CLIENT_SECRET`, the Client Secret. Deploy.

**6. Its address.** The worker's page shows it, like
`https://pegasusx-esrb.<your-name>.workers.dev`. The endpoint is that plus
`/v1/esrb`. Opening it in a browser answers `{"error":"POST only"}`: it's up.

(Changing the code later: Edit code, paste, Deploy. The database, binding and
secrets stay.)

## Test: four games

The request is `test.json`: The Last of Us Part II (ps4, 2020, M), GoldenEye
007 (n64, 1997, T), Super Mario World (snes, 1991, E), Call of Duty: Black
Ops 6 (ps5, 2024, M).

From any computer, or a phone with Termux:

```sh
curl -s -X POST https://pegasusx-esrb.<your-name>.workers.dev/v1/esrb \
  -H 'content-type: application/json' --data @test.json
```

(No curl at hand: the worker's Edit code page has a preview on the side;
on its HTTP tab choose POST, paste `test.json` as the body, path `/v1/esrb`.)

The answer has one result per game, in order. What each status means:

- `ok`: found, the same ESRB letter: `descriptors` and `interactive`.
- `none`: IGDB has no ESRB rating for it, or no single game with exactly that
  name, platform and year.
- `mismatch`: IGDB's ESRB letter (`igdbEsrb`) isn't the one sent: nothing
  given.
- `later`: busy, or the keys/binding aren't right (`reason`): ask again after
  `retryAfter` seconds.
- `invalid`: the entry isn't one it takes (`reason`).

**Interactive elements:** look at Call of Duty: Black Ops 6's result. A
non-empty `interactive` list (e.g. "In-Game Purchases (Includes Random
Items)", "Users Interact") means IGDB does mark them on ESRB ratings. An empty
one means IGDB doesn't list them for that game. Super Mario World answering
`none` is expected if IGDB has no ESRB rating for the SNES original (it came
out before the ESRB).

A second run of the same request answers from D1 (IGDB isn't asked). To ask
IGDB again, run in the D1 Console: `DELETE FROM games; DELETE FROM lookups;`.

## If something's wrong

- `later` with "TWITCH_CLIENT_ID / TWITCH_CLIENT_SECRET not set": step 5.
- `later` with "Twitch: 400/403 (are the keys right?)": a new secret in
  step 1, then step 5 again.
- 503 with "the D1 database isn't bound as DB": step 4 (the name must be `DB`).
- 503 "busy (Cloudflare's free tier?)": D1 or the worker hit a free-tier
  limit, or `schema.sql` wasn't run (step 2). The worker's Logs tab says which.
- Over the free tier's daily requests, Cloudflare itself answers (an error
  page, not JSON): a client should take any 429, 5xx or non-JSON answer as
  "try again later".
