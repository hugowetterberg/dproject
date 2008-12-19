<?php
/**
 * Classes and constants used by dproject
 *
 * @version $Id$
 **/

define('DPROJECT_TMPDIR_ERROR', 100);
define('DPROJECT_RELEASE_FILE_READ_ERROR', 101);
define('DPROJECT_RELEASE_FILE_WRITE_ERROR', 102);
define('DPROJECT_CHECKSUM_ERROR', 200);
define('DPROJECT_DB_INTEGRITY_ERROR', 300);
define('DPROJECT_VALIDATION_ERROR', 400);

/**
 * A class for release management
 **/
class DProjectRelease {
  private $project_node;

  public $rid = 0;
  public $api_version = 6;
  public $major_version = 1;
  public $minor_version = 0;
  public $version_extra = '';
  public $status = 'supported'; // supported, recommended or unsupported

  private $tmp_file = FALSE;
  private $file_path = '';
  private $md5 = '';
  private $metadata;

  /**
   * Project release constructor
   *
   * @param stdClass $project_node
   *  The project node that this should be a release for
   **/
  public function DProjectRelease($project_node) {
    $this->metadata = new stdClass();
    $this->project_node = $project_node;
  }

  /**
   * Loads a project release from the database
   *
   * @param int $rid
   *  The unique id of the release
   * @param stdClass $project_node
   *  Optional node to use as project node, this must match the project_nid in the database
   *
   * @throws DProjectException
   *  Throws a DProjectException when the $project_node cannot be used because of a
   *  nid mismatch or when the project node cannot be loaded.
   *
   * @return DProjectRelease
   *  A project release object or FALSE if no release with the provided $rid exists
   **/
  public static function load($rid, $project_node=NULL) {
    $res = db_query("SELECT rid, project_nid, api_version, major_version, minor_version,
        version_extra, status, file_path, md5, release_metadata
      FROM {dproject_release}
      WHERE rid = %d", $rid);
    // Get the state as an array so that we can use DProjectRelease::__set_state to create the release object
    if ($state = db_fetch_array($res)) {
      // Decode the metadata and map it to the correct state key, unset the old value
      $state['metadata'] = json_decode($state['release_metadata']);
      unset($state['release_metadata']);

      // Check provided $project node so that it matches the project nid
      if (is_object($project_node) && $project_node->nid!=$state['project_nid']) {
        throw new DProjectException('The provided project node (nid: !nid) doesn\'t match the release\'s project node (nid: !pnid)',
          array('!nid' => $project_node->nid, '!pnid' => $state['project_nid']), DPROJECT_VALIDATION_ERROR, TRUE);
      }

      // Load the project node from the database if no $project_node was provided
      if(!is_object($project_node)) {
        $project_node = node_load($state['project_nid']);
        if (!$project_node) {
          throw new DProjectException('Could not load project node for the release with the id !id, might be a orphaned release',
            array('!rid' => $rid), DPROJECT_DB_INTEGRITY_ERROR, TRUE);
        }
      }

      // Unset the project_nid and set the project node in the state array
      unset($state['project_nid']);
      $state['project_node'] = $project_node;

      // Create a release object from the loaded state array
      return DProjectRelease::__set_state($state);
    }
    return FALSE;
  }

  /**
   * Creates a project release object from a associative array
   *
   * @param array $state
   *  The object properties in a associative array
   *
   * @return DProjectRelease
   *  A project release object
   **/
  public static function __set_state($state) {
    $o = new DProjectRelease(NULL);
    foreach ($state as $key => $value) {
      $o->$key = $value;
    }
    return $o;
  }

  /**
   * Sets the release file path so that it can be stored when the release is saved.
   *
   * @param string $file_path
   *  The path to the release file
   * @param string $md5 Optional.
   *  Supply the md5 hash as a 32-character hexadecimal number for file validation
   * @param bool $tmp_file
   *  If this is set to TRUE the file will be moved instead of copied when saved.
   *  If we don't have any write permissions for the $file_path this will always be
   *  FALSE. Defaults to FALSE.
   *
   * @throws DProjectException
   *  Throws a DProjectException when the source cannot be read from, the temporary
   *  file cannot be created or when the file doesn't match the md5 hash.
   *
   * @return void
   *  This function doesn't use return values for results. A DProjectException is thrown on failure.
   **/
  public function fileSet($file_path, $md5=NULL, $tmp_file=FALSE) {
    //Check release file md5 checksum
    $file_md5 = md5_file($file_path);
    if (!empty($md5) && $file_md5 != $md5) {
      throw new DProjectException('MD5 mismatch, the data in !file doesn\'t match the checksum !md5',
        array('!file' => $file, '!md5' => $md5), DPROJECT_CHECKSUM_ERROR, TRUE);
    }

    $this->tmp_file = $tmp_file && is_writable($file_path);
    $this->file_path = $file_path;
    $this->md5 = $file_md5;
  }

  /**
   * Loads a file from a (often external) source, and copies to a temporary file
   * so that it can be stored when the release is saved.
   *
   * All errors are logged to watchdog. The caller has the responsibility of catching any DProjectExceptions and
   * provide feedback to the user.
   *
   * @param string $source_file
   *  The path or url to the source file
   * @param string $md5
   *  Optional. Supply the md5 hash as a 32-character hexadecimal number for file validation
   *
   * @throws DProjectException
   *  Throws a DProjectException when the source cannot be read from, the temporary file cannot be created or
   *  when the file doesn't match the md5 hash.
   *
   * @return void This function doesn't use return values for results. A DProjectException is thrown on failure.
   **/
  public function fileLoad($source_file, $md5=NULL) {
    try {
      // Open the source release file for reading
      $sfp = fopen($source_file, 'r');
      if ($sfp) {
        $file_path = $this->tmpFile();

        // Open the temporary release file for writing
        $tfp = fopen($file_path, 'w');
        if ($tfp) {
          //Copy the release file to local temporary file
          while (!feof($sfp)) {
            $data = fread($sfp, 4096);
            fwrite($tfp, $data);
          }
          fclose($tfp);

          //Check release file md5 checksum
          $file_md5 = md5_file($file_path);
          if (!empty($md5) && $file_md5 != $md5) { // Abort if we have a md5 and it doesn't match
            throw new DProjectException('MD5 mismatch, the data read from !file doesn\'t match the checksum !md5',
              array('!file' => $file, '!md5' => $md5), DPROJECT_CHECKSUM_ERROR);
          }

          $this->tmp_file = TRUE;
          $this->file_path = $file_path;
          $this->md5 = $file_md5;
        }
        else { // Failed to create tmp file
          throw new DProjectException('Could not create temporary project release in !dir',
            array('!dir' => dirname($this->file_path)), DPROJECT_RELEASE_FILE_WRITE_ERROR);
        }

        fclose($sfp);
      }
      else { // Failed to open release file
        throw new DProjectException('Failed to open project release file !file',
          array('!file' => $file), DPROJECT_RELEASE_FILE_READ_ERROR);
      }
    }
    catch (DProjectException $e) {
      // Log the exception to watchdog
      $e.log();

      // Make sure that we close our file handles and remove temporary file
      if ($sfp) {
        fclose($sfp);
        if ($tfp) {
          fclose($tfp);
          unlink($file_path);
        }
      }

      // Rethrow the exception
      throw $e;
    }
  }

  private function save() {

  }

  /**
   * Helper function for getting a unique filename for a temporary release file
   **/
  private function tmpFile() {
    $base = file_directory_temp() . '/dproject';
    if (!file_exists($base)) {
      if (!mkdir($base)) {
        throw new DProjectException('Could not create temporary release directory !dir',
          array('!dir' => $base), DPROJECT_TMPDIR_ERROR);
      }
    }

    $filename = rand(0,10000000);
    while (file_exists($base . '/' . $filename)) {
      $filename = rand(0,10000000);
    }

    return $base . '/' . $filename;
  }

  /**
   * Returns the full version name as a string in the format 6.x-1.2[-dev]
   *
   * @return string
   *  A version string in the format 6.x-1.2[-dev]
   */
  public function version_string() {
    $vs = $this->api_version . '.x';
    $vs .= '-' . $this->major_version . '.' . (is_int($this->minor_version)?$this->minor_version:'x');
    if (!empty($this->version_extra)) {
      $vs .= '-' . $this->version_extra;
    }
    return $vs;
  }

  /**
   * Parses a version string and stores the results in the release object
   *
   * @param string $version
   *  A version string in the format 6.x-1.2[-dev]
   * @return bool
   *  Returns TRUE when the version string was successfully parsed,
   *  otherwise FALSE is returned.
   */
  public function version_from_string($version) {
    $matches = array();
    if (preg_match('/(\d+)\.x-(\d+)\.(\d+)(-([^\s]+))?/', $version, $matches)) {
      $this->api_version = $matches[1];
      $this->major_version = $matches[2];
      $this->minor_version = $matches[3];
      if (!empty($matches[5])) {
        $this->version_extra = $matches[5];
      }
      return TRUE;
    }
    return FALSE;
  }
}

/**
 * A exception that allows localization of the error message
 * and a function for logging the exception to watchdog
 **/
class DProjectException extends Exception {
  private $tmessage;
  private $vars;
  private $logged = FALSE;

