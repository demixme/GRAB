<?php
declare(strict_types=1);

function grab_config(): array
{
    static $cfg = null;
    if ($cfg === null) {
        // Point GRAB at your existing config with the GRAB_CONFIG env var (absolute
        // path), else a local config.php, else the sample.
        $env  = getenv('GRAB_CONFIG');
        $path = ($env && is_file($env)) ? $env
              : (is_file(__DIR__ . '/config.php') ? __DIR__ . '/config.php'
              : __DIR__ . '/config.sample.php');

        $ret = require $path;

        if (is_array($ret)) {
            // config file returned the array directly
            $cfg = $ret;
        } elseif (defined('DB_HOST') && defined('DB_USER') && defined('DB_PASS')) {
            // Your existing config.php defines DB_* constants — use them as-is.
            // Optionally define GRAB_DB_NAME / GRAB_WRITE_TOKEN to override.
            $cfg = [
                'host'       => DB_HOST,
                'dbname'     => defined('GRAB_DB_NAME') ? GRAB_DB_NAME : (defined('DB_NAME') ? DB_NAME : ''),
                'user'       => DB_USER,
                'pass'       => DB_PASS,
                'writeToken' => defined('GRAB_WRITE_TOKEN') ? GRAB_WRITE_TOKEN : '',
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
