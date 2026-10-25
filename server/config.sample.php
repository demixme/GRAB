<?php
// GRAB needs DB credentials as the array below. Two ways to provide them — either
// way, DON'T commit real credentials (config.php is gitignored; prefer keeping the
// secrets in your existing /private file, outside the web root).
//
// (A) Standalone: copy this file to config.php and fill in:
return [
    'host'       => 'localhost',
    'dbname'     => 'demixme_grab',   // or your existing DB (where you ran schema.sql)
    'user'       => 'your_db_user',
    'pass'       => 'CHANGE_ME',
    'writeToken' => '',               // optional X-Grab-Token to require on POSTs
];

// (B) Reuse your existing private credentials: make config.php pull from them so no
//     secrets live in the web root. Adjust the require path and the keys to match
//     YOUR /private config's shape, e.g.:
//
//   <?php
//   $shared = require '/home/demixme/private/config.php'; // your existing file
//   return [
//       'host'       => $shared['db_host'],   // <- rename to your actual keys
//       'dbname'     => $shared['db_name'],
//       'user'       => $shared['db_user'],
//       'pass'       => $shared['db_pass'],
//       'writeToken' => '',
//   ];
//
// Or set the GRAB_CONFIG env var to the absolute path of a file that returns the
// array above (e.g. SetEnv GRAB_CONFIG /home/demixme/private/grab-config.php).