  /**
   * Constructor for DProjectException
   *
   * @param string $message
   *  The message to store in the log. See t() for documentation
   *  on how $message and $variables interact. Keep $message
   *  translatable by not concatenating dynamic values into it!
   * @param $variables
   *  Array of variables to replace in the message, defaults to empty array.
   * @param $code
   *  User defined exception code, defaults to 0.
   * @param $log
   *  Set to TRUE to immediately log the exception to watchdog, defaults to FALSE.
   *  Immediate logging will always use the severity level of WATCHDOG_ERROR.
   **/
  public function DProjectException($message, $variables=array(), $code=0, $log=FALSE) {
    parent::__construct(t($message, $variables), $code);

    $this->tmessage = $message;
    $this->vars = $vars;
    if ($log) {
      $this->log();
    }
  }

  /**
   * Logs the exception to watchdog. This function call only has an effect the first time it's
   * called. Therefore you can safely call this function without worrying about double logging.
   *
   * @param $severity
   *  The severity of the message, as per RFC 3164
   * @param $link
   *  A link to associate with the message.
   *
   * @see watchdog()
   * @see watchdog_severity_levels()
   **/
  public function log($severity=WATCHDOG_ERROR, $link=NULL) {
    if (!$logged) {
      watchdog('dproject', $this->message, $this->vars, $severity, $link);
      $logged = TRUE;
    }
    return $this;
  }
}