<?php
declare(strict_types=1);

function grab_config(): array
{
    static $cfg = null;
    if ($cfg === null) {
        // Find the config: GRAB_CONFIG env (absolute path), else a local config.php,
        // else walk up the tree for a site /private/config.php (demix.me layout),
        // else the sample.
        $ret = null;
        $env = getenv('GRAB_CONFIG');
        if ($env && is_file($env)) {
            $ret = require $env;
        } elseif (is_file(__DIR__ . '/config.php')) {
            $ret = require __DIR__ . '/config.php';
        } else {
            $dir = __DIR__;
            while (true) {
                $cand = $dir . '/private/config.php';
                if (is_file($cand)) { $ret = require $cand; break; }
                $parent = dirname($dir);
                if ($parent === $dir) break; // reached filesystem root
                $dir = $parent;
            }
        }

        if (is_array($ret)) {
            // config file returned the array directly
            $cfg = $ret;
        } elseif (defined('DB_HOST') && defined('DB_USER') && defined('DB_PASS')) {
            // Your existing config.php defines DB_* constants — use them as-is.
            // Optionally define GRAB_DB_NAME / GRAB_WRITE_TOKEN to override.
            $cfg = [
                'host'        => DB_HOST,
                'dbname'      => defined('GRAB_DB_NAME') ? GRAB_DB_NAME : (defined('DB_NAME') ? DB_NAME : ''),
                'user'        => DB_USER,
                'pass'        => DB_PASS,
                'writeToken'  => defined('GRAB_WRITE_TOKEN')  ? GRAB_WRITE_TOKEN  : '',
                'exportToken' => defined('GRAB_EXPORT_TOKEN') ? GRAB_EXPORT_TOKEN : '',
            ];
        } else {
            $cfg = require __DIR__ . '/config.sample.php';
        }
    }
    return $cfg;
}

function grab_db(): PDO
{
    $c = grab_config();
    $dsn = "mysql:host={$c['host']};dbname={$c['dbname']};charset=utf8mb4";
    return new PDO($dsn, $c['user'], $c['pass'], [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES   => false,
    ]);
}

function grab_json(int $status, array $body): void
{
    http_response_code($status);
    header('Content-Type: application/json');
    echo json_encode($body);
    exit;
}

function grab_str($v, int $max): string
{
    return mb_substr(is_scalar($v) ? (string) $v : '', 0, $max);
}

/** Best-effort per-IP rate limit for the open write endpoints. Stores only a salted
 *  hash of the IP in a temp file (no raw IPs, nothing in the DB) with a count +
 *  window start; emits 429 over the limit. A speed bump against spam, not a guarantee. */
function grab_rate_limit(int $maxPerHour = 60): void
{
    $ip = $_SERVER['HTTP_CF_CONNECTING_IP']
        ?? (isset($_SERVER['HTTP_X_FORWARDED_FOR'])
            ? trim(explode(',', $_SERVER['HTTP_X_FORWARDED_FOR'])[0])
            : ($_SERVER['REMOTE_ADDR'] ?? ''));
    if ($ip === '') {
        return;
    }

    $dir = sys_get_temp_dir() . '/grab_rate';
    @mkdir($dir, 0700, true);
    $file = $dir . '/' . substr(hash('sha256', 'grab-rl|' . $ip), 0, 24);

    $now = time();
    $window = 3600;
    $n = 1;
    $start = $now;
    $fp = @fopen($file, 'c+');
    if ($fp !== false) {
        if (flock($fp, LOCK_EX)) {
            $cur = json_decode((string) stream_get_contents($fp), true);
            if (is_array($cur) && ($now - (int) ($cur['t'] ?? 0)) < $window) {
                $start = (int) $cur['t'];
                $n = (int) ($cur['n'] ?? 0) + 1;
            }
            ftruncate($fp, 0);
            rewind($fp);
            fwrite($fp, json_encode(['t' => $start, 'n' => $n]));
            fflush($fp);
            flock($fp, LOCK_UN);
        }
        fclose($fp);
    }
    if ($n > $maxPerHour) {
        header('Retry-After: ' . max(1, $window - ($now - $start)));
        grab_json(429, ['error' => 'rate limit exceeded, please try later']);
    }
}
