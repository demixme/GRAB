<?php
declare(strict_types=1);
require __DIR__ . '/db.php';

// GET /grab/v1/export — MAINTAINER-ONLY export of the raw contributed profiles,
// so GRAB development for other ROLI devices can work from real data.
//
// Auth: an `exportToken` from the server config (never shipped in the app, unlike
// the open write endpoint). Fails CLOSED — until a token is configured, export is
// disabled. See server/README.md.
//
// Query params:
//   token=   the export token (or send it as the X-Grab-Token header — preferred,
//            since a URL token can land in server access logs)
//   kind=    profiles (default) | diagnostics   (which collection to export)
//   family=  optional filter (profiles only; e.g. BLOCKS)
//   type=    optional filter (e.g. Seaboard, RISE, GRAND)
//   format=  json (default) | ndjson  (ndjson = one record per line, streams)
//   limit=   max records (default 5000, capped at 100000)

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'GET') {
    grab_json(405, ['error' => 'GET only']);
}

$cfg   = grab_config();
$token = (string) ($cfg['exportToken'] ?? '');
if ($token === '') {
    grab_json(403, ['error' => 'export not configured']); // fail closed
}
$given = $_SERVER['HTTP_X_GRAB_TOKEN'] ?? ($_GET['token'] ?? '');
if (!is_string($given) || !hash_equals($token, $given)) {
    grab_json(401, ['error' => 'bad token']);
}

$tables = ['profiles' => 'grab_profiles', 'diagnostics' => 'grab_diagnostics', 'captures' => 'grab_captures'];
$kind   = isset($tables[$_GET['kind'] ?? '']) ? $_GET['kind'] : 'profiles';
$table  = $tables[$kind];
$family = isset($_GET['family']) ? grab_str($_GET['family'], 16) : '';
$type   = isset($_GET['type'])   ? grab_str($_GET['type'], 64)   : '';
$format = (($_GET['format'] ?? 'json') === 'ndjson') ? 'ndjson' : 'json';
$limit  = isset($_GET['limit']) ? max(1, min(100000, (int) $_GET['limit'])) : 5000;

$where = [];
$args  = [];
// Only filter on columns the table actually has: family (profiles only),
// type (profiles + diagnostics, not captures).
if ($family !== '' && $kind === 'profiles')  { $where[] = 'family = ?'; $args[] = $family; }
if ($type   !== '' && $kind !== 'captures')  { $where[] = 'type = ?';   $args[] = $type; }

$sql = 'SELECT id, received_at, raw_json FROM ' . $table // $table is from a fixed whitelist
     . ($where ? ' WHERE ' . implode(' AND ', $where) : '')
     . ' ORDER BY received_at DESC LIMIT ' . $limit; // $limit is an int, safe

/** Decode a stored row into {id, receivedAt, record}. */
function grab_export_row(array $row): array {
    $rec = json_decode((string) $row['raw_json'], true);
    return [
        'id'         => (int) $row['id'],
        'receivedAt' => $row['received_at'],
        'record'     => is_array($rec) ? $rec : null,
    ];
}

try {
    $pdo  = grab_db();
    $stmt = $pdo->prepare($sql);
    $stmt->execute($args);

    if ($format === 'ndjson') {
        header('Content-Type: application/x-ndjson');
        while ($row = $stmt->fetch()) {
            echo json_encode(grab_export_row($row)), "\n";
        }
        exit;
    }

    $out = [];
    while ($row = $stmt->fetch()) {
        $out[] = grab_export_row($row);
    }
    grab_json(200, ['kind' => $kind, 'count' => count($out), 'records' => $out]);
} catch (Throwable $e) {
    error_log('grab export.php: ' . $e->getMessage());
    grab_json(500, ['error' => 'server error']);
}
