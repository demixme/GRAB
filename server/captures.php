<?php
declare(strict_types=1);
require __DIR__ . '/db.php';

// POST /grab/v1/captures — store one raw MIDI capture (what a device emitted), for
// reverse-engineering unsupported devices. Opt-in; see CapturePanel / DEVICE-PROFILES.md.

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    grab_json(405, ['error' => 'POST only']);
}

grab_rate_limit(); // per-IP speed bump (defined in db.php)

$raw = file_get_contents('php://input', false, null, 0, 4 * 1024 * 1024); // 4 MB cap
$p   = json_decode((string) $raw, true);
if (!is_array($p) || (int) ($p['schemaVersion'] ?? 0) !== 1
    || ($p['kind'] ?? '') !== 'midi-capture') {
    grab_json(400, ['error' => 'bad or unsupported capture']);
}

$cap = is_array($p['capture'] ?? null) ? $p['capture'] : [];

try {
    $pdo = grab_db();
    $ins = $pdo->prepare(
        'INSERT INTO grab_captures
         (received_at, os, port_name, message_count, tool_version, notes, raw_json)
         VALUES (UTC_TIMESTAMP(), ?, ?, ?, ?, ?, ?)'
    );
    $ins->execute([
        grab_str($p['platform']['os'] ?? '', 64),
        grab_str($cap['port'] ?? '', 128),
        isset($cap['messageCount']) ? (int) $cap['messageCount'] : null,
        grab_str($p['tool']['version'] ?? '', 16),
        grab_str($p['notes'] ?? '', 2000),
        $raw,
    ]);
    grab_json(201, ['message' => 'Thank you', 'id' => (int) $pdo->lastInsertId()]);
} catch (Throwable $e) {
    error_log('grab captures.php: ' . $e->getMessage());
    grab_json(500, ['error' => 'server error']);
}
