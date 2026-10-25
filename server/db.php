<?php
declare(strict_types=1);

function grab_config(): array
{
    static $cfg = null;
    if ($cfg === null) {
        $path = __DIR__ . '/config.php';
        $cfg = is_file($path) ? require $path : require __DIR__ . '/config.sample.php';
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
