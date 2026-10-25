<?php
declare(strict_types=1);
require __DIR__ . '/db.php';

// POST /grab/v1/profiles  — store one device profile. See DEVICE-PROFILES.md.

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    grab_json(405, ['error' => 'POST only']);
}

/** Best-effort per-IP rate limit. Stores only a salted hash of the IP in a temp
 *  file (no raw IPs, nothing in the DB) with a count + window start; returns 429
 *  over the limit. A speed bump against spam, not a hard guarantee. */
function grab_rate_limit(int $maxPerHour = 60): void {
    $ip = $_SERVER['HTTP_CF_CONNECTING_IP']
        ?? (isset($_SERVER['HTTP_X_FORWARDED_FOR'])
            ? trim(explode(',', $_SERVER['HTTP_X_FORWARDED_FOR'])[0])
            : ($_SERVER['REMOTE_ADDR'] ?? ''));
    if ($ip === '') return;

    $dir = sys_get_temp_dir() . '/grab_rate';
    @mkdir($dir, 0700, true);
    $file = $dir . '/' . substr(hash('sha256', 'grab-rl|' . $ip), 0, 24);

    $now = time();
    $window = 3600;
    $n = 1; $start = $now;
    $fp = @fopen($file, 'c+');
    if ($fp !== false) {
        if (flock($fp, LOCK_EX)) {
            $cur = json_decode((string) stream_get_contents($fp), true);
            if (is_array($cur) && ($now - (int) ($cur['t'] ?? 0)) < $window) {
                $start = (int) $cur['t'];
                $n = (int) ($cur['n'] ?? 0) + 1;
            }
            ftruncate($fp, 0); rewind($fp);
            fwrite($fp, json_encode(['t' => $start, 'n' => $n]));
            fflush($fp); flock($fp, LOCK_UN);
        }
        fclose($fp);
    }
    if ($n > $maxPerHour) {
        header('Retry-After: ' . max(1, $window - ($now - $start)));
        grab_json(429, ['error' => 'rate limit exceeded, please try later']);
    }
}

grab_rate_limit();

$cfg = grab_config();
if (!empty($cfg['writeToken'])) {
    $tok = $_SERVER['HTTP_X_GRAB_TOKEN'] ?? '';
    if (!hash_equals($cfg['writeToken'], $tok)) {
        grab_json(401, ['error' => 'bad token']);
    }
}

$raw = file_get_contents('php://input', false, null, 0, 2 * 1024 * 1024); // 2 MB cap
$p   = json_decode((string) $raw, true);
if (!is_array($p) || (int) ($p['schemaVersion'] ?? 0) !== 1) {
    grab_json(400, ['error' => 'bad or unsupported schema']);
}

$dev       = is_array($p['device'] ?? null) ? $p['device'] : [];
$family    = grab_str($dev['family'] ?? '', 16);
$type      = grab_str($dev['type'] ?? '', 64);
$firmware  = grab_str($dev['firmware'] ?? '', 32);
$transport = grab_str($dev['transport'] ?? '', 8);
$serial    = grab_str($dev['serialHash'] ?? '', 80);

if ($family === '' || $type === '' || $serial === '') {
    grab_json(400, ['error' => 'missing device fields (family, type, serialHash)']);
}

try {
    $pdo = grab_db();
    $pdo->beginTransaction();

    $ins = $pdo->prepare(
        'INSERT INTO grab_profiles
         (received_at, family, type, firmware, transport, serial_hash, tool_name, tool_version, notes, raw_json)
         VALUES (UTC_TIMESTAMP(), ?, ?, ?, ?, ?, ?, ?, ?, ?)'
    );
    $ins->execute([
        $family, $type, $firmware, $transport, $serial,
        grab_str($p['tool']['name'] ?? '', 32),
        grab_str($p['tool']['version'] ?? '', 16),
        grab_str($p['notes'] ?? '', 2000),
        $raw,
    ]);
    $pid = (int) $pdo->lastInsertId();

    $obs = $pdo->prepare(
        'INSERT INTO grab_config_observations
         (profile_id, family, type, config_id, name, value, min_val, max_val, ctype, grp, options)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)'
    );
    foreach ((array) ($p['configItems'] ?? []) as $it) {
        if (!is_array($it) || !isset($it['id'])) {
            continue;
        }
        $options = isset($it['options']) && is_array($it['options'])
            ? json_encode(array_map('strval', $it['options'])) : null;
        $obs->execute([
            $pid, $family, $type, (int) $it['id'],
            grab_str($it['name'] ?? '', 128),
            isset($it['value']) ? (int) $it['value'] : null,
            isset($it['min'])   ? (int) $it['min']   : null,
            isset($it['max'])   ? (int) $it['max']   : null,
            grab_str($it['type'] ?? '', 16),
            grab_str($it['group'] ?? '', 64),
            $options,
        ]);
    }

    $pdo->commit();
    grab_json(201, ['message' => 'Thank you', 'id' => $pid]);
} catch (Throwable $e) {
    if (isset($pdo) && $pdo->inTransaction()) {
        $pdo->rollBack();
    }
    // Don't leak internals to the client.
    error_log('grab profiles.php: ' . $e->getMessage());
    grab_json(500, ['error' => 'server error']);
}
