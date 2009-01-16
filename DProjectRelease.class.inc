<?php

/**
 * A class for release management
 **/
class DProjectRelease {
  private $project_node;

  private $rid = 0;
  private $api_version = 6;
  private $major_version = 1;
  private $minor_version = 0;
  private $version_extra = '';
  private $status = DPROJECT_RELEASE_STATUS_SUPPORTED;
  private $temporary = FALSE; // TODO: Implement auto-deletion of temporary releases
  private $published = FALSE;
  private $created = 0;

  private $file_changed = FALSE;
  private $tmp_file = FALSE;
  private $file_path = '';
  private $md5 = '';
  private $packager = 0;
  private $metadata;

  private $file_alters = array();

  /**
   * Project release constructor
   *
   * @param stdClass $project_node
   *  The project node that this should be a release for
   **/
  public function DProjectRelease($project_node) {
    $this->metadata = new stdClass();
    $this->project_node = $project_node;
    $this->add_file_alter('_dproject_append_packager_info');
  }

  /**
   * Replaces the node object with the nid so that
   * we don't serialize the whole node object
   *
   * @return void
   */
  public function __sleep() {
    $this->project_node = $this->project_node->nid;
  }

  /**
   * Restore the node object from the serialized nid
   *
   * @return void
   */
  public function __wakeup() {
    $this->project_node = node_load($this->project_node);
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
    $res = db_query("SELECT *
      FROM {dproject_release}
      WHERE rid = %d", $rid);
    return DProjectRelease::load_from_result($res, $project_node);
  }

  /**
   * Loads a project release from a result resource
   *
   * @param $res
   *  The result resource
   * @param stdClass $project_node
   *  Optional node to use as project node, this must match the project_nid in the database
   *
   * @throws DProjectException
   *  Throws a DProjectException when the $project_node cannot be used because of a
   *  nid mismatch or when the project node cannot be loaded.
   *
   * @return DProjectRelease
   *  A project release object or FALSE if nothing could be fetched from the result resource
   **/
  public static function load_from_result($res, $project_node=NULL) {
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
   * Uses the configured packager to set a release file.
   * Remember to save the release, otherwise the release
   * file won't be persisted.
   *
   * @return bool
   *  Returns TRUE on success, otherwise FALSE will be returned
   */
  public function package() {
    $packager = dproject_get_project_packager($this->packager);
    $packager_type = dproject_get_packager_types($packager->packager);

    $target = call_user_func_array($packager_type['package'], array($packager, $this));

    $zip = new ZipArchive();
    $wd = dproject_working_directory_path($packager, $this);
    $zip_file = dirname($wd) . '/' . $this->project_node->short_name . '-' . $this->version_string() . '.zip';
    if ($zip->open($zip_file, ZipArchive::OVERWRITE)!==TRUE) {
      // Add the project directory to the zip archive
      $zip->addEmptyDir($this->project_node->short_name);
      // Recursively add files to the zip archive, excluding all files matching the exclude patterns
      $this->add_to_release_zip_archive($zip, $this->project_node->short_name, $wd, $this->add_file_exclude());
      $zip->close();

      $this->set_file($zip_file, NULL, TRUE);

      drupal_set_message(t('A !version release of @project has been created', array(
        '!version' => $this->version_string(),
        '@project' => $this->project_node->title,
      )));
      return TRUE;
    }
    else {
      dproject_set_packager_message($packager->pid, 'dproject', 'zip-archive-create', 'Failed to create release zip file !file', array(
        '!file' => $zip_file,
      ));
      return FALSE;
    }
  }

  /**
   * Recursively adds files to a release zip archive
   *
   * @param DProjectRelease $release
   *  The release object we're adding files for
   * @param ZipArchive $zip
   *  A zip archive object
   * @param string $zip_dir
   *  The zip directory to add the files to
   * @param string $dir
   *  The directory to get files and subdirectories from
   * @param string $exclude
   *  An array with regex patterns of filenames to exclude
   * @return void
   */
  private function add_to_release_zip_archive($zip, $zip_dir, $dir, &$exclude=array()) {
    $dp = opendir($dir);
    while ($file = readdir($dp)) {
      if ($file != '.' && $file != '..' && (empty($exclude) || !$this->preg_matches_any($file, $exclude))) {
        if (is_dir($dir . '/' . $file)) { // Recurse into subdirectories
          $zip->addEmptyDir($zip_dir . '/' . $file);
          $this->add_to_release_zip_archive($zip, $zip_dir . '/' . $file, $dir . '/' . $file, $exclude);
        }
        else { // Add files
          $content = $this->file_alter($dir . '/' . $file);
          $zip->addFromString($zip_dir . '/' . $file, $content);
        }
      }
    }
  }

  /**
   * Checks if a string matches any of the regex patterns
   *
   * @param string $string
   *  The string to test
   * @param string $patterns
   *  The patterns to test against
   * @return void
   */
  public function preg_matches_any($string, $patterns) {
    foreach($patterns as $pattern) {
      if (preg_match($pattern, $string)) {
        return TRUE;
      }
    }
    return FALSE;
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
  public function set_file($file_path, $md5=NULL, $tmp_file=FALSE) {
    //Check release file md5 checksum
    $file_md5 = md5_file($file_path);
    if (!empty($md5) && $file_md5 != $md5) {
      throw new DProjectException('MD5 mismatch, the data in !file doesn\'t match the checksum !md5',
        array('!file' => $file, '!md5' => $md5), DPROJECT_CHECKSUM_ERROR, TRUE);
    }

    $this->file_changed = TRUE;
    $this->tmp_file = $tmp_file;
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
  public function load_file($source_file, $md5=NULL) {
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

          $this->file_changed = TRUE;
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

  /**
   * Persist file that has been set through fileSet or fileLoad
   *
   * @return void
   */
  private function persist_file() {
    if ($this->file_changed) {
      $pi = pathinfo($this->file_path);

      // Create the path for the release file and delete it if it exists
      $release_path = dproject_project_file_directory($this->project_node) .
        '/releases/' . $pi['basename'];
      if (!file_exists(dirname($release_path))) {
        mkdir(dirname($release_path), 0777, TRUE);
      }
      else if (file_exists($release_path)) {
        unlink($release_path);
      }

      // Move or copy the file to it's destination
      if ($this->tmp_file) {
        rename($this->file_path, $release_path);
      }
      else {
        copy($this->file_path, $release_path);
      }

      // Set all file related values
      $this->tmp_file = FALSE;
      $this->file_changed = FALSE;
      $this->file_path = $release_path;
    }
  }

  /**
   * Gets the path of the release file
   *
   * @return void
   */
  public function get_file_path() {
    return $this->file_path;
  }
  
  /**
   * Gets the created timestamp of the release
   *
   * @return void
   */
  public function get_created() {
    return $this->created;
  }
  
  /**
   * Gets the md5 hash of the release file
   *
   * @return void
   */
  public function get_md5() {
    return $this->md5;
  }

  /**
   * Deletes the release and it's file
   *
   * @return void
   */
  public function delete() {
    // Delete the release file if it has been set and if it hasn't been
    // changed since load/creation or is a temporary file
    if (!empty($this->file_path) && ($this->tmp_file || !$this->file_changed)) {
      unlink($this->file_path);
    }

    // Delete the database entry if this release has a id
    if ($this->rid) {
      db_query('DELETE FROM {dproject_release} WHERE rid=%d', array(
        ':rid' => $this->rid,
      ));
    }
  }

  /**
   * Saves the release to db and persists any release file that
   * has been set
   *
   * @return void
   */
  public function save() {
    $this->persist_file();

    $values = array(
      'project_nid' => $this->project_node->nid,
      'api_version' => $this->api_version,
      'major_version' => $this->major_version,
      'minor_version' => $this->minor_version,
      'version_extra' => $this->version_extra,
      'created' => $this->created,
      'packager' => $this->packager,
      'status' => $this->status,
      'published' => $this->published,
      'temporary' => $this->temporary,
      'file_path' => $this->file_path,
      'md5' => $this->md5,
      'release_metadata' => json_encode($this->metadata),
    );

    // Update if we got a release id
    if ($this->rid) {
      $values['rid'] = $this->rid;
      drupal_write_record('dproject_release', $values, array('rid'));
    }
    else { // ...insert if we don't
      $values['created'] = time();
      drupal_write_record('dproject_release', $values);
      $this->rid = db_last_insert_id('dproject_release', 'rid');
    }
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
    $vs .= '-' . $this->major_version . '.' . ($this->minor_version!=NULL?$this->minor_version:'x');
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
    if (preg_match('/^(\d+)\.x-(\d+)\.(x|[\d]+)-?([_a-z0-9]+)?$/', $version, $matches)) {
      if (!empty($matches[4])) {
        $extra = $matches[4];
      }
      else {
        $extra = NULL;
      }

      $this->set_version($matches[1], $matches[2], $matches[3], $extra);
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Sets all version information for the release
   *
   * @param int|array $api
   *  The drupal api version or an array containing all the version information.
   *  If an array is passed all the following paramaters can be ignored.
   * @param int $major
   *  The major version of the release
   * @param int $minor
   *  The minor version of the release
   * @param string $extra
   *  The extra version information (dev, beta, and so on)
   * @return void
   */
  public function set_version($api, $major=NULL, $minor=NULL, $extra=NULL) {
    // Unpack version information if we got it as an array
    if (is_array($api)) {
      list($api, $major, $minor, $extra) = $api;
    }

    $this->api_version = $api;
    $this->major_version = $major;
    $this->minor_version = $extra!='dev' ? $minor : NULL;
    $this->version_extra = $extra;
  }

  /**
   * Gets version information as an array containing the
   * api, major, minor and extra version information.
   *
   * @return array
   */
  public function get_version() {
    return array(
      $this->api_version,
      $this->major_version,
      $this->minor_version,
      $this->version_extra,
    );
  }

  /**
   * Sets the information needed for the
   * packager to create a release
   *
   * @param stdClass $packager
   *  The packager object
   * @param array $configuration
   *  The packager configuration data
   */
  public function set_packager_info($packager, $configuration) {
    $this->packager = $packager->pid;
    $this->metadata->packager = $configuration;
  }

  /**
   * Gets the id of this release
   *
   * @return int
   *  The id of this release
   */
  public function get_id() {
    return $this->rid;
  }

  /**
   * Gets the project node for this release
   *
   * @return stdClass
   *  The project node for this release
   */
  public function get_project_node() {
    return $this->project_node;
  }

  /**
   * Sets the packager for this release
   *
   * @param int $pid
   *  The packager used for this release
   * @return void
   */
  public function set_packager($pid) {
    $this->packager = $pid;
  }

  /**
   * Gets the packager for this release
   *
   * @return int
   *  The packager used for this release
   */
  public function get_packager() {
    return $this->packager;
  }

  /**
   * Sets the status of this release. Use one of
   * the defined status constants DPROJECT_RELEASE_STATUS_*
   *
   * @param int $status
   *  The status of the release
   * @return void
   */
  public function set_status($status) {
    $this->status = $status;
  }

  /**
   * Gets the status of this release
   *
   * @return int
   *  The status of this release
   */
  public function get_status() {
    return $this->status;
  }

  /**
   * Sets the published state of this release
   *
   * @param bool $published
   *  The published state of the release
   * @return void
   */
  public function set_published($published) {
    $this->published = $published;
  }

  /**
   * Gets the published status of this release.
   *
   * @return bool
   *  The published status of this release
   */
  public function get_published() {
    return $this->published;
  }

  /**
   * Sets the temporary state of this release
   *
   * @param bool $published
   *  The published state of the release
   * @return void
   */
  public function set_temporary($temporary) {
    $this->temporary = $temporary;
  }

  /**
   * Gets the temporary state of this release
   *
   * @return bool
   *  Returns TRUE if this release is temporary, otherwise FALSE is returned
   */
  public function get_temporary() {
    return $this->temporary;
  }

  /**
   * Private helper function to get the name to use
   * for the namespace uri.
   *
   * @param string $uri
   *  The namespace uri
   * @param string $name
   *  Optional. Set this if you want a name that you can use to
   *  create a new metadata entry, omit if you only want to get
   *  the name of a existing metadata namespace.
   * @return string
   *  The name that is or should be used
   */
  private function metadata_namespace_name($uri, $name=FALSE) {
    $existing = get_object_vars($this->metadata);

    // Get the name from existing entry if the
    // uri already has metadata set
    foreach ($existing as $a_name => $obj) {
      if ($obj->namespace == $uri) {
        return $a_name;
      }
    }

    if ($name!==FALSE) {
      $candidate = $name;
      $i = 0;

      while (isset($existing[$candidate]) && $existing[$candidate]->namespace!=$uri) {
        $i++;
        $candidate = $name . '-' . $i;
      }
      return $candidate;
    }

    return FALSE;
  }

  /**
   * Gets the metadata object for a namespace
   *
   * @param string $name
   *  The name that the caller would like as a alias for
   *  the namespace. Will be altered if needed to ensure
   *  the uniqueness of namespace aliases.
   * @param string $namespace
   *  The namespace to get the data for
   * @return stdClass|bool metadata
   *  A reference to the stored metadata or FALSE if no metadata was stored
   *  for the namespace.
   */
  public function get_metadata_object($name = 'dproject', $namespace = DPROJECT_NAMESPACE) {
    $name = $this->metadata_namespace_name($namespace, $name);
    if ($name!==FALSE) {
      if (!isset($this->metadata->$name)) {
        $this->metadata->$name = (object)array(
          'data' => new stdClass(),
          'namespace' => $namespace,
        );
      }
      $obj = $this->metadata->$name;
      return $obj->data;
    }
    return FALSE;
  }
  
  /**
   * Returns all metadata as a list
   *
   * @return array
   */
  public function get_metadata_list() {
    $metadata = get_object_vars($this->metadata);
    $list = array();
    foreach ($metadata as $name => $cont) {
      $list[] = (object)array(
        'name' => $name,
        'namespace' => $cont->namespace,
        'data' => $cont->data,
      );
    }
    return $list;
  }

  /**
   * Used to add to and get the file alter callbacks
   *
   * @param string $callback
   *  Optional. Omit if you just want to get a list of the alter callbacks
   * @return array
   *  Returns the registered file alters
   */
  public function add_file_alter($callback=NULL) {
    $metadata = $this->get_metadata_object();
    if (!isset($metadata->file_alters)) {
      $metadata->file_alters = array();
    }
    if ($callback) {
      foreach ((array)$callback as $cb) {
        $metadata->file_alters[] = $cb;
      }
    }
    return $metadata->file_alters;
  }

  /**
   * Used to add to and get the file exclude patterns
   *
   * @param string $exclude
   *  Optional. Omit if you just want a list of the exclude patterns
   * @return array
   *  Returns the registered file excludes
   */
  public function add_file_exclude($exclude=NULL) {
    $metadata = $this->get_metadata_object();
    if (!isset($metadata->file_excludes)) {
      $metadata->file_excludes = array();
    }
    if ($exclude) {
      foreach ((array)$exclude as $ex) {
        $metadata->file_excludes[] = $ex;
      }
    }
    return $metadata->file_excludes;
  }

  /**
   * Alters the file before adding it to a release archive
   *
   * @param string $file
   *  The path of the file
   * @return string
   *  The final contents of the file
   */
  public function file_alter($file) {
    $pi = pathinfo($file);
    $content = file_get_contents($file);
    $metadata = $this->get_metadata_object();

    if (!empty($metadata->file_alters)) {
      foreach ($metadata->file_alters as $callback) {
        call_user_func_array($callback, array(&$this, $pi, &$content));
      }
    }

    return $content;
  }
}