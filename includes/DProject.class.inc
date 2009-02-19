<?php

/**
 * Core class for DProject
 *
 * @package default
 */
class DProject {
  const API_VERSION = 1;
  const NAMESPACE = 'http://goodold.se/project/dproject';

  // Error codes
  const TMPDIR_ERROR = 100;
  const RELEASE_FILE_READ_ERROR = 101;
  const RELEASE_FILE_WRITE_ERROR = 102;
  const CHECKSUM_ERROR = 200;
  const DB_INTEGRITY_ERROR = 300;
  const VALIDATION_ERROR = 400;


  private static $instance = NULL;

  protected $modules = array();
  protected $packagers = array();

  /**
   * Gets the DProject class instance
   *
   * @return DProject
   *  A instance of the DProject utility class
   */
  public static function singleton() {
    if (!DProject::$instance) {
      DProject::$instance = new DProject();
    }
    return DProject::$instance;
  }

  /**
   * Constructor for the DProject class
   */
  function __construct() {
    // Execute all dproject_api-hooks and collect the results
    $modules = module_implements('dproject_api');
    foreach ($modules as $module) {
      $impl = call_user_func($module . '_dproject_api');
      if (isset($impl[DProject::API_VERSION])) {
        $this->registerApiImplementation($module, $impl[DProject::API_VERSION]);
      }
    }
  }

  /**
   * Registers a module and it's classes with the DProject
   * helper class.
   *
   * @param string $module
   *  The module that declared the implementation
   * @param array $impl
   *  The implementation details for the module
   * @return void
   */
  private function registerApiImplementation($module, $impl) {
    // Store module path
    $this->modules[$module]['path'] = $impl['path'];

    // Register the packagers
    foreach ($impl['packagers'] as $name => $info) {
      $this->packagers[$name] = $info;
      $this->classes[$info['class']] = (object)array(
        'module' => $module,
        'failed' => FALSE,
      );
    }
  }

  /**
   * Helper function for instantiating classes for DProject.
   *
   * @param string $class
   *  The name of the class that should be instantiated
   * @param array $arguments
   *  An array of arguments that should be passed to the constructor
   *
   * @return object
   *  Returns a instance of the requested class, if it could be
   *  loaded. Otherwise nothing is returned
   **/
  public function newInstanceOf($class, $arguments=array()) {
    if (class_exists($class)) {
      $ref = new ReflectionClass($class);
      if ($ref->isInstantiable()) {
        $i = $ref->newInstanceArgs($arguments);
        if ($ref->isInstance($i)) {
          return $i;
        }
      }
    }
  }

  /**
   * Gets information about the available packager types
   *
   * @param string $type_name
   *  Optional. Supply a name if you wan information about
   *  a single packager type.
   *
   * @return array
   *  An array of packager type definitions, or a single packager
   *  type definition if a name was supplied
   */
  public function getPackagerTypes($type_name=NULL) {
    if ($type_name) {
      return isset($this->packagers[$type_name]) ? $this->packagers[$type_name] : NULL;
    }
    return $this->packagers;
  }

  /**
   * Returns the release types as a associative array with
   * the machine-readable name as key and the human readable
   * name as value
   *
   * @param bool $localize
   *  Set to true if you want the human-readable name to
   *  be localized. As the old project module uses a human-
   *  readable value for release type terms this defaults
   *  to FALSE.
   *
   * @return array
   */
  public function releaseTypes($localize=FALSE) {
    if ($localize) {
      return array(
        'bug_fixes' => t('Bug fixes'),
        'features' => t('New features'),
        'security' => t('Security update'),
      );
    }
    else
    {
      return array(
        'bug_fixes' => 'Bug fixes',
        'features' => 'New features',
        'security' => 'Security update',
      );
    }
  }

  /**
   * Gets the latest n releases for the specified api version.
   *
   * @param stdClass|int $project
   *  The project node or the nid of the project node.
   * @param int $api_version
   * @param string $count
   *  Number of releases to get. Defaults to 0 (no limit).
   * @param string $offset
   *  The offset for the limiting count. Defaults to 0.
   * @return void
   */
  function getReleases($project, $api_version=0, $count=10, $offset=0) {
    if (is_object($project)) {
      $node = $project;
    }
    else {
      $node = node_load($project);
    }

    if ($node) {
      $releases = array();

      $params = array(
        ':nid' => $node->nid
      );

      if ($api_version) {
        $version_sql = 'AND api_version=%d';
        $params[':api_version'] = $api_version;
      }
      else {
        $version_sql = '';
      }

      $args = array("SELECT * FROM {dproject_release}
        WHERE project_nid=%d
        {$version_sql}
        ORDER BY api_version DESC, major_version DESC, minor_version DESC, version_extra DESC, created DESC",
        $params);

      if ($count) {
        $query_func = 'db_query_range';
        $args[] = $offset;
        $args[] = $count;
      }
      else {
        $query_func = 'db_query';
      }

      $res = call_user_func_array($query_func, $args);
      while ($rel = DProjectRelease::loadFromResult($res, $node)) {
        $releases[] = $rel;
      }
      return $releases;
    }

    return FALSE;
  }

