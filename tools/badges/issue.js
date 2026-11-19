#!/usr/bin/env node
/*
This file is part of YandereGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/

// Badge issuer tool. The private key never enters the repository: it is read
// from badges/issuer.key (or YANDEREGRAM_BADGE_KEY), and only the matching
// public key is compiled into the client.
//
//   node tools/badges/issue.js keygen
//       Creates badges/issuer.key and prints the public key to paste into
//       Telegram/SourceFiles/core/badge_keys.h.
//
//   node tools/badges/issue.js grant <userId> <role>
//       Prints a yg://badge/... link for that user and role.
//
// Roles: supporter | bug | dev | founder.

"use strict";

const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "..", "..");
const KEY_PATH = process.env.YANDEREGRAM_BADGE_KEY
	|| path.join(ROOT, "badges", "issuer.key");

const ROLES = {
	supporter: 1,
	bug: 2,
	dev: 3,
	founder: 4,
};

function fail(message) {
	process.stderr.write(message + "\n");
	process.exit(1);
}

// A private key is the raw 32-byte seed, hex encoded on one line.
function readPrivateKey() {
	if (!fs.existsSync(KEY_PATH)) {
		fail("missing " + KEY_PATH + ", run `issue.js keygen` first");
	}
	const text = fs.readFileSync(KEY_PATH, "utf8").trim();
	const seed = Buffer.from(text, "hex");
	if (seed.length !== 32) {
		fail("bad private key in " + KEY_PATH + ", expected 32 hex bytes");
	}
	const prefix = Buffer.from("302e020100300506032b657004220420", "hex");
	return crypto.createPrivateKey({
		key: Buffer.concat([prefix, seed]),
		format: "der",
		type: "pkcs8",
	});
}

function publicKeyHex(privateKey) {
	const der = crypto.createPublicKey(privateKey).export({
		format: "der",
		type: "spki",
	});
	return der.subarray(der.length - 32).toString("hex");
}

// The signed message binds the role to the user, so one link can't be
// replayed for another account.
function signingMessage(userId, roleCode) {
	return Buffer.from("yg-badge:v1:" + userId + ":" + roleCode, "utf8");
}

function keygen() {
	fs.mkdirSync(path.dirname(KEY_PATH), { recursive: true });
	if (fs.existsSync(KEY_PATH)) {
		fail(KEY_PATH + " already exists, refusing to overwrite");
	}
	const { privateKey } = crypto.generateKeyPairSync("ed25519");
	const der = privateKey.export({ format: "der", type: "pkcs8" });
	const seed = der.subarray(der.length - 32);
	fs.writeFileSync(KEY_PATH, seed.toString("hex") + "\n", { mode: 0o600 });
	process.stdout.write(publicKeyHex(privateKey) + "\n");
}

function grant(userIdText, roleName) {
	const roleCode = ROLES[roleName];
	if (!roleCode) {
		fail("unknown role `" + roleName + "`, use: "
			+ Object.keys(ROLES).join(" | "));
	}
	const userId = Number(userIdText);
	if (!Number.isSafeInteger(userId) || userId <= 0) {
		fail("bad user id `" + userIdText + "`");
	}
	const privateKey = readPrivateKey();
	const signature = crypto.sign(
		null,
		signingMessage(userId, roleCode),
		privateKey);
	const payload = Buffer.alloc(1 + 8 + 64);
	payload.writeUInt8(roleCode, 0);
	payload.writeBigUInt64BE(BigInt(userId), 1);
	signature.copy(payload, 9);
	process.stdout.write(
		"tg://badge/" + payload.toString("base64url") + "\n");
}

const [command, ...args] = process.argv.slice(2);
if (command === "keygen") {
	keygen();
} else if (command === "grant") {
	grant(args[0], args[1]);
} else {
	fail("usage: issue.js keygen | issue.js grant <userId> <role>");
}
