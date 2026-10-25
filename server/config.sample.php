<?php
// Copy to config.php and fill in. config.php is gitignored (don't commit creds).
return [
    'host'   => 'localhost',
    'dbname' => 'demix_grab',
    'user'   => 'demix_grab',
    'pass'   => 'CHANGE_ME',
    // Optional: require this token in an "X-Grab-Token" header to accept POSTs.
    // Leave '' to accept anonymous submissions (fine to start).
    'writeToken' => '',
];