  /**
   * Gets the project file directory
   *
   * @param int/stdClass $project
   *  The nid of the project or the project node
   * @return string
   *  The path to the project directory. No checks are made to determine
   *  if the directory exist, is writeable or can be created.
   */
  public function projectFileDirectory($project) {
    if (is_object($project)) {
      $project = $project->nid;
    }
    if ($project) {
      return file_directory_path() . '/dproject/' . $project;
    }
  }

  /**
   * Recursively and completely deletes a directory
   *
   * @param string $dir
   *  The path to the directory
   * @return void
   */
  public function deleteDirectory($dir) {
    if (is_dir($dir)) {
      if($dh = opendir($dir)) {
        // Loop through the contents of the directory
        while (FALSE !== ($obj = readdir($dh))) {
          $path = $dir . '/' . $obj;
          if($obj == '.' || $obj == '..') { // Skip non-content
            continue;
          }
          if (is_dir($path)) { // Recurse into directories
            $this->deleteDirectory($path);
          }
          else { // Delete files immediately
            unlink($path);
          }
        }
        closedir($dh);
      }
      rmdir($dir);
    }
    else if (file_exists($dir)) {
      // We really want to delete whatever is on this path
      // kill it even if it's a file
      unlink($dir);
    }
  }

  /**
   * Gets a project node by the short name
   *
   * @param string $short_name
   *  The short name of the project to get
   * @return stdClass
   *  The project node
   */
  public function getProjectByName($short_name) {
    $res = db_query("SELECT nid FROM {dproject} WHERE short_name = '%s'", $short_name);
    if ($o = db_fetch_object($res)) {
      return node_load($o->nid);
    }
  }

  /**
   * Gets the latest release for a project
   *
   * @param stdClass|int $project
   *  The project node or the nid of the project node
   * @return DProjectRelease
   *  The release object for the latest release
   */
  public function getLatestRelease($project) {
    if (is_object($project)) {
      $node = $project;
    }
    else {
      $node = node_load($project);
    }

    if ($node) {
      $res = db_query_range("SELECT * FROM {dproject_release}
        WHERE project_nid=%d
        ORDER BY api_version DESC, major_version DESC, minor_version DESC, version_extra DESC, created DESC", array(
          ':nid' => $node->nid,
        ), 0, 1);
      return DProjectRelease::loadFromResult($res, $node);
    }

    return FALSE;
  }

  /**
   * Gets the recommended release for a project
   *
   * @param stdClass|int $project
   *  The project node or the nid of the project node
   * @param int $api_version
   *  The api version to get the recommended release for
   * @return DProjectRelease
   *  The release object for the latest release
   */
  public function getRecommendedRelease($project, $api_version) {
    if (is_object($project)) {
      $node = $project;
    }
    else {
      $node = node_load($project);
    }

    if ($node) {
      $res = db_query("SELECT * FROM {dproject_release}
        WHERE project_nid=%d
        AND status=%d
        AND api_version=%d", array(
          ':nid' => $node->nid,
          ':status' => DProjectRelease::RECOMMENDED,
          ':api' => $api_version,
        ));
      return DProjectRelease::loadFromResult($res, $node);
    }

    return FALSE;
  }

  /**
   * Gets the major versions of supported releases for a project
   *
   * @param stdClass|int $project
   *  The project node or the nid of the project node
   * @param int $api_version
   *  The api version to get the recommended release for
   * @return array
   *  An array with the major versions
   */
  public function getSupportedMajorVersions($project, $api_version) {
    if (is_object($project)) {
      $node = $project;
    }
    else {
      $node = node_load($project);
    }

    $versions = array();

    if ($node) {
      $res = db_query("SELECT DISTINCT major_version FROM {dproject_release}
        WHERE project_nid=%d
        AND status>=%d
        AND api_version=%d", array(
          ':nid' => $node->nid,
          ':status' => DProjectRelease::SUPPORTED,
          ':api' => $api_version,
        ));

      while (($major=db_result($res))!==NULL) {
        $versions[] = $major;
      }
    }

    return $versions;
  }

  /**
   * Changes the status of a release.
   *
   * TODO: Look at options for creating a audit trail for dproject
   *
   * @param string $rid
   * @param string $status
   * @return void
   */
  public function changeReleaseStatus($rid, $status) {
    db_query("UPDATE {dproject_release} SET status=%d WHERE rid=%d", array(
      ':status' => $status,
      ':rid' => $rid,
    ));
  }

  /**
   * Helper function used to check if
   * a value can be treated as a int
   *
   * @param mixed $v
   *  A value that should be checked
   * @return bool
   *  Returns TRUE if the value can be treated as a int
   */
  public static function isInt($v) {
    if (is_numeric($v) && intval($v)==$v) {
      return TRUE;
    }
  }
}