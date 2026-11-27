// Local test of worker.js: D1 as an in-memory SQLite (node:sqlite), Twitch and
// IGDB as fakes (no network, no keys: the "secrets" here are made up).
// Run: node server/esrb-worker/test/test.mjs   (Node 22 or newer)

import { DatabaseSync } from "node:sqlite";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import assert from "node:assert/strict";

const here = dirname(fileURLToPath(import.meta.url));
// (worker.js exports only its handler, as Cloudflare wants: its pieces are
// exported here from a copy, for the tests below)
import { writeFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
const copy = join(mkdtempSync(join(tmpdir(), "esrb-worker-")), "worker.mjs");
writeFileSync(copy, readFileSync(join(here, "..", "worker.js"), "utf8")
    + "\nexport { letterOf, esrbOf, readGame, pickExact };\n");
const mod = await import(copy);
const worker = mod.default;
const { letterOf, esrbOf, readGame, pickExact } = mod;

// -- D1, as far as the worker uses it ----------------------------------------------------------
function d1() {
    const sql = new DatabaseSync(":memory:");
    sql.exec(readFileSync(join(here, "..", "schema.sql"), "utf8"));
    const stmt = (text, args = []) => ({
        bind: (...a) => stmt(text, a),
        first: async () => sql.prepare(text).get(...args) ?? null,
        all: async () => ({ results: sql.prepare(text).all(...args) }),
        run: async () => { sql.prepare(text).run(...args); return { success: true }; },
        _run: () => sql.prepare(text).run(...args),
    });
    return {
        sql,
        prepare: text => stmt(text),
        batch: async list => { sql.exec("BEGIN"); for (const s of list) s._run(); sql.exec("COMMIT"); return []; },
    };
}

// -- IGDB's games, as its API gives them (expanded fields) -------------------------------------
const ESRB = { id: 1, name: "ESRB" };
const PEGI = { id: 2, name: "PEGI" };
const rating = (org, letter, descs) => ({
    organization: org, rating_category: { rating: letter },
    rating_content_descriptions: descs.map(([d, type]) => ({
        description: d, organization: org.id, description_type: type ? { name: type, slug: type.toLowerCase().replace(/ /g, "-") } : undefined,
    })),
});
const GAMES = [
    { id: 26192, name: "The Last of Us Part II", release_dates: [{ platform: 48, y: 2020 }],
      age_ratings: [rating(ESRB, "M", [["Blood and Gore"], ["Intense Violence"], ["Nudity"], ["Sexual Content"], ["Strong Language"], ["Use of Drugs"]]),
                    rating(PEGI, "18", [["Violence"]])] },
    { id: 1638, name: "GoldenEye 007", release_dates: [{ platform: 4, y: 1997 }],
      age_ratings: [rating(ESRB, "T", [["Animated Violence"], ["Animated Blood"]])] },
    { id: 1070, name: "Super Mario World", release_dates: [{ platform: 19, y: 1990 }, { platform: 19, y: 1991 }],
      age_ratings: [] },
    { id: 285000, name: "Call of Duty: Black Ops 6", release_dates: [{ platform: 167, date: Date.UTC(2024, 9, 25) / 1000 }],
      age_ratings: [rating(ESRB, "M", [["Blood and Gore"], ["Intense Violence"], ["Strong Language"],
                                       ["In-Game Purchases (Includes Random Items)", "Interactive Element"], ["Users Interact", "Interactive Element"]])] },
    // two games with one name, platform and year: never a guess
    { id: 5001, name: "Twins", release_dates: [{ platform: 19, y: 1993 }], age_ratings: [rating(ESRB, "E", [["Mild Violence"]])] },
    { id: 5002, name: "Twins", release_dates: [{ platform: 19, y: 1993 }], age_ratings: [rating(ESRB, "E", [])] },
];

let calls = { token: 0, igdb: 0, bodies: [] };
let tokenNo = 0, expireNext = false, igdbStatus = 200, twitchStatus = 200;
globalThis.fetch = async (url, init) => {
    if (url === "https://id.twitch.tv/oauth2/token") {
        calls.token++;
        const form = new URLSearchParams(String(init.body));
        assert.equal(form.get("grant_type"), "client_credentials");
        if (twitchStatus !== 200)
            return new Response("{}", { status: twitchStatus });
        return Response.json({ access_token: `tok${++tokenNo}`, expires_in: 5000000, token_type: "bearer" });
    }
    assert.equal(url, "https://api.igdb.com/v4/multiquery", "only IGDB's multiquery");
    calls.igdb++;
    calls.bodies.push(init.body);
    assert.equal(init.headers["Client-ID"], "test-id");
    if (expireNext) {
        expireNext = false;
        return Response.json([{ title: "Authorization Failure", status: 401 }], { status: 401 });
    }
    if (igdbStatus !== 200)
        return new Response("[]", { status: igdbStatus });
    const out = [];
    for (const m of String(init.body).matchAll(/query games "(\w+)" \{\n(.*)\n\};/g)) {
        const q = m[2];
        let found = [];
        const ids = /where id = \(([\d,]+)\)/.exec(q);
        const byName = /where name ~ "(.*)" & platforms = \((\d+)\)/.exec(q);
        if (ids)
            found = GAMES.filter(g => ids[1].split(",").map(Number).includes(g.id));
        else if (byName)
            found = GAMES.filter(g => g.name.toLowerCase() === byName[1].toLowerCase()
                && g.release_dates.some(r => r.platform === Number(byName[2])));
        out.push({ name: m[1], result: JSON.parse(JSON.stringify(found)) });
    }
    return Response.json(out);
};

const env = { DB: d1(), TWITCH_CLIENT_ID: "test-id", TWITCH_CLIENT_SECRET: "test-secret" };
let ip = 1;
const ask = async (body, opts = {}) => {
    const r = await worker.fetch(new Request(`https://esrb.example${opts.path || "/v1/esrb"}`, {
        method: opts.method || "POST", body: opts.method === "GET" ? undefined : (typeof body === "string" ? body : JSON.stringify(body)),
        headers: { "cf-connecting-ip": opts.ip || `10.0.0.${ip}` },
    }), opts.env || env);
    return { status: r.status, body: await r.json() };
};

let passed = 0;
const test = async (name, fn) => { await fn(); passed++; console.log("ok  ", name); };

// -- the pieces --------------------------------------------------------------------------------
await test("letters", () => {
    assert.equal(letterOf("e10+"), "E10+");
    assert.equal(letterOf("E10"), "E10+");
    assert.equal(letterOf("ESRB: T"), "T");
    assert.equal(letterOf("Not Rated"), "");
    assert.equal(letterOf("18"), "");
});
await test("ESRB only, interactive apart", () => {
    const r = esrbOf(GAMES[3]);
    assert.equal(r.esrb, "M");
    assert.deepEqual(r.descriptors, ["Blood and Gore", "Intense Violence", "Strong Language"]);
    assert.deepEqual(r.interactive, ["In-Game Purchases (Includes Random Items)", "Users Interact"]);
    assert.deepEqual(esrbOf(GAMES[0]).descriptors.includes("Violence"), false, "no PEGI description");
    assert.equal(esrbOf({ age_ratings: [rating(PEGI, "12", [["Violence"]])] }).esrb, null);
    // two ESRB letters on one game: not sure
    assert.equal(esrbOf({ age_ratings: [rating(ESRB, "T", []), rating(ESRB, "M", [])] }).esrb, null);
    // by name alone (no description type): still interactive
    assert.deepEqual(esrbOf({ age_ratings: [rating(ESRB, "E", [["In-Game Purchases"], ["Mild Fantasy Violence"]])] }).interactive, ["In-Game Purchases"]);
});
await test("what's taken", () => {
    assert.equal(readGame({ name: "A", platform: "n64", year: 1997 }).invalid !== undefined, true, "no letter");
    assert.equal(readGame({ name: 'A" | fields *', platform: "n64", year: 1997, esrb: "T" }).invalid, "name");
    assert.equal(readGame({ name: "A", platform: "nosuch", year: 1997, esrb: "T" }).invalid, "platform");
    assert.equal(readGame({ name: "A", platform: "n64", year: 97, esrb: "T" }).invalid, "year");
    assert.equal(readGame({ igdbId: "1; fields *", esrb: "T" }).invalid, "igdbId");
    assert.deepEqual(readGame({ name: "  GoldenEye   007 ", platform: "N64", year: "1997", esrb: "t" }),
        { esrb: "T", name: "GoldenEye 007", platform: 4, year: 1997, key: "goldeneye 007|4|1997" });
});
await test("exact matches only", () => {
    const want = { name: "Super Mario World", platform: 19, year: 1991 };
    assert.equal(pickExact(GAMES, want).game.id, 1070);
    assert.equal(pickExact(GAMES, { ...want, year: 1992 }).reason, "none");
    assert.equal(pickExact(GAMES, { ...want, platform: 4 }).reason, "none");
    assert.equal(pickExact(GAMES, { name: "Super Mario", platform: 19, year: 1991 }).reason, "none");
    assert.equal(pickExact(GAMES, { name: "Twins", platform: 19, year: 1993 }).reason, "several");
});

// -- the endpoint ------------------------------------------------------------------------------
const FOUR = [
    { name: "The Last of Us Part II", platform: "ps4", year: 2020, esrb: "M" },
    { name: "GoldenEye 007", platform: "n64", year: 1997, esrb: "T" },
    { name: "Super Mario World", platform: "snes", year: 1991, esrb: "E" },
    { name: "Call of Duty: Black Ops 6", platform: "ps5", year: 2024, esrb: "M" },
];
await test("the four games", async () => {
    const { status, body } = await ask({ games: FOUR });
    assert.equal(status, 200);
    const [tlou, ge, smw, cod] = body.results;
    assert.equal(tlou.status, "ok");
    assert.equal(tlou.igdbId, 26192);
    assert.ok(tlou.descriptors.includes("Blood and Gore"));
    assert.deepEqual(tlou.interactive, []);
    assert.equal(ge.status, "ok");
    assert.deepEqual(ge.descriptors, ["Animated Violence", "Animated Blood"]);
    assert.deepEqual(smw, { status: "none", igdbId: 1070 });
    assert.deepEqual(cod.interactive, ["In-Game Purchases (Includes Random Items)", "Users Interact"]);
    assert.equal(calls.token, 1);
    assert.equal(calls.igdb, 1, "one multiquery for the batch");
});
await test("asked again: from D1, IGDB not asked", async () => {
    const before = calls.igdb;
    const { body } = await ask({ games: FOUR });
    assert.equal(body.results[0].status, "ok");
    assert.equal(body.results[2].status, "none");
    assert.equal(calls.igdb, before);
    // by id, of a game looked up by name: D1 too
    const byId = await ask({ games: [{ igdbId: 1638, esrb: "T" }] });
    assert.equal(byId.body.results[0].status, "ok");
    assert.equal(calls.igdb, before);
});
await test("letter not the same: nothing", async () => {
    const { body } = await ask({ games: [{ igdbId: 26192, esrb: "T" }] });
    assert.deepEqual(body.results[0], { status: "mismatch", igdbId: 26192, igdbEsrb: "M" });
});
await test("not found, several, wrong year: none, kept", async () => {
    const before = calls.igdb;
    const games = [
        { name: "No Such Game", platform: "snes", year: 1991, esrb: "E" },
        { name: "Twins", platform: "snes", year: 1993, esrb: "E" },
        { name: "GoldenEye 007", platform: "n64", year: 1998, esrb: "T" },
        { igdbId: 999999, esrb: "E" },
    ];
    const first = await ask({ games });
    assert.deepEqual(first.body.results.map(r => r.status), ["none", "none", "none", "none"]);
    assert.equal(calls.igdb, before + 1);
    const again = await ask({ games });
    assert.deepEqual(again.body.results.map(r => r.status), ["none", "none", "none", "none"]);
    assert.equal(calls.igdb, before + 1, "checked, none found: kept");
    const row = env.DB.sql.prepare("SELECT reason FROM lookups WHERE lookup_key = 'twins|19|1993'").get();
    assert.equal(row.reason, "several");
});
await test("only age fields asked, only ESRB's given", async () => {
    for (const b of calls.bodies) {
        assert.ok(!/fields \*/.test(b));
        assert.ok(!/age_ratings\.(category|rating|content_descriptions)\b/.test(b), "no deprecated fields");
    }
    const { body } = await ask({ games: [{ igdbId: 26192, esrb: "M" }] });
    assert.deepEqual(Object.keys(body.results[0]).sort(), ["descriptors", "esrb", "igdbId", "interactive", "status"]);
});
await test("token ran out: a new one, asked again once", async () => {
    const t = calls.token;
    expireNext = true;
    GAMES.push({ id: 7000, name: "Expiry", release_dates: [], age_ratings: [rating(ESRB, "E", [["Comic Mischief"]])] });
    const { body } = await ask({ games: [{ igdbId: 7000, esrb: "E" }] });
    assert.equal(body.results[0].status, "ok");
    assert.equal(calls.token, t + 1);
    assert.equal(env.DB.sql.prepare("SELECT access_token FROM token").get().access_token, `tok${tokenNo}`);
});
await test("IGDB busy: later, not kept", async () => {
    igdbStatus = 429;
    GAMES.push({ id: 7001, name: "Busy", release_dates: [], age_ratings: [rating(ESRB, "E", [])] });
    const { status, body } = await ask({ games: [{ igdbId: 7001, esrb: "E" }, { igdbId: 26192, esrb: "M" }] });
    assert.equal(status, 200);
    assert.equal(body.results[0].status, "later");
    assert.equal(body.results[1].status, "ok", "what's known still answered");
    assert.ok(body.retryAfter >= 1);
    igdbStatus = 200;
    assert.equal((await ask({ games: [{ igdbId: 7001, esrb: "E" }] })).body.results[0].status, "ok");
});
await test("4 a second, 8 open", async () => {
    // 4 taken this second, and the next: later
    const s = Math.floor(Date.now() / 1000);
    env.DB.sql.prepare("INSERT OR REPLACE INTO igdb_seconds VALUES (?, 4), (?, 4), (?, 4)").run(s, s + 1, s + 2);
    const a = await ask({ games: [{ igdbId: 8001, esrb: "E" }] });
    assert.equal(a.body.results[0].status, "later");
    env.DB.sql.exec("DELETE FROM igdb_seconds");
    const open = env.DB.sql.prepare("INSERT INTO igdb_open VALUES (?, ?)");
    for (let i = 0; i < 8; i++)
        open.run(`x${i}`, Date.now());
    const b = await ask({ games: [{ igdbId: 8001, esrb: "E" }] });
    assert.equal(b.body.results[0].status, "later");
    assert.match(b.body.results[0].reason, /8 requests open/);
    env.DB.sql.exec("DELETE FROM igdb_open");
    assert.equal((await ask({ games: [{ igdbId: 8001, esrb: "E" }] })).body.results[0].status, "none");
});
await test("one address: 30 a minute", async () => {
    let last;
    for (let i = 0; i < 31; i++)
        last = await ask({ games: [{ igdbId: 26192, esrb: "M" }] }, { ip: "192.0.2.9" });
    assert.equal(last.status, 429);
    assert.equal((await ask({ games: [{ igdbId: 26192, esrb: "M" }] }, { ip: "192.0.2.10" })).status, 200);
});
await test("not a proxy: one path, POST, at most 10", async () => {
    assert.equal((await ask({ games: FOUR }, { path: "/v4/games" })).status, 404);
    assert.equal((await ask(null, { method: "GET" })).status, 405);
    assert.equal((await ask("fields *;")).status, 400);
    assert.equal((await ask({ games: Array(11).fill({ igdbId: 1, esrb: "E" }) })).status, 400);
    assert.equal((await ask({ games: [] })).status, 400);
    const { body } = await ask({ games: [{ name: 'x"; fields *; where id > 0', platform: "n64", year: 1997, esrb: "T" }] });
    assert.equal(body.results[0].status, "invalid");
});
await test("no keys, no D1, D1 failing: later", async () => {
    const noKeys = await ask({ games: [{ igdbId: 9001, esrb: "E" }] }, { env: { DB: env.DB } });
    assert.equal(noKeys.body.results[0].status, "later");
    assert.match(noKeys.body.results[0].reason, /TWITCH_CLIENT_ID/);
    assert.equal((await ask({ games: FOUR }, { env: {} })).status, 503);
    const broken = { ...env, DB: { prepare: () => { throw new Error("D1_ERROR: daily limit"); } } };
    const r = await ask({ games: FOUR }, { env: broken });
    assert.equal(r.status, 503);
    assert.equal(r.body.error, "later");
});
await test("Twitch refuses the keys: later, no key in the answer", async () => {
    twitchStatus = 400;
    env.DB.sql.exec("DELETE FROM token");
    const r = await ask({ games: [{ igdbId: 9002, esrb: "E" }] });
    assert.equal(r.body.results[0].status, "later");
    assert.ok(!JSON.stringify(r.body).includes("test-secret"));
    twitchStatus = 200;
});

console.log(`\n${passed} passed`);
