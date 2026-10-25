<?php
// GRAB needs DB credentials as the array below. Two ways to provide them — either
// way, DON'T commit real credentials (config.php is gitignored; prefer keeping the
// secrets in your existing /private file, outside the web root).
//
// (A) Standalone: copy this file to config.php and fill in:
return [
    'host'       => 'localhost',
    'dbname'     => 'grab',           // or your existing DB (where you ran schema.sql)
    'user'       => 'your_db_user',
    'pass'       => 'CHANGE_ME',
    'writeToken' => '',               // optional X-Grab-Token to require on POSTs
    'exportToken' => '',              // set a long random secret to enable GET /export
];

// (B) Reuse your existing private credentials so no secrets live in the web root.
//     If your private config defines constants (DB_HOST, DB_NAME, DB_USER, DB_PASS),
//     make config.php require it and map them:
//
//   <?php
//   require_once '/absolute/path/to/private/config.php'; // defines DB_* constants
//   return [
//       'host'       => DB_HOST,
//       'dbname'     => DB_NAME,   // the GRAB tables can live in this database
//       'user'        => DB_USER,
//       'pass'        => DB_PASS,
//       'writeToken'  => '',
//       'exportToken' => 'a-long-random-secret', // enables GET /export
//   ];
//
//   (Or, if your private config defines constants, add
//    define('GRAB_EXPORT_TOKEN', 'a-long-random-secret'); and db.php picks it up.)
//
//   If instead it returns an array, map that array's keys the same way.
//
// Or set the GRAB_CONFIG env var to the absolute path of a file that returns the
// array above (e.g. SetEnv GRAB_CONFIG /home/.../private/grab-config.php).
