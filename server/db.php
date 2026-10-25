<?php
declare(strict_types=1);

function grab_config(): array
{
    static $cfg = null;
    if ($cfg === null) {
        // Priority: GRAB_CONFIG env (absolute path, e.g. your /private/… file that
        // returns the array), then local config.php, then the sample.
        $env  = getenv('GRAB_CONFIG');
        $path = ($env && is_file($env)) ? $env
              : (is_file(__DIR__ . '/config.php') ? __DIR__ . '/config.php'
              : __DIR__ . '/config.sample.php');
        $cfg = require $path;
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
