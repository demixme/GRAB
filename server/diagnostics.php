<?php
declare(strict_types=1);
require __DIR__ . '/db.php';

// POST /grab/v1/diagnostics — store one anonymized connection diagnostic (the MIDI
// ports GRAB saw + how the connect attempt ended). See Diagnostics.h / DEVICE-PROFILES.md.

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    grab_json(405, ['error' => 'POST only']);
}

grab_rate_limit(); // per-IP speed bump (defined in db.php)

$raw = file_get_contents('php://input', false, null, 0, 1024 * 1024); // 1 MB cap
$p   = json_decode((string) $raw, true);
if (!is_array($p) || (int) ($p['schemaVersion'] ?? 0) !== 1
    || ($p['kind'] ?? '') !== 'connection-diagnostic') {
    grab_json(400, ['error' => 'bad or unsupported diagnostic']);
}

$session = is_array($p['session'] ?? null) ? $p['session'] : [];
$device  = is_array($p['device'] ?? null)  ? $p['device']  : [];

try {
    $pdo = grab_db();
    $ins = $pdo->prepare(
        'INSERT INTO grab_diagnostics
         (received_at, os, req_transport, outcome, rise_grand, type, tool_version, notes, raw_json)
         VALUES (UTC_TIMESTAMP(), ?, ?, ?, ?, ?, ?, ?, ?)'
    );
    $ins->execute([
        grab_str($p['platform']['os'] ?? '', 64),
        grab_str($session['requestedTransport'] ?? '', 8),
        grab_str($session['outcome'] ?? '', 24),
        !empty($session['looksLikeRiseGrand']) ? 1 : 0,
        grab_str($device['type'] ?? '', 64),
        grab_str($p['tool']['version'] ?? '', 16),
        grab_str($p['notes'] ?? '', 2000),
        $raw,
    ]);
    grab_json(201, ['message' => 'Thank you', 'id' => (int) $pdo->lastInsertId()]);
} catch (Throwable $e) {
    error_log('grab diagnostics.php: ' . $e->getMessage());
    grab_json(500, ['error' => 'server error']);
}
