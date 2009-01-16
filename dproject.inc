<?php
/**
 * Classes and constants used by dproject
 *
 * @version $Id$
 **/

define('DPROJECT_NAMESPACE', 'http://goodold.se/project/dproject');

// Error codes
define('DPROJECT_TMPDIR_ERROR', 100);
define('DPROJECT_RELEASE_FILE_READ_ERROR', 101);
define('DPROJECT_RELEASE_FILE_WRITE_ERROR', 102);
define('DPROJECT_CHECKSUM_ERROR', 200);
define('DPROJECT_DB_INTEGRITY_ERROR', 300);
define('DPROJECT_VALIDATION_ERROR', 400);

// Release status codes
define('DPROJECT_RELEASE_STATUS_UNSUPPORTED', 0);
define('DPROJECT_RELEASE_STATUS_SUPPORTED', 1);
define('DPROJECT_RELEASE_STATUS_RECOMMENDED', 2); // Recommended releases are always supported

require('DProjectException.class.inc');
require('DProjectRelease.class.inc');

