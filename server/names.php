<?php
declare(strict_types=1);
require __DIR__ . '/db.php';

// GET /grab/v1/names?family=BLOCKS&type=Seaboard
// Returns the community-learned name/type/options map for a device, built from
// submitted profiles. The tool merges this into its local name overrides.

$family = grab_str($_GET['family'] ?? '', 16);
$type   = grab_str($_GET['type'] ?? '', 64);
if ($family === '' || $type === '') {
    grab_json(400, ['error' => 'family and type are required']);
}

try {
    $pdo = grab_db();
    // Per config_id, rank candidate names by how often they were observed, and
    // take the most common non-empty one (plus a representative type/options).
    $stmt = $pdo->prepare(
        'SELECT config_id, name, ctype, options, COUNT(*) AS c
         FROM grab_config_observations
         WHERE family = ? AND type = ? AND name IS NOT NULL AND name <> \'\'
         GROUP BY config_id, name, ctype, options
         ORDER BY config_id ASC, c DESC'
    );
    $stmt->execute([$family, $type]);

    $names = [];
    foreach ($stmt as $row) {
        $cid = (string) $row['config_id'];
        if (isset($names[$cid])) {
            continue; // first row per id = most observed
        }
        $entry = ['name' => $row['name']];
        if (!empty($row['ctype'])) {
            $entry['type'] = $row['ctype'];
        }
        if (!empty($row['options'])) {
            $opts = json_decode((string) $row['options'], true);
            if (is_array($opts)) {
                $entry['options'] = $opts;
            }
        }
        $names[$cid] = $entry;
    }

    grab_json(200, ['schemaVersion' => 1, 'names' => (object) $names]);
} catch (Throwable $e) {
    error_log('grab names.php: ' . $e->getMessage());
    grab_json(500, ['error' => 'server error']);
}
